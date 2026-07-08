#include <iostream>
#include <cassert>
#include <vector>
#include <stdexcept>
#include <cstdlib>

#include <fstream>
#include "OrderBook/OrderBook.hpp"
#include "MatchingEngine/MatchingEngine.hpp"
#include "DataLoader/DataLoader.hpp"
#include "Strategy/Strategy.hpp"
#include "Simulator/Simulator.hpp"
#include "Simulator/MultiBacktester.hpp"

// Memory tracker using class-specific allocation counters to be robust under optimizations
#define activeAllocations (Order::activeAllocations + PriceLevel::activeAllocations)

Order* createOrder(uint64_t id, Side side, int price, int qty, uint64_t ts) {
    Order* o = new Order();
    o->id = id;
    o->side = side;
    o->price = price;
    o->remainingQuantity = qty;
    o->timestamp = ts;
    return o;
}

void test_basic_matching() {
    std::cout << "Running test_basic_matching...\n";
    int64_t initialAlloc = activeAllocations;
    {
        MatchingEngine engine;

        // Submit Sell order first: 10 units at 100
        Order* s1 = createOrder(1, Side::SELL, 100, 10, 1000);
        auto trades1 = engine.submitOrder(s1);
        assert(trades1.empty());
        assert(engine.getOrderBook().getBestAsk() == 100);

        // Submit Buy order: 10 units at 100 (should match)
        Order* b1 = createOrder(2, Side::BUY, 100, 10, 1001);
        auto trades2 = engine.submitOrder(b1);
        assert(trades2.size() == 1);
        assert(trades2[0].price == 100);
        assert(trades2[0].quantity == 10);
        assert(trades2[0].buyOrderId == 2);
        assert(trades2[0].sellOrderId == 1);

        assert(!engine.getOrderBook().hasAsks());
        assert(!engine.getOrderBook().hasBids());
    }
    // All allocations within the block (engine, trades, orders) should be cleaned up.
    assert(activeAllocations == initialAlloc);
    std::cout << "test_basic_matching passed!\n";
}

