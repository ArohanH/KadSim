#include "assert.h"
#include "node.h"
#include "simulator.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>

class Event {
public:
    NodeID const node_id; 
    double const time;
    Event(NodeID node_id, double time) : node_id(node_id), time(time) { }
    virtual ~Event() = default;
    virtual void process(std::shared_ptr<Node> n) = 0;
}

class InterNodeEvent : public Event {
public:
    NodeID const receiver;
    double const time;

    InterNodeEvent(NodeID r, double t) : receiver(r), time(t) { }
    virtual ~InterNodeEvent() = default;
    virtual void process(std::shared_ptr<Node> n) = 0;
};

class DeliveryEvent : public InterNodeEvent {
public:
    std::shared_ptr<Sendable> object;
    NodeID from;
    DeliveryEvent(NodeID from, std::shared_ptr<Sendable> object, NodeID receiver, double t)
        : InterNodeEvent(receiver, t), object(object), from(from)
    {
    }
    void process(std::shared_ptr<Node> n) override
    {
        object->receive(n, from);
    }
};
std::shared_ptr<InterNodeEvent> create_synthetic_delivery_event(std::shared_ptr<Sendable> object, NodeID receiver, double time)
{
    return std::make_shared<DeliveryEvent>(NO_NODE_ID, object, receiver, time);
}

class TimedEvent : public InterNodeEvent {
public:
    std::shared_ptr<Timer> trigger;
    TimedEvent(NodeID receiver, std::shared_ptr<Timer> trigger, double time)
        : InterNodeEvent(receiver, time), trigger(trigger)
    {
    }
    void process(std::shared_ptr<Node> n) override
    {
        trigger->trigger(n);
    }
};
std::shared_ptr<InterNodeEvent> create_synthetic_timed_event(std::shared_ptr<Timer> timer, NodeID receiver, double time)
{
    return std::make_shared<TimedEvent>(receiver, timer, time);
}



bool EventComparator::operator()(std::shared_ptr<InterNodeEvent> e1, std::shared_ptr<InterNodeEvent> e2)
{
    return e1->time > e2->time;
}

void Simulator::deliver_sendable(NodeID sender, std::shared_ptr<Sendable> sendable, NodeID receiver)
{
    auto const& link_delay = link_delays[sender][receiver];
    double receive_time = current_time + link_delay(rng, sendable->size_kb());

    std::shared_ptr<InterNodeEvent> e = std::make_shared<DeliveryEvent>(sender, sendable, receiver, receive_time);
    event_queue.push(e);
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

void Simulator::add_links(std::map<NodeID, std::map<NodeID, LinkDelay>> additional_links)
{
    for (auto const& [node_id, delay_map] : additional_links) {
        auto it = adj_list.find(node_id);
        assert(it != adj_list.end());
        auto& adjset = it->second;

        for (auto const& [neighbor_id, delay] : delay_map) {
            adjset.insert(neighbor_id);
            link_delays[node_id][neighbor_id] = delay;
        }
    }
}

void Simulator::log(NodeID requester, std::string log)
{
    logging << "[" << std::setprecision(3) << std::setw(13) << std::fixed << current_time << "ms] " << std::setw(3) << requester << ": " << log << '\n';
}

void Simulator::init(double time)
{
    event_queue = std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventComparator> {};
    current_time = time;
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
        e->process(nodes.at(e->receiver));
    }
}
