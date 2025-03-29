#ifndef EVENT
#define EVENT

#include <memory>
#include <cstring>
enum class TAG
{
    GET = 0,
    SEND = 1,
    NONE = 3
};

class Event
{
    public:
    Event();
    Event(const Event&& message);
    Event& operator=(const Event&& e);
    Event(TAG t, char* payload_source, size_t payload_size);
    TAG event_type;
    char* payload;
};

template <typename MESSAGE>
MESSAGE* unwrap(Event& event);

#endif // EVENT