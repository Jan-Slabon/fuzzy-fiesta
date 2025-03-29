#include <string>

class Send_Asset
{
    public:
    Send_Asset(std::string dest, std::string _data) : destination{dest}, data{_data}{}
    std::string destination;
    std::string data;
};