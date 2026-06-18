#include "OrderBook.hpp"

OrderBook::~OrderBook()
{
    for(auto &[price, level] : bid)
        delete level;

    for(auto &[price, level] : ask)
        delete level;
}

void OrderBook::addOrder(Order* order)
{
    if(orderMap.count(order->id))
    {
        throw std::runtime_error("Duplicate order id");
    }

    PriceLevel* level;

    if(order->side == Side::BUY)
    {
        auto it = bid.find(order->price);

        if(it == bid.end())
        {
            level = new PriceLevel(order->price);
            bid[order->price] = level;
        }
        else
        {
            level = it->second;
        }
    }
    else
    {
        auto it = ask.find(order->price);

        if(it == ask.end())
        {
            level = new PriceLevel(order->price);
            ask[order->price] = level;
        }
        else
        {
            level = it->second;
        }
    }

    order->priceLevel = level;

    level->currQuantity += order->remainingQuantity;

    if(level->head == nullptr)
    {
        level->head = order;
        level->tail = order;
    }
    else
    {
        level->tail->next = order;
        order->prev = level->tail;

        level->tail = order;
    }

    orderMap[order->id] = order;
}

void OrderBook::removeOrder(uint64_t id)
{
    // paste your existing implementation
}

int OrderBook::getBestBid() const
{
    if(bid.empty()) return -1;
    return bid.begin()->first;
}

int OrderBook::getBestAsk() const
{
    if(ask.empty()) return -1;
    return ask.begin()->first;
}

bool OrderBook::hasBids() const
{
    return !bid.empty();
}

bool OrderBook::hasAsks() const
{
    return !ask.empty();
}

PriceLevel* OrderBook::getBestBidLevel()
{
    if(bid.empty()) return nullptr;

    return bid.begin()->second;
}

PriceLevel* OrderBook::getBestAskLevel()
{
    if(ask.empty()) return nullptr;

    return ask.begin()->second;
}

Order* OrderBook::getBestBidOrder()
{
    auto level = getBestBidLevel();

    if(level == nullptr)
        return nullptr;

    return level->head;
}

Order* OrderBook::getBestAskOrder()
{
    auto level = getBestAskLevel();

    if(level == nullptr)
        return nullptr;

    return level->head;
}

Order* OrderBook::findOrder(uint64_t id)
{
    auto it = orderMap.find(id);

    if(it == orderMap.end())
        return nullptr;

    return it->second;
}