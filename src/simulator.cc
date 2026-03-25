#include "assert.h"
#include "node.h"
#include "simulator.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>

class Event {
public:
    NodeID const receiver;
    double const time;

    Event(NodeID r, double t) : receiver(r), time(t) { }
    virtual ~Event() = default;
    virtual void process(Simulator& simulator) = 0;
};

class DeliveryEvent : public Event {
public:
    std::shared_ptr<Sendable> object;
    NodeID logical_sender;
    NodeID final_recipient;

    DeliveryEvent(NodeID logical_sender, std::shared_ptr<Sendable> object, NodeID current_receiver, NodeID final_recipient, double t)
        : Event(current_receiver, t), object(object), logical_sender(logical_sender), final_recipient(final_recipient)
    {
    }

    void process(Simulator& simulator) override
    {
        if (receiver == final_recipient) {
            object->receive(simulator.get_node(final_recipient), logical_sender);
            return;
        }

        simulator.forward_in_flight_sendable(logical_sender, object, receiver, final_recipient);
    }
};
std::shared_ptr<Event> create_synthetic_delivery_event(std::shared_ptr<Sendable> object, NodeID receiver, double time)
{
    return std::make_shared<DeliveryEvent>(NO_NODE_ID, object, receiver, receiver, time);
}

class TimedEvent : public Event {
public:
    std::shared_ptr<Timer> trigger;
    TimedEvent(NodeID receiver, std::shared_ptr<Timer> trigger, double time)
        : Event(receiver, time), trigger(trigger)
    {
    }
    void process(Simulator& simulator) override
    {
        trigger->trigger(simulator.get_node(receiver));
    }
};
std::shared_ptr<Event> create_synthetic_timed_event(std::shared_ptr<Timer> timer, NodeID receiver, double time)
{
    return std::make_shared<TimedEvent>(receiver, timer, time);
}

bool EventComparator::operator()(std::shared_ptr<Event> e1, std::shared_ptr<Event> e2)
{
    return e1->time > e2->time;
}

void Simulator::recompute_routes()
{
    next_hops.clear();

    for (auto const& [source, _] : adj_list) {
        /*
         * Dijkstra from source using propagation_delay_ms as edge weight.
         * Finds latency-shortest paths and records the first hop toward
         * every destination.
         */
        std::map<NodeID, double> dist;
        std::map<NodeID, NodeID> first_hop;
        std::map<NodeID, int> hops;
        std::set<NodeID> settled;

        using PQEntry = std::pair<double, NodeID>;
        std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;

        dist[source] = 0.0;
        first_hop[source] = source;
        hops[source] = 0;
        pq.push({ 0.0, source });

        while (!pq.empty()) {
            auto [d, current] = pq.top();
            pq.pop();

            if (settled.count(current) > 0)
                continue;
            settled.insert(current);

            for (auto neighbor : adj_list.at(current)) {
                if (settled.count(neighbor) > 0)
                    continue;

                double edge_weight = link_properties.at(current).at(neighbor).propagation_delay_ms;
                double new_dist = d + edge_weight;

                if (dist.count(neighbor) == 0 || new_dist < dist[neighbor]) {
                    dist[neighbor] = new_dist;
                    first_hop[neighbor] = (current == source) ? neighbor : first_hop[current];
                    hops[neighbor] = hops[current] + 1;
                    pq.push({ new_dist, neighbor });
                }
            }
        }

        for (auto const& [node, hop] : first_hop)
            next_hops[source][node] = hop;
        for (auto const& [node, d] : dist)
            path_distances[source][node] = d;
        for (auto const& [node, h] : hops)
            path_hop_counts[source][node] = h;
    }
}

void Simulator::schedule_delivery_hop(NodeID logical_sender, std::shared_ptr<Sendable> sendable, NodeID current_hop, NodeID next_hop, NodeID final_recipient, double send_time)
{
    auto const& properties = link_properties.at(current_hop).at(next_hop);

    // Per-hop queuing delay: models the inv/getdata protocol handshake,
    // app-level processing, and store-and-forward overhead at each relay.
    // Exp(mean=100ms) — covers inv/getdata RTT and processing overhead;
    // Decker & Wattenhofer (2013) measured ~80ms per-hop overhead in Bitcoin.
    std::exponential_distribution<double> exp_dist(1.0 / 100.0); // mean 100ms
    double queuing_delay = exp_dist(rng);

    // Upload channel queuing: sender's uplink is shared across P channels
    double upload_start = acquire_upload_slot(current_hop, send_time + queuing_delay, sendable->size_kb());

    // The receiver gets the last byte when the slower of (upload channel, link) finishes.
    // With P channels the per-channel speed is typically the bottleneck.
    double speed = node_upload_speed.count(current_hop) > 0 ? node_upload_speed.at(current_hop) : 12.5;
    double per_channel_speed = speed / static_cast<double>(upload_parallelism);
    double effective_speed = std::min(per_channel_speed, properties.link_speed_kb_per_ms);
    double transmission_delay = sendable->size_kb() / effective_speed;
    double receive_time = upload_start + properties.propagation_delay_ms + transmission_delay;

    event_queue.push(std::make_shared<DeliveryEvent>(logical_sender, sendable, next_hop, final_recipient, receive_time));
}

