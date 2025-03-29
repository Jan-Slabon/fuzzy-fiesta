#include <Dispatcher.hpp>
#include <Queues.hpp>

class Scheduler
{
    public:
    template<typename... Processes>
    Scheduler(Processes... proc)
    {
        for(int i = 0; i < core_num; i++)
        {
            dispatchers.push_back(std::make_unique<Dispatcher>(make_proc(proc)...));
        }
        for(int i = 0; i < core_num; i++)
        {
            dispatchers[i]->spawn(i);
        }
    }

    void run()
    {
        while (!queues.is_empty())
        {
            Event event = queues.get(queue_counter);
            dispatchers[dispatcher_count]->receive(event, queue_counter);
            //printf("Event send to dispatcher = %d\n", dispatcher_count);
            dispatcher_count = (dispatcher_count + 1) % core_num;
            queue_counter = (queue_counter + 1) % queue_count;
        }
        //printf("All events handled\n");
        for(auto& dispatcher : dispatchers)
        {
            dispatcher->abort();
            dispatcher->join();
        }
    }
    private:
    std::vector<std::unique_ptr<Dispatcher>> dispatchers;
    queue_id queue_counter{0};
    uint8_t dispatcher_count{0};
};