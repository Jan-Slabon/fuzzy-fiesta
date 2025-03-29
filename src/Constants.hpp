#ifndef CONSTANTS
#define CONSTANTS
#include <stddef.h>

constexpr size_t queue_count = 2; // Amount of queues used
constexpr size_t core_num = 4; // Amount of cores avaliable for application deployment
constexpr size_t message_size = 126; // max size of message payload
constexpr size_t real_core_count = 8; // amount of cores 

static_assert(real_core_count > core_num);

#endif // CONSTANTS