#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct BookSnapshot {
    int day;
    uint64_t timestamp;
    std::string product;
    
    // Bids (-1 means empty level)
    int bidPrice1 = -1;
    int bidVolume1 = 0;
    int bidPrice2 = -1;
    int bidVolume2 = 0;
    int bidPrice3 = -1;
    int bidVolume3 = 0;

    // Asks (-1 means empty level)
    int askPrice1 = -1;
    int askVolume1 = 0;
    int askPrice2 = -1;
    int askVolume2 = 0;
    int askPrice3 = -1;
    int askVolume3 = 0;

    double midPrice = 0.0;
    double profitAndLoss = 0.0;
};

struct HistoricalTrade {
    uint64_t timestamp;
    std::string buyer;
    std::string seller;
    std::string symbol;
    std::string currency;
    double price;
    int quantity;
};

class DataLoader {
public:
    static std::vector<BookSnapshot> loadMarketData(const std::string& filepath);
    static std::vector<HistoricalTrade> loadTradeData(const std::string& filepath);
};
