#pragma once

#include <vector>

#include "OrderBook.hpp"
#include "Trade.hpp"

class MatchingEngine
{
private:

    OrderBook orderBook;

    uint64_t nextTradeId = 1;

    std::vector<Trade> match(Order* incoming);

public:

    std::vector<Trade> submitOrder(Order* order);

    void cancelOrder(uint64_t orderId);

    OrderBook& getOrderBook();
};