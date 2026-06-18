#pragma once

#include <cstdint>

class PriceLevel;

enum class Side {
    BUY,
    SELL
};

class Order {
public:

    uint64_t id;

    Side side;

    int price;
    int remainingQuantity;

    uint64_t timestamp;

    // part of intrusive linked list -> in orderbook at each price level there is a linked list so for saving the cache we can have pointers here
    Order* prev = nullptr;
    Order* next = nullptr;

    PriceLevel* priceLevel = nullptr;
};