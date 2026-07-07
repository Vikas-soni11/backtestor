#pragma once

#include <cstdint>

struct Trade {
    uint64_t tradeId;
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    int price;
    int quantity;
    uint64_t timestamp;
};
