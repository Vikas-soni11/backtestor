#pragma once

#include "../DataLoader/DataLoader.hpp"

#include <functional>

class Simulator;

class Strategy {
protected:
    Simulator* simulator;

public:
    Strategy(Simulator* sim) : simulator(sim) {}
    virtual ~Strategy() = default;

    virtual void onDepthUpdate(const BookSnapshot& snapshot) = 0;
    virtual void onMarketTrade(const HistoricalTrade& trade) = 0;
};

// A helper wrapper to write strategies as simple standalone functions
class FunctionalStrategy : public Strategy {
private:
    std::function<void(Simulator*, const BookSnapshot&)> func;
public:
    FunctionalStrategy(Simulator* sim, std::function<void(Simulator*, const BookSnapshot&)> f)
        : Strategy(sim), func(f) {}

    void onDepthUpdate(const BookSnapshot& snapshot) override {
        func(simulator, snapshot);
    }

    void onMarketTrade(const HistoricalTrade& trade) override {
        // Left empty for simple strategies that do not monitor raw trade logs
    }
};
