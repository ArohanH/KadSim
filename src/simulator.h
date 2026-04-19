#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "id.h"

#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <set>

class Node;

/*
 * abstract class to be extended by any class
 * whose objects can be sent between nodes
 */
class Sendable : public std::enable_shared_from_this<Sendable> {
public:
    virtual size_t size_kb() const = 0;
    virtual void receive(std::shared_ptr<Node> n, NodeID from) = 0;
    virtual ~Sendable() = default;
};

/*
 * abstract class to be extended by any class
 * whose objects should be given in timed interval
 */
class Timer : public std::enable_shared_from_this<Timer> {
public:
    virtual void trigger(std::shared_ptr<Node> n) = 0;
    virtual ~Timer() = default;
};

class Event;
/*
 * functions to create synthetic events for injection
 */
std::shared_ptr<Event> create_synthetic_delivery_event(std::shared_ptr<Sendable> object, NodeID receiver, double time);
std::shared_ptr<Event> create_synthetic_timed_event(std::shared_ptr<Timer> timer, NodeID receiver, double time);

struct EventComparator {
    bool operator()(std::shared_ptr<Event> e1, std::shared_ptr<Event> e2);
};

/*
 * simulator class
 */
class Simulator {
public:
    struct LinkProperties {
        double propagation_delay_ms;
        double link_speed_kb_per_ms;
    };

private:
    std::ostream& logging;

    std::map<NodeID, std::shared_ptr<Node>> nodes;
    std::map<NodeID, std::set<NodeID>> adj_list;
    std::map<NodeID, std::map<NodeID, LinkProperties>> link_properties;
    std::map<NodeID, std::map<NodeID, NodeID>> next_hops;
    std::map<NodeID, std::map<NodeID, double>> path_distances;
    std::map<NodeID, std::map<NodeID, int>> path_hop_counts;

    /*
     * per-node upload bandwidth with P parallel channels.
     * Total bandwidth = node_upload_speed; each channel gets speed/P.
     * Sends are assigned to the channel with the earliest free time.
     */
    size_t upload_parallelism = 8;
    std::map<NodeID, double> node_upload_speed;
    std::map<NodeID, std::vector<double>> node_upload_slots;

    double acquire_upload_slot(NodeID node, double send_time, double size_kb);

    /*
     * event queue
     */
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventComparator> event_queue;
    double current_time;

    /*
     * message statistics counters
     */
    size_t total_messages_sent;
    double total_traffic_kb;

    /*
     * upload contention tracking
     */
    std::vector<double> upload_wait_times;
    std::map<NodeID, std::vector<double>> per_node_upload_waits;

    void recompute_routes();
    void schedule_delivery_hop(NodeID logical_sender, std::shared_ptr<Sendable> sendable, NodeID current_hop, NodeID next_hop, NodeID final_recipient, double send_time);

public:
    std::mt19937& rng;

    Simulator(std::mt19937& rng, std::ostream& logging) : logging(logging), rng(rng) { }
    void add_nodes(std::set<std::shared_ptr<Node>> nodes);
    void add_links(std::map<NodeID, std::map<NodeID, LinkProperties>> links);

    /*
     * util functions that can be called by nodes
     */
    NodeID get_random_peer(NodeID requester) const
    {
        std::uniform_int_distribution<> i(0, nodes.size() - 1);
        while (true) {
            auto it = nodes.begin();
            std::advance(it, i(rng));
            if (it->first != requester)
                return it->first;
        }
    }
    std::set<NodeID> const& get_neighbors(NodeID requester) const
    {
        return adj_list.at(requester);
    }
    std::shared_ptr<Node> get_node(NodeID node_id) const
    {
        return nodes.at(node_id);
    }
    std::set<NodeID> get_node_ids() const
    {
        std::set<NodeID> node_ids;
        for (auto const& [node_id, _] : nodes)
            node_ids.insert(node_id);
        return node_ids;
    }
    double get_time() const
    {
        return current_time;
    }
    void log(NodeID requester, std::string log);

    NodeID get_next_hop(NodeID current_hop, NodeID final_recipient) const
    {
        return next_hops.at(current_hop).at(final_recipient);
    }
    void forward_in_flight_sendable(NodeID logical_sender, std::shared_ptr<Sendable> sendable, NodeID current_hop, NodeID final_recipient);
    void deliver_sendable(NodeID sender, std::shared_ptr<Sendable> sendable, NodeID recipient);
    void deliver_sendable_direct(NodeID sender, std::shared_ptr<Sendable> sendable, NodeID recipient);
    void set_node_upload_speed(NodeID node, double speed_kb_per_ms);
    void set_upload_parallelism(size_t P);
    void register_timer(NodeID sender, std::shared_ptr<Timer> trigger, double time_delay);

    size_t get_total_messages_sent() const { return total_messages_sent; }
    double get_total_traffic_kb() const { return total_traffic_kb; }
    std::vector<double> const& get_upload_wait_times() const { return upload_wait_times; }

    double get_node_recent_upload_wait(NodeID node, size_t window = 20) const
    {
        auto it = per_node_upload_waits.find(node);
        if (it == per_node_upload_waits.end() || it->second.empty())
            return 0.0;
        auto const& waits = it->second;
        size_t start = waits.size() > window ? waits.size() - window : 0;
        double sum = 0.0;
        for (size_t i = start; i < waits.size(); ++i)
            sum += waits[i];
        return sum / (waits.size() - start);
    }

    double get_path_distance(NodeID from, NodeID to) const
    {
        auto it = path_distances.find(from);
        if (it == path_distances.end())
            return 1e9;
        auto jt = it->second.find(to);
        if (jt == it->second.end())
            return 1e9;
        return jt->second;
    }

    /*
     * functions for managing the simulation
     */
    void init(double time);
    void inject_events(std::vector<std::shared_ptr<Event>> events);
    void run(double duration);
};

#endif // SIMULATOR_H