void Simulator::deliver_sendable(NodeID sender, std::shared_ptr<Sendable> sendable, NodeID receiver)
{
    ++total_messages_sent;
    total_traffic_kb += sendable->size_kb();

    if (sender == receiver) {
        event_queue.push(std::make_shared<DeliveryEvent>(sender, sendable, receiver, receiver, current_time));
        return;
    }

    schedule_delivery_hop(sender, sendable, sender, get_next_hop(sender, receiver), receiver, current_time);
}

void Simulator::forward_in_flight_sendable(NodeID logical_sender, std::shared_ptr<Sendable> sendable, NodeID current_hop, NodeID final_recipient)
{
    schedule_delivery_hop(logical_sender, sendable, current_hop, get_next_hop(current_hop, final_recipient), final_recipient, current_time);
}

void Simulator::set_node_upload_speed(NodeID node, double speed_kb_per_ms)
{
    node_upload_speed[node] = speed_kb_per_ms;
}

void Simulator::set_upload_parallelism(size_t P)
{
    upload_parallelism = P;
}

double Simulator::acquire_upload_slot(NodeID node, double send_time, double size_kb)
{
    auto& slots = node_upload_slots[node];
    if (slots.size() < upload_parallelism)
        slots.resize(upload_parallelism, 0.0);

    // Find the slot with the earliest free time
    auto min_it = std::min_element(slots.begin(), slots.end());
    double slot_free = *min_it;
    double start = std::max(send_time, slot_free);

    double speed = node_upload_speed.count(node) > 0 ? node_upload_speed.at(node) : 12.5;
    double per_channel_speed = speed / static_cast<double>(upload_parallelism);
    double transmission_delay = size_kb / per_channel_speed;

    *min_it = start + transmission_delay;
    return start;
}

void Simulator::deliver_sendable_direct(NodeID sender, std::shared_ptr<Sendable> sendable, NodeID receiver)
{
    ++total_messages_sent;
    total_traffic_kb += sendable->size_kb();

    if (sender == receiver) {
        event_queue.push(std::make_shared<DeliveryEvent>(sender, sendable, receiver, receiver, current_time));
        return;
    }

    double propagation = path_distances.at(sender).at(receiver);

    // Upload channel queuing: sender's uplink is shared across P channels
    double upload_start = acquire_upload_slot(sender, current_time, sendable->size_kb());
    double speed = node_upload_speed.count(sender) > 0 ? node_upload_speed.at(sender) : 12.5;
    double per_channel_speed = speed / static_cast<double>(upload_parallelism);
    double transmission_delay = sendable->size_kb() / per_channel_speed;

    // Per-hop queuing delay: KADcast pushes chunks directly — no inv/getdata
    // handshake. Intermediate overlay hops only do IP/TCP-level forwarding.
    // Exp(mean=20ms) — covers OS network stack, buffer bloat, TCP overhead
    // at loaded overlay transit nodes.
    int intermediate_hops = std::max(0, path_hop_counts.at(sender).at(receiver) - 1);
    double queuing_delay = 0.0;
    if (intermediate_hops > 0) {
        std::exponential_distribution<double> exp_dist(1.0 / 20.0); // mean 20ms
        for (int i = 0; i < intermediate_hops; ++i)
            queuing_delay += exp_dist(rng);
    }

    double receive_time = upload_start + propagation + queuing_delay + transmission_delay;

    event_queue.push(std::make_shared<DeliveryEvent>(sender, sendable, receiver, receiver, receive_time));
}

void Simulator::register_timer(NodeID sender, std::shared_ptr<Timer> trigger, double time_delay)
{
    assert(time_delay >= 0.0);
    std::shared_ptr<Event> e = std::make_shared<TimedEvent>(sender, trigger, current_time + time_delay);
    event_queue.push(e);
}

void Simulator::add_nodes(std::set<std::shared_ptr<Node>> new_nodes)
{
    for (auto const& node : new_nodes) {
        assert(nodes.count(node->node_id) == 0);
        nodes[node->node_id] = node;
        adj_list[node->node_id] = {};
    }
}

void Simulator::add_links(std::map<NodeID, std::map<NodeID, LinkProperties>> additional_links)
{
    for (auto const& [node_id, delay_map] : additional_links) {
        auto it = adj_list.find(node_id);
        assert(it != adj_list.end());
        auto& adjset = it->second;

        for (auto const& [neighbor_id, delay] : delay_map) {
            adjset.insert(neighbor_id);
            link_properties[node_id][neighbor_id] = delay;
        }
    }

    recompute_routes();
}

void Simulator::log(NodeID requester, std::string log)
{
    logging << "[" << std::setprecision(3) << std::setw(13) << std::fixed << current_time << "ms] " << std::setw(3) << requester << ": " << log << '\n';
}

void Simulator::init(double time)
{
    event_queue = std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventComparator> {};
    current_time = time;
    node_upload_slots.clear();
    total_messages_sent = 0;
    total_traffic_kb = 0.0;
}

void Simulator::inject_events(std::vector<std::shared_ptr<Event>> events)
{
    for (auto e : events)
        event_queue.push(e);
}

void Simulator::run(double duration)
{
    double end_time = current_time + duration;

    std::shared_ptr<Event> e;
    while (!event_queue.empty()) {
        e = event_queue.top();
        event_queue.pop();
        assert(e->time >= current_time);
        if (e->time > end_time) {
            event_queue.push(e);
            break;
        }
        current_time = e->time;
        e->process(*this);
    }
}