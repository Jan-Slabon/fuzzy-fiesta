#include <memory>
#include <cstring>
#include <Event.hpp>

class Get_Asset;
class Send_Asset;

Event::Event() : event_type{TAG::NONE}{}
Event::Event(const Event&& message) : event_type{message.event_type}, payload{message.payload}{}
Event& Event::operator=(const Event&& e)
{
    event_type = e.event_type;
    payload = e.payload;
    return *this;
}
Event::Event(TAG t, char* payload_source, size_t payload_size) : event_type{t}
{
    //Schould be replaced with custom allocator that uses prealocated memory and not syscalls
    payload = static_cast<char*>(malloc(payload_size));

    std::memcpy(payload, payload_source, payload_size);
}


template <typename MESSAGE>
MESSAGE* unwrap(Event& event)
{
    return reinterpret_cast<MESSAGE*>(event.payload);
}


template Get_Asset* unwrap<Get_Asset>(Event&);
template Send_Asset* unwrap<Send_Asset>(Event&);