void test_price_time_priority() {
    std::cout << "Running test_price_time_priority...\n";
    int64_t initialAlloc = activeAllocations;
    {
        MatchingEngine engine;

        // Submit two sell orders at price 100.
        // Order 1: 5 units at 100 (time 1000)
        // Order 2: 10 units at 100 (time 1001)
        engine.submitOrder(createOrder(1, Side::SELL, 100, 5, 1000));
        engine.submitOrder(createOrder(2, Side::SELL, 100, 10, 1001));

        // Submit buy order: 8 units at 100 (time 1002)
        // It should match 5 units from Order 1 (fully filled) and 3 units from Order 2 (partially filled, 7 remaining).
        auto trades = engine.submitOrder(createOrder(3, Side::BUY, 100, 8, 1002));
        assert(trades.size() == 2);

        assert(trades[0].sellOrderId == 1);
        assert(trades[0].quantity == 5);

        assert(trades[1].sellOrderId == 2);
        assert(trades[1].quantity == 3);

        // Check book state: Order 1 should be gone, Order 2 should have 7 remaining quantity.
        OrderBook& book = engine.getOrderBook();
        assert(book.getBestAsk() == 100);
        Order* s2 = book.findOrder(2);
        assert(s2 != nullptr);
        assert(s2->remainingQuantity == 7);
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_price_time_priority passed!\n";
}

void test_partial_fill_and_spread() {
    std::cout << "Running test_partial_fill_and_spread...\n";
    int64_t initialAlloc = activeAllocations;
    {
        MatchingEngine engine;

        // Buy order: 10 units at 99
        engine.submitOrder(createOrder(1, Side::BUY, 99, 10, 1000));
        // Sell order: 15 units at 101 (creates spread)
        engine.submitOrder(createOrder(2, Side::SELL, 101, 15, 1001));

        OrderBook& book = engine.getOrderBook();
        assert(book.getBestBid() == 99);
        assert(book.getBestAsk() == 101);

        // Submit Sell order: 5 units at 98 (should fill against best bid at 99)
        auto trades = engine.submitOrder(createOrder(3, Side::SELL, 98, 5, 1002));
        assert(trades.size() == 1);
        assert(trades[0].price == 99); // price of resting order
        assert(trades[0].quantity == 5);
        assert(trades[0].buyOrderId == 1);
        assert(trades[0].sellOrderId == 3);

        assert(book.getBestBid() == 99);
        Order* b1 = book.findOrder(1);
        assert(b1 != nullptr);
        assert(b1->remainingQuantity == 5);
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_partial_fill_and_spread passed!\n";
}

void test_cancel_order() {
    std::cout << "Running test_cancel_order...\n";
    int64_t initialAlloc = activeAllocations;
    {
        MatchingEngine engine;

        engine.submitOrder(createOrder(1, Side::BUY, 100, 10, 1000));
        engine.submitOrder(createOrder(2, Side::BUY, 100, 20, 1001));

        OrderBook& book = engine.getOrderBook();
        assert(book.getBestBid() == 100);

        // Cancel order 1 (head of price level)
        engine.cancelOrder(1);
        assert(book.findOrder(1) == nullptr);
        assert(book.findOrder(2) != nullptr);

        // Cancel order 2 (last remaining order at price level)
        engine.cancelOrder(2);
        assert(book.findOrder(2) == nullptr);
        assert(!book.hasBids());
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_cancel_order passed!\n";
}

void test_duplicate_order_id() {
    std::cout << "Running test_duplicate_order_id...\n";
    int64_t initialAlloc = activeAllocations;
    {
        MatchingEngine engine;

        engine.submitOrder(createOrder(1, Side::BUY, 100, 10, 1000));

        bool exceptionThrown = false;
        Order* duplicate = createOrder(1, Side::SELL, 100, 5, 1001);
        try {
            engine.submitOrder(duplicate);
        } catch (const std::runtime_error& e) {
            exceptionThrown = true;
            delete duplicate; // delete since it failed to insert
        }
        assert(exceptionThrown);
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_duplicate_order_id passed!\n";
}

void test_print_functions() {
    std::cout << "Running test_print_functions...\n";
    int64_t initialAlloc = activeAllocations;
    {
        MatchingEngine engine;

        engine.submitOrder(createOrder(1, Side::BUY, 99, 10, 1000));
        engine.submitOrder(createOrder(2, Side::BUY, 100, 15, 1001));
        engine.submitOrder(createOrder(3, Side::SELL, 105, 20, 1002));
        engine.submitOrder(createOrder(4, Side::SELL, 106, 25, 1003));

        std::cout << "\n--- EXPECTED ORDER BOOK PRINT ---\n";
        engine.getOrderBook().printBook();

        std::cout << "\n--- EXPECTED ORDERS AT PRICE 100 BUY ---\n";
        engine.getOrderBook().printOrdersAtPrice(100, Side::BUY);

        std::cout << "\n--- EXPECTED ORDERS AT PRICE 105 SELL ---\n";
        engine.getOrderBook().printOrdersAtPrice(105, Side::SELL);
        std::cout << "--------------------------------------\n\n";
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_print_functions passed!\n";
}

void test_data_loader() {
    std::cout << "Running test_data_loader...\n";
    int64_t initialAlloc = activeAllocations;
    {
        std::string mktPath = "temp_market_data.csv";
        std::ofstream mktFile(mktPath);
        mktFile << "day;timestamp;product;bid_price_1;bid_volume_1;bid_price_2;bid_volume_2;bid_price_3;bid_volume_3;ask_price_1;ask_volume_1;ask_price_2;ask_volume_2;ask_price_3;ask_volume_3;mid_price;profit_and_loss\n";
        mktFile << "0;0;ASH_COATED_OSMIUM;;;;;;;10013;30;;;;;10013.0;0.0\n";
        mktFile << "0;100;INTARIAN_PEPPER_ROOT;11994;9;11991;23;;;12007;9;12010;23;;;12000.5;0.0\n";
        mktFile.close();

        std::string trdPath = "temp_trade_data.csv";
        std::ofstream trdFile(trdPath);
        trdFile << "timestamp;buyer;seller;symbol;currency;price;quantity\n";
        trdFile << "200;;;INTARIAN_PEPPER_ROOT;XIRECS;12007.0;5\n";
        trdFile << "3300;;;INTARIAN_PEPPER_ROOT;XIRECS;12010.0;7\n";
        trdFile.close();

        auto snapshots = DataLoader::loadMarketData(mktPath);
        assert(snapshots.size() == 2);
        
        assert(snapshots[0].day == 0);
        assert(snapshots[0].timestamp == 0);
        assert(snapshots[0].product == "ASH_COATED_OSMIUM");
        assert(snapshots[0].bidPrice1 == -1);
        assert(snapshots[0].askPrice1 == 10013);
        assert(snapshots[0].askVolume1 == 30);
        assert(snapshots[0].askPrice2 == -1);
        assert(snapshots[0].midPrice == 10013.0);

        assert(snapshots[1].day == 0);
        assert(snapshots[1].timestamp == 100);
        assert(snapshots[1].product == "INTARIAN_PEPPER_ROOT");
        assert(snapshots[1].bidPrice1 == 11994);
        assert(snapshots[1].bidVolume1 == 9);
        assert(snapshots[1].bidPrice2 == 11991);
        assert(snapshots[1].bidVolume2 == 23);
        assert(snapshots[1].bidPrice3 == -1);
        assert(snapshots[1].askPrice1 == 12007);
        assert(snapshots[1].askVolume1 == 9);
        assert(snapshots[1].askPrice2 == 12010);
        assert(snapshots[1].askVolume2 == 23);
        assert(snapshots[1].askPrice3 == -1);
        assert(snapshots[1].midPrice == 12000.5);

        auto trades = DataLoader::loadTradeData(trdPath);
        assert(trades.size() == 2);

        assert(trades[0].timestamp == 200);
        assert(trades[0].buyer == "");
        assert(trades[0].seller == "");
        assert(trades[0].symbol == "INTARIAN_PEPPER_ROOT");
        assert(trades[0].currency == "XIRECS");
        assert(trades[0].price == 12007.0);
        assert(trades[0].quantity == 5);

        assert(trades[1].timestamp == 3300);
        assert(trades[1].buyer == "");
        assert(trades[1].seller == "");
        assert(trades[1].symbol == "INTARIAN_PEPPER_ROOT");
        assert(trades[1].currency == "XIRECS");
        assert(trades[1].price == 12010.0);
        assert(trades[1].quantity == 7);

        std::remove(mktPath.c_str());
        std::remove(trdPath.c_str());
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_data_loader passed!\n";
}

class DummyStrategy : public Strategy {
private:
    uint64_t restingOrderId = 0;
    int buyThreshold;
    int sellThreshold;
    bool silent;
public:
    DummyStrategy(Simulator* sim, int buyThresh = 11000, int sellThresh = 12000, bool silentMode = false) 
        : Strategy(sim), buyThreshold(buyThresh), sellThreshold(sellThresh), silent(silentMode) {}

    void onDepthUpdate(const BookSnapshot& snapshot) override {
        // Place a resting order at t=0
        if (snapshot.timestamp == 0) {
            auto trades = simulator->submitStrategyOrder(Side::BUY, 9000, 5);
            assert(trades.empty()); // limit order at 9000 shouldn't fill immediately
            restingOrderId = 1000000000; // First strategy ID starts at 1B
            
            // Check that it's in the order book
            assert(simulator->getOrderBook().findOrder(restingOrderId) != nullptr);
        }

        // Cancel the resting order at t=100
        if (snapshot.timestamp == 100 && restingOrderId > 0) {
            if (!silent) std::cout << "[Strategy] Cancelling resting order " << restingOrderId << "\n";
            simulator->cancelStrategyOrder(restingOrderId);
            
            // Verify it was removed
            assert(simulator->getOrderBook().findOrder(restingOrderId) == nullptr);
            restingOrderId = 0;
        }

        // Buy if askPrice1 < buyThreshold and position is 0
        if (snapshot.askPrice1 > 0 && snapshot.askPrice1 < buyThreshold && simulator->getPosition() == 0) {
            if (!silent) std::cout << "[Strategy] BUY trigger: placing order at ask price " << snapshot.askPrice1 << "\n";
            simulator->submitStrategyOrder(Side::BUY, 20000, 5); // Place BUY of 5 at high limit
        }
        // Sell if bidPrice1 > sellThreshold and position > 0
        if (snapshot.bidPrice1 > 0 && snapshot.bidPrice1 > sellThreshold && simulator->getPosition() > 0) {
            if (!silent) std::cout << "[Strategy] SELL trigger: placing order at bid price " << snapshot.bidPrice1 << "\n";
            simulator->submitStrategyOrder(Side::SELL, 1, 5); // Place SELL of 5 at low limit
        }
    }

    void onMarketTrade(const HistoricalTrade& trade) override {
        if (!silent) std::cout << "[Strategy] Observed market trade: " << trade.symbol << " qty " << trade.quantity << " at " << trade.price << "\n";
    }
};

void test_strategy_simulation() {
    std::cout << "Running test_strategy_simulation...\n";
    int64_t initialAlloc = activeAllocations;
    {
        std::string mktPath = "temp_market_data.csv";
        std::ofstream mktFile(mktPath);
        mktFile << "day;timestamp;product;bid_price_1;bid_volume_1;bid_price_2;bid_volume_2;bid_price_3;bid_volume_3;ask_price_1;ask_volume_1;ask_price_2;ask_volume_2;ask_price_3;ask_volume_3;mid_price;profit_and_loss\n";
        mktFile << "0;0;ASH_COATED_OSMIUM;;;;;;;10013;30;;;;;10013.0;0.0\n";
        mktFile << "0;100;INTARIAN_PEPPER_ROOT;12009;19;11991;23;;;;;;;;;12000.5;0.0\n";
        mktFile.close();

        std::string trdPath = "temp_trade_data.csv";
        std::ofstream trdFile(trdPath);
        trdFile << "timestamp;buyer;seller;symbol;currency;price;quantity\n";
        trdFile << "50;;;INTARIAN_PEPPER_ROOT;XIRECS;11998.0;6\n";
        trdFile.close();

        Simulator sim;
        DummyStrategy strat(&sim, 11000, 12000);

        sim.runBacktest(mktPath, trdPath, &strat);

        // Assert strategy position and cash are updated
        assert(sim.getPosition() == 0);
        // Bought 5 at 10013, sold 5 at 12009.
        // P&L = 5 * (12009 - 10013) = 5 * 1996 = 9980.
        // Cash = 100000 + 9980 = 109980.
        assert(sim.getCash() == 109980.0);
        
        // Assert trade and drawdown calculations
        assert(sim.getStrategyTrades().size() == 2);
        assert(sim.getCompletedTradePnLs().size() == 1);
        assert(sim.getCompletedTradePnLs()[0] == 9980.0);
        assert(sim.getMaxDrawdownAbsolute() == 0.0); // Portfolio value went straight up
        assert(sim.getMaxDrawdownPercent() == 0.0);

        // Output summary report to verify formatting
        sim.printSummaryReport(12009);

        std::remove(mktPath.c_str());
        std::remove(trdPath.c_str());
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_strategy_simulation passed!\n";
}

// Define a simple standalone strategy function
void mySimpleTradingRule(Simulator* sim, const BookSnapshot& snapshot) {
    if (snapshot.askPrice1 > 0 && snapshot.askPrice1 < 10800 && sim->getPosition() == 0) {
        sim->submitStrategyOrder(Side::BUY, 20000, 5);
    }
    if (snapshot.bidPrice1 > 0 && snapshot.bidPrice1 > 12000 && sim->getPosition() > 0) {
        sim->submitStrategyOrder(Side::SELL, 1, 5);
    }
}

void test_parallel_backtests() {
    std::cout << "Running test_parallel_backtests...\n";
    int64_t initialAlloc = activeAllocations;
    {
        std::string mktPath = "temp_market_data.csv";
        std::ofstream mktFile(mktPath);
        mktFile << "day;timestamp;product;bid_price_1;bid_volume_1;bid_price_2;bid_volume_2;bid_price_3;bid_volume_3;ask_price_1;ask_volume_1;ask_price_2;ask_volume_2;ask_price_3;ask_volume_3;mid_price;profit_and_loss\n";
        mktFile << "0;0;ASH_COATED_OSMIUM;;;;;;;10013;30;;;;;10013.0;0.0\n";
        mktFile << "0;100;INTARIAN_PEPPER_ROOT;12009;19;11991;23;;;;;;;;;12000.5;0.0\n";
        mktFile.close();

        std::string trdPath = "temp_trade_data.csv";
        std::ofstream trdFile(trdPath);
        trdFile << "timestamp;buyer;seller;symbol;currency;price;quantity\n";
        trdFile << "50;;;INTARIAN_PEPPER_ROOT;XIRECS;11998.0;6\n";
        trdFile.close();

        // Define factories with different configurations
        std::vector<std::pair<std::string, StrategyFactory>> factories;
        
        // Configuration 1: Standard thresholds (Wins $9980)
        factories.push_back({"Strat_Buy11k_Sell12k", [](Simulator* sim) {
            return new DummyStrategy(sim, 11000, 12000, true);
        }});

        // Configuration 2: Too tight buy threshold (No trade, returns $0)
        factories.push_back({"Strat_Buy10k_Sell13k", [](Simulator* sim) {
            return new DummyStrategy(sim, 10000, 13000, true);
        }});

        // Configuration 3: Alternative threshold (Wins $9980)
        factories.push_back({"Strat_Buy10k5_Sell12k", [](Simulator* sim) {
            return new DummyStrategy(sim, 10500, 12000, true);
        }});

        // Configuration 4: Plain strategy function wrapped automatically (Wins $9980)
        factories.push_back({"Functional_Buy10k8_Sell12k", [](Simulator* sim) {
            return new FunctionalStrategy(sim, mySimpleTradingRule);
        }});

        // Run the backtests in parallel
        MultiBacktester::runParallelBacktests(mktPath, trdPath, factories, 12009);

        std::remove(mktPath.c_str());
        std::remove(trdPath.c_str());
    }
    #undef activeAllocations
    int64_t currentAlloc = Order::activeAllocations + PriceLevel::activeAllocations;
    if (currentAlloc != initialAlloc) {
        std::cout << "[ERROR] Leak in test_parallel_backtests! Initial: " << initialAlloc 
                  << " Current: " << currentAlloc 
                  << " Orders: " << Order::activeAllocations 
                  << " Levels: " << PriceLevel::activeAllocations << "\n";
    }
    assert(currentAlloc == initialAlloc);
    #define activeAllocations (Order::activeAllocations + PriceLevel::activeAllocations)
    std::cout << "test_parallel_backtests passed!\n";
}

#include <chrono>

void test_matching_engine_benchmark() {
    std::cout << "Starting Matching Engine Benchmark (1,000,000 orders)...\n";
    int64_t initialAlloc = activeAllocations;
    {
        // 1. Generate 1,000,000 pre-allocated orders to avoid allocation overhead during benchmark
        std::vector<Order*> orders;
        orders.reserve(1000000);
        for (int i = 0; i < 1000000; ++i) {
            Order* o = new Order();
            o->id = i;
            o->side = (i % 2 == 0) ? Side::BUY : Side::SELL;
            o->price = 10000 + (rand() % 20); // dense price range to trigger matching
            o->remainingQuantity = 10 + (rand() % 100);
            o->timestamp = i;
            orders.push_back(o);
        }

        MatchingEngine engine;

        // 2. Start timer
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 1000000; ++i) {
            engine.submitOrder(orders[i]);
        }

        // 3. Stop timer
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;

        double seconds = diff.count();
        double throughput = 1000000.0 / seconds;
        double avgLatencyNs = (seconds / 1000000.0) * 1e9;

        std::cout << "\n======================================\n";
        std::cout << "        BENCHMARK PERFORMANCE        \n";
        std::cout << "======================================\n";
        std::cout << "Total Orders Processed: 1,000,000\n";
        std::cout << "Total Elapsed Time:     " << seconds << " seconds\n";
        std::cout << "Throughput:             " << (throughput / 1e6) << " Million Orders/sec\n";
        std::cout << "Average Latency:        " << avgLatencyNs << " nanoseconds (ns)\n";
        std::cout << "======================================\n\n";
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_matching_engine_benchmark passed!\n";
}

void create_large_mock_files(const std::string& mktPath, const std::string& trdPath, int rows) {
    std::ofstream mktFile(mktPath);
    mktFile << "day;timestamp;product;bid_price_1;bid_volume_1;bid_price_2;bid_volume_2;bid_price_3;bid_volume_3;ask_price_1;ask_volume_1;ask_price_2;ask_volume_2;ask_price_3;ask_volume_3;mid_price;profit_and_loss\n";
    for (int i = 0; i < rows; ++i) {
        mktFile << "0;" << (i * 100) << ";INTARIAN_PEPPER_ROOT;"
                << (12000 + (i % 10)) << ";10;"
                << (11990 + (i % 10)) << ";20;;"
                << (12010 + (i % 10)) << ";15;"
                << (12020 + (i % 10)) << ";25;;12000;0.0\n";
    }
    mktFile.close();

    std::ofstream trdFile(trdPath);
    trdFile << "timestamp;buyer;seller;symbol;currency;price;quantity\n";
    for (int i = 0; i < rows / 10; ++i) {
        trdFile << (i * 1000 + 50) << ";;;INTARIAN_PEPPER_ROOT;XIRECS;12000.0;5\n";
    }
    trdFile.close();
}

void test_concurrency_scaling() {
    std::cout << "Running test_concurrency_scaling (Sequential vs Parallel backtests)...\n";
    int64_t initialAlloc = activeAllocations;
    {
        std::string mktPath = "bench_market_data.csv";
        std::string trdPath = "bench_trade_data.csv";
        create_large_mock_files(mktPath, trdPath, 20000);

        // Prepare 8 runs
        std::vector<std::pair<std::string, StrategyFactory>> factories;
        for (int i = 0; i < 8; ++i) {
            factories.push_back({"Strat_" + std::to_string(i), [](Simulator* sim) {
                return new DummyStrategy(sim, 11000, 12000, true);
            }});
        }

        // 1. Sequential Execution
        std::cout << "Running 8 backtests sequentially...\n";
        auto startSeq = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 8; ++i) {
            Simulator sim;
            Strategy* strat = factories[i].second(&sim);
            sim.runBacktest(mktPath, trdPath, strat);
            delete strat;
        }
        auto endSeq = std::chrono::high_resolution_clock::now();
        double timeSeq = std::chrono::duration<double>(endSeq - startSeq).count();

        // 2. Parallel Execution
        std::cout << "Running 8 backtests in parallel...\n";
        auto startPar = std::chrono::high_resolution_clock::now();
        MultiBacktester::runParallelBacktests(mktPath, trdPath, factories, 12000);
        auto endPar = std::chrono::high_resolution_clock::now();
        double timePar = std::chrono::duration<double>(endPar - startPar).count();

        double speedup = timeSeq / timePar;
        double efficiency = (speedup / std::thread::hardware_concurrency()) * 100.0;

        std::cout << "\n======================================\n";
        std::cout << "     CONCURRENCY BENCHMARK RESULTS    \n";
        std::cout << "======================================\n";
        std::cout << "Total Backtests Evaluated: 8\n";
        std::cout << "Available CPU Cores:       " << std::thread::hardware_concurrency() << "\n";
        std::cout << "Sequential Time:           " << timeSeq << " seconds\n";
        std::cout << "Parallel Time:             " << timePar << " seconds\n";
        std::cout << "Speedup Factor:            " << speedup << "x faster\n";
        std::cout << "Parallel Efficiency:       " << efficiency << "%\n";
        std::cout << "======================================\n\n";

        std::remove(mktPath.c_str());
        std::remove(trdPath.c_str());
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_concurrency_scaling passed!\n";
}

class PositionFlipStrategy : public Strategy {
public:
    using Strategy::Strategy;
    void onDepthUpdate(const BookSnapshot&) override {}
    void onMarketTrade(const HistoricalTrade&) override {}
};

void test_position_flipping() {
    std::cout << "Running test_position_flipping...\n";
    int64_t initialAlloc = activeAllocations;
    {
        Simulator sim;
        PositionFlipStrategy strat(&sim);
        OrderBook& book = sim.getOrderBook();

        // 1. Open a short position of -30 shares by selling 30 at 10000
        Order* buyer = new Order();
        buyer->id = 1;
        buyer->side = Side::BUY;
        buyer->price = 10000;
        buyer->remainingQuantity = 100;
        buyer->timestamp = 0;
        buyer->isStrategyOrder = false;
        
        book.addOrder(buyer);
        sim.submitStrategyOrder(Side::SELL, 10000, 30);
        assert(sim.getPosition() == -30);

        // 2. Buy 50 shares at 11000 (flips position from -30 to +20)
        Order* seller = new Order();
        seller->id = 2;
        seller->side = Side::SELL;
        seller->price = 11000;
        seller->remainingQuantity = 100;
        seller->timestamp = 0;
        seller->isStrategyOrder = false;

        book.addOrder(seller);
        sim.submitStrategyOrder(Side::BUY, 11000, 50);
        
        assert(sim.getPosition() == 20);
        // First 30 shares cover short at a loss of 30 * (10000 - 11000) = -30000
        assert(sim.getCompletedTradePnLs().size() == 1);
        assert(sim.getCompletedTradePnLs()[0] == -30000.0);

        // 3. Sell 30 shares at 12000 (flips position from +20 to -10)
        Order* buyer2 = new Order();
        buyer2->id = 3;
        buyer2->side = Side::BUY;
        buyer2->price = 12000;
        buyer2->remainingQuantity = 100;
        buyer2->timestamp = 0;
        buyer2->isStrategyOrder = false;

        book.addOrder(buyer2);
        sim.submitStrategyOrder(Side::SELL, 12000, 30);

        assert(sim.getPosition() == -10);
        // Sells out 20 longs at profit of 20 * (12000 - 11000) = +20000
        assert(sim.getCompletedTradePnLs().size() == 2);
        assert(sim.getCompletedTradePnLs()[1] == 20000.0);

        // Clean up remaining market orders from the book to prevent leaks
        book.clearMarketOrders();
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_position_flipping passed!\n";
}

void test_simulator_constraints() {
    std::cout << "Running test_simulator_constraints...\n";
    int64_t initialAlloc = activeAllocations;
    {
        Simulator sim;
        sim.setEnforceConstraints(true);
        OrderBook& book = sim.getOrderBook();

        // 1. Attempt to SELL when position is 0 -> should be rejected
        auto trades1 = sim.submitStrategyOrder(Side::SELL, 10000, 5);
        assert(trades1.empty());
        assert(sim.getPosition() == 0);

        // 2. Attempt to BUY costing more than cash (costs 5 * 25k = 125k, cash starts at 100k) -> should be rejected
        auto trades2 = sim.submitStrategyOrder(Side::BUY, 25000, 5);
        assert(trades2.empty());
        assert(sim.getCash() == 100000.0);

        // 3. Inject a market seller at 10000
        Order* seller = new Order();
        seller->id = 1;
        seller->side = Side::SELL;
        seller->price = 10000;
        seller->remainingQuantity = 10;
        seller->timestamp = 0;
        seller->isStrategyOrder = false;
        
        book.addOrder(seller);

        // Submit valid BUY of 5 at 10000 -> should succeed and fill
        auto trades3 = sim.submitStrategyOrder(Side::BUY, 10000, 5);
        assert(!trades3.empty());
        assert(sim.getPosition() == 5);
        assert(sim.getCash() == 50000.0);

        // Clean up remaining market orders from the book to prevent leaks
        book.clearMarketOrders();
    }
    assert(activeAllocations == initialAlloc);
    std::cout << "test_simulator_constraints passed!\n";
}

int main() {
    std::cout << "======================================\n";
    std::cout << "Starting Matching Engine Test Suite\n";
    std::cout << "======================================\n";

    test_basic_matching();
    test_price_time_priority();
    test_partial_fill_and_spread();
    test_cancel_order();
    test_duplicate_order_id();
    test_print_functions();
    test_data_loader();
    test_strategy_simulation();
    test_parallel_backtests();
    test_matching_engine_benchmark();
    test_concurrency_scaling();
    test_position_flipping();
    test_simulator_constraints();

    std::cout << "======================================\n";
    std::cout << "All tests passed successfully! Zero leaks.\n";
    std::cout << "======================================\n";
    return 0;
}