#include <Event.hpp>
#include <mutex>
#include <thread>
#include <sys/syscall.h>
#include <vector>
#include <Constants.hpp>
#include <Process.hpp>
#include <condition_variable>
#include <unistd.h>
class Dispatcher
{
    public:
    template<typename... T>
    Dispatcher(T... Processes)
    {
        ((void) Scheduled_Processes.push_back(std::make_unique<T>(std::forward<T>(Processes))), ...);
    }
    
    void set_scheduling_properties(uint16_t cpu_id);
    void run(uint16_t cpu_id);
    void spawn(uint16_t cpu_id);
    void join();
    void receive(const Event& event, unsigned int proc_id);

    // use only for benchmark reasons
    void abort();
    private:
    bool will_abort();
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