#include <Process.hpp>
#include <Messages/Send_Asset.hpp>
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
                //usleep(10);
                //printf("Processing Send_Req in Sender\n");
                break;
            }
            default:
                printf("Wrong message sent to Sender_Eo\n");
                break;
        }
    }
};