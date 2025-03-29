#ifndef PROC
#define PROC
#include <Event.hpp>
class Process
{
    public: 
    virtual void operator()(Event&) = 0;
    virtual ~Process(){}
};

template<typename Proc>
Proc make_proc(Proc /*unused*/)
{
    return Proc();
}

#endif // PROC