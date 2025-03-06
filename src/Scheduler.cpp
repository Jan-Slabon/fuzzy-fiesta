#include <vector>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <cstring>
#include <condition_variable>
#include <memory>

constexpr size_t buffer_size = 1024; // No message can be larger than buffer_size bytes
constexpr size_t queue_count = 2; // Amount of queues used
constexpr size_t core_num = 4; // Amount of cores avaliable for application deployment
constexpr size_t message_size = buffer_size - 4; // buffer_size - sizeof(TAG)

using queue_id = uint16_t;

enum class TAG
{
    GET = 0,
    SEND = 1,
    NONE = 3
};

class Event
{
    public:
    Event() : event_type{TAG::NONE}{}
    Event(const Event& message) : event_type{message.event_type}
    {
        std::memcpy(payload, message.payload, message_size);
    }
    Event(TAG t, char* payload_source, size_t payload_size) : event_type{t}
    {
        std::memcpy(payload, payload_source, payload_size);
    }
    TAG event_type;
    char payload[message_size];
};

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
    void emplace(queue_id id, Event event)
    {
        auto& synchro_context = queues_locks[id];
        std::unique_lock<std::mutex> queue_lock(synchro_context.first);
        synchro_context.second.wait(queue_lock, [&]{return true;});
        event_queues[id].emplace_back(event);

        queue_lock.unlock();
        synchro_context.second.notify_all();
    }
    Event get(queue_id id)
    {
        auto& synchro_context = queues_locks[id];
        std::unique_lock<std::mutex> queue_lock(synchro_context.first);
        synchro_context.second.wait(queue_lock, [&]{return !this->is_empty(id);});

        auto first_elem = event_queues[id][0];
        event_queues[id].pop_front();

        queue_lock.unlock();
        synchro_context.second.notify_all();
        return first_elem;
    }
    bool is_empty(queue_id id)
    {
        return event_queues[id].size() == 0;
    }
    private:
    std::vector<std::deque<Event>> event_queues;
    std::array< std::pair<std::mutex, std::condition_variable>, queue_count> queues_locks{};
};
Queues queues{};

template <typename MESSAGE>
MESSAGE* unwrap(Event& event)
{
    return reinterpret_cast<MESSAGE*>(&(event.payload));
}
class Get_Asset
{
    public:
    Get_Asset(std::string str) : asset_name{str}{}
    std::string asset_name;
};
class Send_Asset
{
    public:
    Send_Asset(std::string dest, std::string _data) : destination{dest}, data{_data}{}
    std::string destination;
    std::string data;
};

class Process
{
    public: 
    virtual void operator()(Event) = 0;
    virtual ~Process(){}
};

class Dispatcher
{
    public:
    template<typename... T>
    Dispatcher(T... Processes)
    {
        ((void) Scheduled_Processes.push_back(std::make_unique<T>(std::forward<T>(Processes))), ...);
    }

    void run()
    {
        char copied_buffer[buffer_size];
        Event tmp_msg;
        while(true)
        {
            std::unique_lock<std::mutex> buffer_lock{buffer_guard};
            buffer_sync.wait(buffer_lock, [&](){return message_in_the_buffer;});
            printf("Event in the buffer taken\n");

            tmp_msg = buffered_msg;
            message_in_the_buffer = false;

            buffer_lock.unlock();
            buffer_sync.notify_all();

            printf("Sending Event to aproperiate Process = %d\n", proc_index);
            Scheduled_Processes[proc_index]->operator()(tmp_msg);
        }
    }
    void spawn()
    {
        std::thread(&Dispatcher::run, this).detach();
    }

    void receive(const Event& event, unsigned int proc_id)
    {
        std::unique_lock<std::mutex> buffer_lock{buffer_guard};
        buffer_sync.wait(buffer_lock, [&](){return !message_in_the_buffer;});

        message_in_the_buffer = true;

        proc_index = proc_id;
        buffered_msg = event;

        buffer_lock.unlock();
        buffer_sync.notify_all();
    }

    private:
    std::vector<std::unique_ptr<Process>> Scheduled_Processes;
    Event buffered_msg{};
    bool message_in_the_buffer{false};
    unsigned int proc_index;
    std::condition_variable buffer_sync{};
    std::mutex buffer_guard{};
};

template<typename Proc>
Proc make_proc(Proc /*unused*/)
{
    return Proc();
}

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
            dispatchers[i]->spawn();
        }
    }

    void run()
    {
        while (true)
        {
            if(!queues.is_empty(queue_counter))
            {
                Event event = queues.get(queue_counter);
                dispatchers[dispatcher_count]->receive(event, queue_counter);
                printf("Event send to dispatcher = %d\n", dispatcher_count);
                dispatcher_count = (dispatcher_count + 1) % core_num;
            }
            queue_counter = (queue_counter + 1) % queue_count;
        }
        
    }
    private:
    std::vector<std::unique_ptr<Dispatcher>> dispatchers;
    queue_id queue_counter{0};
    uint8_t dispatcher_count{0};
};


class Getter : public Process
{
    public:
    void operator() (Event event) override
    {
        printf("Getter Process starts working\n");
        switch(event.event_type)
        {
            case TAG::GET:
            {
                Get_Asset* msg_body = unwrap<Get_Asset>(event);
                printf("Processing Get_Req in Getter\n");
                break;
            }
            default:
                printf("Wrong message sent to Getter_Eo\n");
                break;
        }
    }
};

class Sender : public Process
{
    public:
    void operator() (Event event) override
    {
        printf("Sender Process starts working\n");
        switch(event.event_type)
        {
            case TAG::SEND:
            {
                Send_Asset* msg_body = unwrap<Send_Asset>(event);
                printf("Processing Send_Req in Sender\n");
                break;
            }
            default:
                printf("Wrong message sent to Sender_Eo\n");
                break;
        }
    }
};

int main()
{
    Scheduler sch(Getter{}, Sender{});
    std::thread sched_td(&Scheduler::run, std::move(sch));
    for(int i = 0; i < 100; i++)
    {
        if(i%2 == 0)
        {
            auto msg_payload = Get_Asset("Some Asset");
            auto event = Event(TAG::GET, (char*)&msg_payload, sizeof(msg_payload));
            queues.emplace(0, event);
        }
        else
        {
            auto msg_payload = Send_Asset("Some Destination", "Some Asset");
            auto event = Event(TAG::SEND, (char*)&msg_payload, sizeof(msg_payload));
            queues.emplace(1, event);
        }
    }
    printf("Scheduler started succesfully\n");
    sched_td.join();
    return 0;
}