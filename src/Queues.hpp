#include <array>
#include <vector>
#include <mutex>
#include <thread>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <Event.hpp>
#include <Constants.hpp>

using queue_id = uint16_t;

class Queues
{
    public:
    Queues()
    {
       for(int i = 0; i < queue_count; i++)
       {
            event_queues.push_back({});
       }
    }
    void emplace(queue_id id, Event& event)
    {
        auto& synchro_context = queues_locks[id];
        std::unique_lock<std::mutex> queue_lock(synchro_context.first);
        synchro_context.second.wait(queue_lock, [&]{return true;});
        event_queues[id].emplace_back(std::move(event));

        queue_lock.unlock();
        synchro_context.second.notify_all();
    }
    Event get(queue_id id)
    {
        auto& synchro_context = queues_locks[id];
        std::unique_lock<std::mutex> queue_lock(synchro_context.first);
        synchro_context.second.wait(queue_lock, [&]{return !this->is_empty(id);});

        auto first_elem = std::move(event_queues[id][0]);
        event_queues[id].pop_front();

        queue_lock.unlock();
        synchro_context.second.notify_all();
        return first_elem;
    }
    bool is_empty(queue_id id)
    {
        return event_queues[id].size() == 0;
    }
    bool is_empty()
    {
        bool empty = true;
        for(int i=0; i<queue_count; i++)
        {
            empty &= is_empty(i);
        }
        return empty;
    }
    private:
    std::vector<std::deque<Event>> event_queues;
    std::array<std::pair<std::mutex, std::condition_variable>, queue_count> queues_locks{};
};
Queues queues{};