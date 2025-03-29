#include <Dispatcher.hpp>

void Dispatcher::set_scheduling_properties(uint16_t cpu_id)
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
    int result = sched_setscheduler(tid, SCHED_FIFO, &sched_prio);
    if(result != 0) printf("Error %d changing sched properties on core %u\n",errno, cpu_id);
}

void Dispatcher::run(uint16_t cpu_id)
{
    set_scheduling_properties(cpu_id);
    Event saved_msg;
    unsigned int saved_proc_id;
    while(true)
    {
        std::unique_lock<std::mutex> buffer_lock{buffer_guard};
        buffer_sync.wait(buffer_lock, [&](){return message_in_buffer || should_abort;});
        if(should_abort) break;

        saved_msg = std::move(buffered_message);
        saved_proc_id = stored_proc_id;
        buffer_lock.unlock();
        buffer_sync.notify_all();

        //printf("Sending Event to aproperiate Process = %d\n", ctx.second);
        Scheduled_Processes[saved_proc_id]->operator()(saved_msg);
    }
    //printf("Exiting Dispatcher\n");
}
void Dispatcher::spawn(uint16_t cpu_id)
{
    // should be changed to jthread in c++20 to make it easier to stop the thread for benchmark purposes
    thread = std::make_unique<std::thread>(&Dispatcher::run, this, cpu_id);
}
void Dispatcher::join()
{
    thread->join();
}
void Dispatcher::abort()
{
    std::unique_lock<std::mutex> abort_lock{abort_guard};
    should_abort = true;
    buffer_sync.notify_all();
}
void Dispatcher::receive(const Event& event, unsigned int proc_id)
{
    std::unique_lock<std::mutex> buffer_lock{buffer_guard};
    buffer_sync.wait(buffer_lock, [&](){return !message_in_buffer;});

    stored_proc_id = proc_id;
    buffered_message = std::move(event);

    buffer_lock.unlock();
    buffer_sync.notify_all();
}
bool Dispatcher::will_abort()
{
    std::unique_lock<std::mutex> abort_lock{abort_guard};
    return should_abort;
}