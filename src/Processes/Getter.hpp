#include <Process.hpp>
#include <Messages/Get_Asset.hpp>

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
                //usleep(10);
                //printf("Processing Get_Req in Getter\n");
                break;
            }
            default:
                printf("Wrong message sent to Getter_Eo\n");
                break;
        }
    }
};