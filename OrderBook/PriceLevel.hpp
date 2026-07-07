#pragma once

#include "Order.hpp"

class PriceLevel {
public:
    static int64_t activeAllocations;
    void* operator new(size_t size) {
        activeAllocations++;
        return ::operator new(size);
    }
    void operator delete(void* ptr) noexcept {
        if (ptr) {
            activeAllocations--;
            ::operator delete(ptr);
        }
    }

    int price;

    int currQuantity = 0;

    Order* head = nullptr;
    Order* tail = nullptr;

    PriceLevel(int p)
        : price(p)
    {
    }

    ~PriceLevel()
    {
        Order* curr = head;
        while(curr != nullptr)
        {
            Order* next = curr->next;
            delete curr;
            curr = next;
        }
    }
};