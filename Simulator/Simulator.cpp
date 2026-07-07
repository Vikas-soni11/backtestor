#include "Simulator.hpp"
#include <iostream>
#include <algorithm>

void Simulator::processTrades(const std::vector<Trade>& trades) {
    for (const auto& trade : trades) {
        bool isBuy = (strategyOrderIds.count(trade.buyOrderId) > 0);
        bool isSell = (strategyOrderIds.count(trade.sellOrderId) > 0);
        
        if (!isBuy && !isSell) continue;
        
        int qty = trade.quantity;
        double price = trade.price;
        
        if (isBuy) {
            if (position >= 0) {
                // Adding to a long position
                double totalCost = (position * avgEntryPrice) + (qty * price);
                position += qty;
                avgEntryPrice = totalCost / position;
            } else {
                // Covering a short position (closer to zero)
                int closedQty = std::min(qty, -position);
                double tradePnL = closedQty * (avgEntryPrice - price);
                completedTradePnLs.push_back(tradePnL);
                
                position += qty;
                if (position == 0) {
                    avgEntryPrice = 0.0;
                }
            }
            cash -= (qty * price);
            strategyTrades.push_back(trade);
        }
        else if (isSell) {
            if (position <= 0) {
                // Adding to a short position
                double totalCost = (-position * avgEntryPrice) + (qty * price);
                position -= qty;
                avgEntryPrice = totalCost / (-position);
            } else {
                // Selling out of a long position (closer to zero)
                int closedQty = std::min(qty, position);
                double tradePnL = closedQty * (price - avgEntryPrice);
                completedTradePnLs.push_back(tradePnL);
                
                position -= qty;
                if (position == 0) {
                    avgEntryPrice = 0.0;
                }
            }
            cash += (qty * price);
            strategyTrades.push_back(trade);
        }
    }
}

void Simulator::processSnapshot(const BookSnapshot& snapshot, Strategy* strategy) {
    currentTimestamp = snapshot.timestamp;
    
    // 1. Clear old market orders from the book
    engine.getOrderBook().clearMarketOrders();
    
    // 2. Submit new market bids as resting BUY orders
    auto submitMarketLiquidity = [&](Side side, int price, int qty) {
        if (price > 0 && qty > 0) {
            Order* o = new Order();
            o->id = nextMarketOrderId++;
            o->side = side;
            o->price = price;
            o->remainingQuantity = qty;
            o->timestamp = currentTimestamp;
            o->isStrategyOrder = false;
            
            auto trades = engine.submitOrder(o);
            processTrades(trades);
        }
    };

    submitMarketLiquidity(Side::BUY, snapshot.bidPrice1, snapshot.bidVolume1);
    submitMarketLiquidity(Side::BUY, snapshot.bidPrice2, snapshot.bidVolume2);
    submitMarketLiquidity(Side::BUY, snapshot.bidPrice3, snapshot.bidVolume3);

    // 3. Submit new market asks as resting SELL orders
    submitMarketLiquidity(Side::SELL, snapshot.askPrice1, snapshot.askVolume1);
    submitMarketLiquidity(Side::SELL, snapshot.askPrice2, snapshot.askVolume2);
    submitMarketLiquidity(Side::SELL, snapshot.askPrice3, snapshot.askVolume3);

    // 4. Update Drawdown tracking
    int midPrice = (snapshot.bidPrice1 > 0 && snapshot.askPrice1 > 0) 
                   ? (snapshot.bidPrice1 + snapshot.askPrice1) / 2 
                   : (snapshot.midPrice > 0 ? snapshot.midPrice : 0);
    
    if (midPrice > 0) {
        double currentPortfolioValue = getPortfolioValue(midPrice);
        if (currentPortfolioValue > peakPortfolioValue) {
            peakPortfolioValue = currentPortfolioValue;
        } else {
            double drawdownAbs = peakPortfolioValue - currentPortfolioValue;
            double drawdownPct = drawdownAbs / peakPortfolioValue;
            
            if (drawdownAbs > maxDrawdownAbsolute) {
                maxDrawdownAbsolute = drawdownAbs;
            }
            if (drawdownPct > maxDrawdownPercent) {
                maxDrawdownPercent = drawdownPct;
            }
        }
    }

    // 5. Let the strategy react to this depth snapshot
    strategy->onDepthUpdate(snapshot);
}

