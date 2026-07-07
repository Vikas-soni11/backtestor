#pragma once

#include <vector>
#include <string>
#include <functional>
#include "Simulator.hpp"

// Structure to hold backtest performance metrics for a single run
struct BacktestResult {
    std::string strategyName;
    double finalPortfolioValue = 0.0;
    double netReturn = 0.0;
    double netReturnPercent = 0.0;
    int totalTrades = 0;
    double winRate = 0.0;
    double maxDrawdownPercent = 0.0;
};

// Define a StrategyFactory as a function that instantiates a Strategy given a Simulator pointer.
// This allows each thread to instantiate its own local Strategy object.
using StrategyFactory = std::function<Strategy*(Simulator*)>;

class MultiBacktester {
public:
    // Spawns parallel threads to run multiple backtest configurations independently.
    // At completion, prints a sorted comparative leaderboard of strategy returns.
    static void runParallelBacktests(
        const std::string& marketDataPath,
        const std::string& tradeDataPath,
        const std::vector<std::pair<std::string, StrategyFactory>>& factories,
        int currentMidPrice
    );
};
