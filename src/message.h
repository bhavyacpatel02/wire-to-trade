#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>

struct Message {
    uint32_t seq_num;
    uint32_t quantity;
    uint32_t price;
};

#endif
