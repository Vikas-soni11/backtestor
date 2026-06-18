#pragma once

#include <bits/stdc++.h>

#include "Order.hpp"
#include "PriceLevel.hpp"

class OrderBook {

private:

    std::map<int, PriceLevel*, std::greater<int>> bid;
    std::map<int, PriceLevel*> ask;

    // map which connect id with its order object pointer
    std::unordered_map<uint64_t, Order*> orderMap;

public:

    ~OrderBook();

    void addOrder(Order* order);

    void removeOrder(uint64_t id);

    int getBestBid() const;

    int getBestAsk() const;

    void printBook() const;

    void printOrdersAtPrice(int price, Side side);

    // helper functions for matching engine

    bool hasBids() const;

    bool hasAsks() const;

    PriceLevel* getBestBidLevel();

    PriceLevel* getBestAskLevel();

    Order* getBestBidOrder();

    Order* getBestAskOrder();

    Order* findOrder(uint64_t id);
};