#include <vector>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <cstring>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>

constexpr size_t buffer_size = 125; // Amount of messages stored in dispatcher buffer
constexpr size_t queue_count = 2; // Amount of queues used
constexpr size_t core_num = 4; // Amount of cores avaliable for application deployment
constexpr size_t message_size = 126; // max size of message payload
constexpr size_t real_core_count = 8; // amount of cores 

static_assert(real_core_count > core_num);

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
    virtual void operator()(Event&) = 0;
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
    
    void set_scheduling_properties(uint16_t cpu_id)
    {
        pthread_t my_thread_native = thread->native_handle();
        cpu_set_t cpuset_1;
        CPU_ZERO(&cpuset_1);
        CPU_SET(real_core_count - cpu_id - 1, &cpuset_1);
        auto s1 = pthread_setaffinity_np(my_thread_native, sizeof(cpu_set_t), &cpuset_1);
        if (s1 != 0) printf("Error during setting cpu affinity for core %u\n", cpu_id);
        pid_t tid = syscall(SYS_gettid);
        sched_param sched_prio;
        sched_prio.sched_priority = 99;
        sched_setscheduler(tid, SCHED_FIFO, &sched_prio);
    }
    void run(uint16_t cpu_id)
    {
        set_scheduling_properties(cpu_id);
        Event saved_msg;
        unsigned int saved_proc_id;
        while(true)
        {
            std::unique_lock<std::mutex> buffer_lock{buffer_guard};
            buffer_sync.wait(buffer_lock, [&](){return message_in_buffer || should_abort;});
            if(should_abort) break;

            saved_msg = buffered_message;
            saved_proc_id = stored_proc_id;
            buffer_lock.unlock();
            buffer_sync.notify_all();

            //printf("Sending Event to aproperiate Process = %d\n", ctx.second);
            Scheduled_Processes[saved_proc_id]->operator()(buffered_message);
        }
        //printf("Exiting Dispatcher\n");
    }
    void spawn(uint16_t cpu_id)
    {
        thread = std::make_unique<std::thread>(&Dispatcher::run, this, cpu_id);
    }
    void join()
    {
        thread->join();
    }
    void abort()
    {
        std::unique_lock<std::mutex> abort_lock{abort_guard};
        should_abort = true;
        buffer_sync.notify_all();
    }
    void receive(const Event& event, unsigned int proc_id)
    {
        std::unique_lock<std::mutex> buffer_lock{buffer_guard};
        buffer_sync.wait(buffer_lock, [&](){return !message_in_buffer;});

        stored_proc_id = proc_id;
        buffered_message = std::move(event);

        buffer_lock.unlock();
        buffer_sync.notify_all();
    }

    private:
    bool will_abort()
    {
        std::unique_lock<std::mutex> abort_lock{abort_guard};
        return should_abort;
    }
    std::vector<std::unique_ptr<Process>> Scheduled_Processes;
    Event buffered_message;
    bool message_in_buffer{false};
    unsigned int stored_proc_id;
    std::condition_variable buffer_sync{};
    std::mutex buffer_guard{};
    std::unique_ptr<std::thread> thread;
    std::mutex abort_guard;
    bool should_abort{false};
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


class Getter : public Process
{
    public:
    void operator() (Event& event) override
    {
        //printf("Getter Process starts working\n");
        switch(event.event_type)
        {
            case TAG::GET:
            {
                Get_Asset* msg_body = unwrap<Get_Asset>(event);
                usleep(10);
                //printf("Processing Get_Req in Getter\n");
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
    void operator() (Event& event) override
    {
        //printf("Sender Process starts working\n");
        switch(event.event_type)
        {
            case TAG::SEND:
            {
                Send_Asset* msg_body = unwrap<Send_Asset>(event);
                usleep(10);
                //printf("Processing Send_Req in Sender\n");
                break;
            }
            default:
                printf("Wrong message sent to Sender_Eo\n");
                break;
        }
    }
};
void handle_sequential()
{
    int queue_counter = 0;
    Getter get_process{};
    Sender send_process{};
    while(!queues.is_empty())
    {
        auto msg = queues.get(queue_counter);
        if(queue_counter == 0)
        {
            get_process(msg);
        }
        else
        {
            send_process(msg);
        }

        queue_counter = (queue_counter + 1) % queue_count;
    }
}
int main()
{
    for(int i = 0; i < 1000000; i++)
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
    printf("Queues loaded\n");
    Scheduler sch(Getter{}, Sender{});
    std::thread sched_td(&Scheduler::run, std::move(sch));
    pthread_t my_thread_native = sched_td.native_handle();
    cpu_set_t cpuset_1;
    CPU_ZERO(&cpuset_1);
    CPU_SET(0, &cpuset_1);
    auto s1 = pthread_setaffinity_np(my_thread_native, sizeof(cpu_set_t), &cpuset_1);
    if (s1 != 0) printf("Error during setting Scheduler cpu affinity\n");

    auto start = std::chrono::high_resolution_clock::now();
    printf("Scheduler started succesfully\n");
    sched_td.join();
    auto stop = std::chrono::high_resolution_clock::now();
    printf("Elapsed time = %lu microseconds for async\n", std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    for(int i = 0; i < 1000000; i++)
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
    printf("Queues loaded\n");
    start = std::chrono::high_resolution_clock::now();
    handle_sequential();
    stop = std::chrono::high_resolution_clock::now();
    printf("Elapsed time = %lu microseconds for sequential\n", std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    
    auto msg_payload = Send_Asset("Some Destination", "Some Asset");
    auto event = Event(TAG::SEND, (char*)&msg_payload, sizeof(msg_payload));

    start = std::chrono::high_resolution_clock::now();
    Sender()(event);
    stop = std::chrono::high_resolution_clock::now();
    printf("Elapsed time = %lu microseconds for single job\n", std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    return 0;
}