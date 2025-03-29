#include <Scheduler.hpp>
#include <Processes/Getter.hpp>
#include <Processes/Sender.hpp>



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
            Event event(TAG::GET, (char*)&msg_payload, sizeof(msg_payload));
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