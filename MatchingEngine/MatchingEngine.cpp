#include "MatchingEngine.hpp"

using namespace std;

OrderBook& MatchingEngine::getOrderBook()
{
    return orderBook;
}

void MatchingEngine::cancelOrder(uint64_t orderId)
{
    orderBook.removeOrder(orderId);
}

vector<Trade> MatchingEngine::submitOrder(Order* order)
{
    if (orderBook.findOrder(order->id) != nullptr)
    {
        throw std::runtime_error("Duplicate order id");
    }
    return match(order);
}

vector<Trade> MatchingEngine::match(Order* incoming)
{
    vector<Trade> trades;

    while(incoming->remainingQuantity > 0)
    {
        PriceLevel* bestLevel = nullptr;

        //-------------------------------------------------
        // Find best opposite price level
        //-------------------------------------------------

        if(incoming->side == Side::BUY)
        {
            if(!orderBook.hasAsks())
                break;

            bestLevel = orderBook.getBestAskLevel();

            if(bestLevel->price > incoming->price)
                break;
        }
        else
        {
            if(!orderBook.hasBids())
                break;

            bestLevel = orderBook.getBestBidLevel();

            if(bestLevel->price < incoming->price)
                break;
        }

        //-------------------------------------------------
        // Oldest resting order at best price
        //-------------------------------------------------

        Order* resting = bestLevel->head;

        //-------------------------------------------------
        // Execute trade
        //-------------------------------------------------

        int tradedQty =
            min(incoming->remainingQuantity,
                resting->remainingQuantity);

        Trade trade;

        trade.tradeId = nextTradeId++;

        if(incoming->side == Side::BUY)
        {
            trade.buyOrderId = incoming->id;
            trade.sellOrderId = resting->id;
        }
        else
        {
            trade.buyOrderId = resting->id;
            trade.sellOrderId = incoming->id;
        }

        // Price of resting order
        trade.price = resting->price;

        trade.quantity = tradedQty;

        trade.timestamp =
            max(incoming->timestamp,
                resting->timestamp);

        trades.push_back(trade);

        //-------------------------------------------------
        // Update quantities
        //-------------------------------------------------

        incoming->remainingQuantity -= tradedQty;

        resting->remainingQuantity -= tradedQty;

        bestLevel->currQuantity -= tradedQty;

        //-------------------------------------------------
        // Remove fully executed resting order
        //-------------------------------------------------

        if(resting->remainingQuantity == 0)
        {
            orderBook.removeOrder(resting->id);
        }
    }

    //-------------------------------------------------
    // Remaining quantity becomes resting order
    //-------------------------------------------------

    if(incoming->remainingQuantity > 0)
    {
        orderBook.addOrder(incoming);
    }
    else
    {
        delete incoming;
    }

    return trades;
}