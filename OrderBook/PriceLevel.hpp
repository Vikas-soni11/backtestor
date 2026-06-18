#pragma once

#include "Order.hpp"

class PriceLevel {
public:

    int price;

    int currQuantity = 0;

    Order* head = nullptr;
    Order* tail = nullptr;

    PriceLevel(int p)
        : price(p)
    {
    }
};