std::vector<Trade> Simulator::submitStrategyOrder(Side side, int price, int quantity) {
    Order* o = new Order();
    o->id = nextStrategyOrderId++;
    o->side = side;
    o->price = price;
    o->remainingQuantity = quantity;
    o->timestamp = currentTimestamp;
    o->isStrategyOrder = true;
    
    strategyOrderIds.insert(o->id);
    
    auto trades = engine.submitOrder(o);
    processTrades(trades);
    return trades;
}

void Simulator::cancelStrategyOrder(uint64_t orderId) {
    if (strategyOrderIds.count(orderId) > 0) {
        engine.cancelOrder(orderId);
    }
}

void Simulator::runBacktest(const std::string& marketDataPath, const std::string& tradeDataPath, Strategy* strategy) {
    std::cout << "Starting backtest simulation...\n";
    
    auto snapshots = DataLoader::loadMarketData(marketDataPath);
    auto trades = DataLoader::loadTradeData(tradeDataPath);
    
    size_t snapIdx = 0;
    size_t tradeIdx = 0;
    
    // Reset performance tracking metrics
    cash = 100000.0;
    position = 0;
    avgEntryPrice = 0.0;
    peakPortfolioValue = 100000.0;
    maxDrawdownAbsolute = 0.0;
    maxDrawdownPercent = 0.0;
    strategyOrderIds.clear();
    strategyTrades.clear();
    completedTradePnLs.clear();
    
    // Chronological event processing loop
    while (snapIdx < snapshots.size() || tradeIdx < trades.size()) {
        uint64_t nextSnapTime = (snapIdx < snapshots.size()) ? snapshots[snapIdx].timestamp : -1ULL;
        uint64_t nextTradeTime = (tradeIdx < trades.size()) ? trades[tradeIdx].timestamp : -1ULL;
        
        if (nextSnapTime <= nextTradeTime) {
            processSnapshot(snapshots[snapIdx], strategy);
            snapIdx++;
        } else {
            strategy->onMarketTrade(trades[tradeIdx]);
            tradeIdx++;
        }
    }
    
    std::cout << "Backtest simulation complete!\n";
}

void Simulator::printSummaryReport(int currentMidPrice) const {
    double finalPortfolioValue = getPortfolioValue(currentMidPrice);
    double totalReturn = finalPortfolioValue - 100000.0;
    double totalReturnPercent = (totalReturn / 100000.0) * 100.0;

    int totalTrades = completedTradePnLs.size();
    int winningTrades = 0;
    double totalPnLSum = 0.0;
    
    for (double pnl : completedTradePnLs) {
        if (pnl > 0) winningTrades++;
        totalPnLSum += pnl;
    }
    
    double winRate = (totalTrades > 0) ? (static_cast<double>(winningTrades) / totalTrades) * 100.0 : 0.0;
    double avgPnL = (totalTrades > 0) ? totalPnLSum / totalTrades : 0.0;

    std::cout << "\n======================================\n";
    std::cout << "        BACKTEST SUMMARY REPORT        \n";
    std::cout << "======================================\n";
    std::cout << "Initial Portfolio Value: $100,000.00\n";
    std::cout << "Final Portfolio Value:   $" << finalPortfolioValue << "\n";
    std::cout << "Net Return:              $" << totalReturn << " (" << totalReturnPercent << "%)\n";
    std::cout << "Final Position:          " << position << "\n";
    std::cout << "Final Cash:              $" << cash << "\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Total Completed Trades:  " << totalTrades << "\n";
    std::cout << "Winning Trades:          " << winningTrades << "\n";
    std::cout << "Win Rate:                " << winRate << "%\n";
    std::cout << "Average Profit per Trade: $" << avgPnL << "\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Max Drawdown (Absolute): $" << maxDrawdownAbsolute << "\n";
    std::cout << "Max Drawdown (Percent):  " << (maxDrawdownPercent * 100.0) << "%\n";
    std::cout << "======================================\n\n";
}
