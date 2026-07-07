#include "OrderBook.hpp"

int64_t Order::activeAllocations = 0;
int64_t PriceLevel::activeAllocations = 0;

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
    auto it = orderMap.find(id);

    if(it == orderMap.end())
        return;

    Order* currOrder = it->second;

    PriceLevel* level = currOrder->priceLevel;

    level->currQuantity -= currOrder->remainingQuantity;

    // only node

    if(currOrder->prev == nullptr &&
       currOrder->next == nullptr)
    {
        level->head = nullptr;
        level->tail = nullptr;
    }

    // head node

    else if(currOrder->prev == nullptr)
    {
        level->head = currOrder->next;

        currOrder->next->prev = nullptr;
    }

    // tail node

    else if(currOrder->next == nullptr)
    {
        level->tail = currOrder->prev;

        currOrder->prev->next = nullptr;
    }

    // middle node

    else
    {
        currOrder->prev->next = currOrder->next;
        currOrder->next->prev = currOrder->prev;
    }

    currOrder->prev = nullptr;
    currOrder->next = nullptr;
    currOrder->priceLevel = nullptr;

    orderMap.erase(it);

    // remove empty price level

    if(level->head == nullptr)
    {
        if(currOrder->side == Side::BUY)
        {
            bid.erase(currOrder->price);
        }
        else
        {
            ask.erase(currOrder->price);
        }

        delete level;
    }

    delete currOrder;
}

void OrderBook::printBook() const
{
    std::cout << "Order Book:\n";
    std::cout << "--- Asks ---\n";
    for (auto it = ask.rbegin(); it != ask.rend(); ++it)
    {
        std::cout << "Price: " << it->first << " | Quantity: " << it->second->currQuantity << "\n";
    }
    std::cout << "--- Bids ---\n";
    for (auto it = bid.begin(); it != bid.end(); ++it)
    {
        std::cout << "Price: " << it->first << " | Quantity: " << it->second->currQuantity << "\n";
    }
    std::cout << "---------------------\n";
}

void OrderBook::printOrdersAtPrice(int price, Side side)
{
    PriceLevel* level = nullptr;
    if (side == Side::BUY)
    {
        auto it = bid.find(price);
        if (it != bid.end())
            level = it->second;
    }
    else
    {
        auto it = ask.find(price);
        if (it != ask.end())
            level = it->second;
    }

    if (level == nullptr)
    {
        std::cout << "No orders at price " << price << " for " << (side == Side::BUY ? "BUY" : "SELL") << "\n";
        return;
    }

    std::cout << "Orders at price " << price << " (" << (side == Side::BUY ? "BUY" : "SELL") << "):\n";
    Order* curr = level->head;
    while (curr != nullptr)
    {
        std::cout << "  ID: " << curr->id << " | Remaining Qty: " << curr->remainingQuantity << " | Timestamp: " << curr->timestamp << "\n";
        curr = curr->next;
    }
}

void OrderBook::clearMarketOrders()
{
    std::vector<uint64_t> idsToRemove;
    for (auto& [id, order] : orderMap)
    {
        if (!order->isStrategyOrder)
        {
            idsToRemove.push_back(id);
        }
    }
    for (uint64_t id : idsToRemove)
    {
        removeOrder(id);
    }
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