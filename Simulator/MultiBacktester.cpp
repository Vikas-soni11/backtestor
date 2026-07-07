#include "MultiBacktester.hpp"
#include <thread>
#include <iostream>
#include <algorithm>
#include <cstdio>

void MultiBacktester::runParallelBacktests(
    const std::string& marketDataPath,
    const std::string& tradeDataPath,
    const std::vector<std::pair<std::string, StrategyFactory>>& factories,
    int currentMidPrice
) {
    std::cout << "Starting parallel backtests for " << factories.size() << " strategy configurations...\n";

    // Vector to store results. Since each thread writes to a unique index,
    // we don't need any locks or mutexes for this vector.
    std::vector<BacktestResult> results(factories.size());

    // Vector to store spawned threads
    std::vector<std::thread> threads;

    for (size_t i = 0; i < factories.size(); ++i) {
        // Spawn a thread for each strategy configuration
        threads.push_back(std::thread([&marketDataPath, &tradeDataPath, &factories, &results, currentMidPrice, i]() {
            // 1. Create a local Simulator instance for this thread
            Simulator sim;

            // 2. Instantiate the Strategy locally using the factory lambda
            Strategy* strategy = factories[i].second(&sim);

            // 3. Run the backtest simulation locally on this thread
            sim.runBacktest(marketDataPath, tradeDataPath, strategy);

            // 4. Gather the results and store them in the results vector at index 'i'
            double finalVal = sim.getPortfolioValue(currentMidPrice);
            double netRet = finalVal - 100000.0;
            double netRetPct = (netRet / 100000.0) * 100.0;

            int totalT = sim.getCompletedTradePnLs().size();
            int wins = 0;
            for (double pnl : sim.getCompletedTradePnLs()) {
                if (pnl > 0) wins++;
            }
            double winR = (totalT > 0) ? (static_cast<double>(wins) / totalT) * 100.0 : 0.0;

            BacktestResult res;
            res.strategyName = factories[i].first;
            res.finalPortfolioValue = finalVal;
            res.netReturn = netRet;
            res.netReturnPercent = netRetPct;
            res.totalTrades = totalT;
            res.winRate = winR;
            res.maxDrawdownPercent = sim.getMaxDrawdownPercent() * 100.0;

            results[i] = res;

            // 5. Clean up local strategy allocation
            delete strategy;
        }));
    }

    // Wait for all threads to complete execution
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Sort the results in descending order of net return percentage
    std::sort(results.begin(), results.end(), [](const BacktestResult& a, const BacktestResult& b) {
        return a.netReturnPercent > b.netReturnPercent;
    });

    // Print a clean, comparative leaderboard of all runs
    std::cout << "\n==================================================================================\n";
    std::cout << "                           PARALLEL BACKTEST LEADERBOARD                          \n";
    std::cout << "==================================================================================\n";
    std::cout << " Rank | Strategy Name             | Net Return ($) & (%)          | Win Rate | Drawdown | Trades \n";
    std::cout << "------|---------------------------|-------------------------------|----------|----------|--------\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& res = results[i];
        std::printf("  %3d | %-25s | $%10.2f (%6.2f%%)    | %6.1f%%  | %7.2f%%  | %6d \n",
                    static_cast<int>(i + 1),
                    res.strategyName.c_str(),
                    res.netReturn,
                    res.netReturnPercent,
                    res.winRate,
                    res.maxDrawdownPercent,
                    res.totalTrades);
    }
    std::cout << "==================================================================================\n\n";
}
