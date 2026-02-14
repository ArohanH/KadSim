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
    /*
     * typedef for link delay lambda
     * separate one for each link
     */
    using LinkDelay = std::function<double(std::mt19937&, size_t)>;

private:
    std::ostream& logging;

    std::map<NodeID, std::shared_ptr<Node>> nodes;
    std::map<NodeID, std::set<NodeID>> adj_list;
    std::map<NodeID, std::map<NodeID, LinkDelay>> link_delays;

    /*
     * event queue
     */
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventComparator> event_queue;
    double current_time;

public:
    std::mt19937& rng;

    Simulator(std::mt19937& rng, std::ostream& logging) : logging(logging), rng(rng) { }
    void add_nodes(std::set<std::shared_ptr<Node>> nodes);
    void add_links(std::map<NodeID, std::map<NodeID, LinkDelay>> links);

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
    double get_time() const
    {
        return current_time;
    }
    void log(NodeID requester, std::string log);

    void deliver_sendable(NodeID sender, std::shared_ptr<Sendable> sendable, NodeID recipient);
    void register_timer(NodeID sender, std::shared_ptr<Timer> trigger, double time_delay);

    /*
     * functions for managing the simulation
     */
    void init(double time);
    void inject_events(std::vector<std::shared_ptr<Event>> events);
    void run(double duration);
};

#endif // SIMULATOR_H
