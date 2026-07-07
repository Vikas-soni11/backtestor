#pragma once

#include <vector>
#include <string>
#include <unordered_set>

#include "../OrderBook/OrderBook.hpp"
#include "../MatchingEngine/MatchingEngine.hpp"
#include "../DataLoader/DataLoader.hpp"
#include "../Strategy/Strategy.hpp"

class Simulator {
private:
    MatchingEngine engine;
    
    double cash = 100000.0; // Start with $100,000 cash
    int position = 0;       // Position in product
    
    double avgEntryPrice = 0.0;
    double peakPortfolioValue = 100000.0;
    double maxDrawdownAbsolute = 0.0;
    double maxDrawdownPercent = 0.0;
    std::vector<double> completedTradePnLs;
    
    uint64_t nextStrategyOrderId = 1000000000; // Start strategy IDs at 1B
    uint64_t nextMarketOrderId = 1;            // Market orders start at 1
    uint64_t currentTimestamp = 0;
    
    std::unordered_set<uint64_t> strategyOrderIds;
    std::vector<Trade> strategyTrades;

    void processTrades(const std::vector<Trade>& trades);
    void processSnapshot(const BookSnapshot& snapshot, Strategy* strategy);

public:
    Simulator() = default;
    ~Simulator() = default;

    void runBacktest(const std::string& marketDataPath, const std::string& tradeDataPath, Strategy* strategy);

    std::vector<Trade> submitStrategyOrder(Side side, int price, int quantity);
    void cancelStrategyOrder(uint64_t orderId);
    void printSummaryReport(int currentMidPrice) const;

    double getCash() const { return cash; }
    int getPosition() const { return position; }
    const std::vector<Trade>& getStrategyTrades() const { return strategyTrades; }
    OrderBook& getOrderBook() { return engine.getOrderBook(); }
    
    double getPortfolioValue(int currentMidPrice) const {
        return cash + (position * currentMidPrice);
    }
    
    double getMaxDrawdownAbsolute() const { return maxDrawdownAbsolute; }
    double getMaxDrawdownPercent() const { return maxDrawdownPercent; }
    const std::vector<double>& getCompletedTradePnLs() const { return completedTradePnLs; }
};
