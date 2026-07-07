#include "DataLoader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// Helper function to split a string by delimiter, keeping empty tokens
static std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delim);
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delim, start);
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

// Helpers to handle empty/blank fields safely
static int parseOptionalInt(const std::string& token) {
    if (token.empty()) return -1;
    return std::stoi(token);
}

static double parseOptionalDouble(const std::string& token) {
    if (token.empty()) return 0.0;
    return std::stod(token);
}

std::vector<BookSnapshot> DataLoader::loadMarketData(const std::string& filepath) {
    std::vector<BookSnapshot> snapshots;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Could not open market data file: " << filepath << "\n";
        return snapshots;
    }

    std::string line;
    // Read/Skip header line
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<std::string> row = split(line, ';');
        if (row.size() < 17) continue;

        BookSnapshot snap;
        snap.day = std::stoi(row[0]);
        snap.timestamp = std::stoull(row[1]);
        snap.product = row[2];

        snap.bidPrice1  = parseOptionalInt(row[3]);
        snap.bidVolume1 = parseOptionalInt(row[4]);
        snap.bidPrice2  = parseOptionalInt(row[5]);
        snap.bidVolume2 = parseOptionalInt(row[6]);
        snap.bidPrice3  = parseOptionalInt(row[7]);
        snap.bidVolume3 = parseOptionalInt(row[8]);

        snap.askPrice1  = parseOptionalInt(row[9]);
        snap.askVolume1 = parseOptionalInt(row[10]);
        snap.askPrice2  = parseOptionalInt(row[11]);
        snap.askVolume2 = parseOptionalInt(row[12]);
        snap.askPrice3  = parseOptionalInt(row[13]);
        snap.askVolume3 = parseOptionalInt(row[14]);

        snap.midPrice = parseOptionalDouble(row[15]);
        snap.profitAndLoss = parseOptionalDouble(row[16]);

        snapshots.push_back(snap);
    }
    return snapshots;
}

std::vector<HistoricalTrade> DataLoader::loadTradeData(const std::string& filepath) {
    std::vector<HistoricalTrade> trades;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Could not open trade data file: " << filepath << "\n";
        return trades;
    }

    std::string line;
    // Read/Skip header line
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<std::string> row = split(line, ';');
        if (row.size() < 7) continue;

        HistoricalTrade trade;
        trade.timestamp = std::stoull(row[0]);
        trade.buyer = row[1];
        trade.seller = row[2];
        trade.symbol = row[3];
        trade.currency = row[4];
        trade.price = parseOptionalDouble(row[5]);
        trade.quantity = parseOptionalInt(row[6]);

        trades.push_back(trade);
    }
    return trades;
}
