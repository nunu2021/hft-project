#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Callback function for libcurl
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

class KrakenOrderBook {
private:
    std::string symbol;
    std::string krakenSymbol;
    std::map<double, double, std::greater<double>> bids; // Price -> Quantity (descending)
    std::map<double, double> asks;                       // Price -> Quantity (ascending)
    
    std::string convertToKrakenSymbol(const std::string& input) {
        // Convert common symbols to Kraken format
        std::string result = input;
        
        // Common conversions
        if (result == "BTCUSDT" || result == "BTCUSD") return "XBTUSD";
        if (result == "ETHUSDT" || result == "ETHUSD") return "ETHUSD";
        if (result == "ADAUSDT" || result == "ADAUSD") return "ADAUSD";
        if (result == "DOTUSDT" || result == "DOTUSD") return "DOTUSD";
        if (result == "LINKUSDT" || result == "LINKUSD") return "LINKUSD";
        if (result == "LTCUSDT" || result == "LTCUSD") return "LTCUSD";
        if (result == "XRPUSDT" || result == "XRPUSD") return "XRPUSD";
        
        // If no conversion found, try as-is
        return result;
    }
    
public:
    KrakenOrderBook(const std::string& symbol) : symbol(symbol) {
        krakenSymbol = convertToKrakenSymbol(symbol);
    }
    
    bool fetchOrderBook() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return false;
        }
        
        std::string url = "https://api.kraken.com/0/public/Depth?pair=" + krakenSymbol + "&count=20";
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "OrderBook/1.0");
        

        
        CURLcode res = curl_easy_perform(curl);
        
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "Failed to fetch data: " << curl_easy_strerror(res) << std::endl;
            return false;
        }
        
        std::cout << "HTTP Response Code: " << response_code << std::endl;
        
        if (response_code != 200) {
            std::cerr << "HTTP Error: " << response_code << std::endl;
            std::cerr << "Response: " << response << std::endl;
            return false;
        }
        
        if (response.empty()) {
            std::cerr << "Empty response received" << std::endl;
            return false;
        }
        
        try {
            json data = json::parse(response);
            
            // Check for errors
            if (data.contains("error") && !data["error"].empty()) {
                std::cerr << "API Error: ";
                for (const auto& error : data["error"]) {
                    std::cerr << error << " ";
                }
                std::cerr << std::endl;
                return false;
            }
            
            // Clear existing data
            bids.clear();
            asks.clear();
            
            // Kraken returns data in result object with pair name as key
            if (!data.contains("result") || data["result"].empty()) {
                std::cerr << "No result data found" << std::endl;
                return false;
            }
            
            // Get the first (and should be only) pair data
            auto pairData = data["result"].begin().value();
            
            if (!pairData.contains("bids") || !pairData.contains("asks")) {
                std::cerr << "Response missing bids or asks data" << std::endl;
                return false;
            }
            
            std::cout << "Processing " << pairData["bids"].size() << " bids and " << pairData["asks"].size() << " asks" << std::endl;
            
            // Process bids - Kraken format: [price, volume, timestamp]
            for (const auto& bid : pairData["bids"]) {
                if (bid.size() >= 2) {
                    double price = std::stod(bid[0].get<std::string>());
                    double quantity = std::stod(bid[1].get<std::string>());
                    bids[price] = quantity;
                }
            }
            
            // Process asks
            for (const auto& ask : pairData["asks"]) {
                if (ask.size() >= 2) {
                    double price = std::stod(ask[0].get<std::string>());
                    double quantity = std::stod(ask[1].get<std::string>());
                    asks[price] = quantity;
                }
            }
            
            std::cout << "Loaded " << bids.size() << " bids and " << asks.size() << " asks" << std::endl;
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            std::cerr << "Raw response: " << response.substr(0, 500) << std::endl;
            return false;
        }
    }
    
    void display() {
        std::cout << "\033[2J\033[H"; // Clear screen
        std::cout << "=== Kraken Order Book for " << symbol << " (" << krakenSymbol << ") ===" << std::endl << std::endl;
        
        std::cout << std::left << std::setw(15) << "Bid Price" 
                  << std::setw(15) << "Bid Quantity" 
                  << std::setw(5) << " | "
                  << std::setw(15) << "Ask Price" 
                  << std::setw(15) << "Ask Quantity" << std::endl;
        
        std::cout << std::string(70, '-') << std::endl;
        
        auto bidIt = bids.begin();
        auto askIt = asks.begin();
        
        for (int i = 0; i < 15 && (bidIt != bids.end() || askIt != asks.end()); ++i) {
            // Print bid
            if (bidIt != bids.end()) {
                std::cout << std::fixed << std::setprecision(2);
                std::cout << std::left << std::setw(15) << bidIt->first
                          << std::setw(15) << std::setprecision(8) << bidIt->second;
                ++bidIt;
            } else {
                std::cout << std::setw(30) << " ";
            }
            
            std::cout << " | ";
            
            // Print ask
            if (askIt != asks.end()) {
                std::cout << std::fixed << std::setprecision(2);
                std::cout << std::left << std::setw(15) << askIt->first
                          << std::setw(15) << std::setprecision(8) << askIt->second;
                ++askIt;
            }
            
            std::cout << std::endl;
        }
        
        // Calculate spread
        if (!bids.empty() && !asks.empty()) {
            double bestBid = bids.begin()->first;
            double bestAsk = asks.begin()->first;
            double spread = bestAsk - bestBid;
            double spreadPercent = (spread / bestBid) * 100.0;
            
            std::cout << std::endl;
            std::cout << "Best Bid: $" << std::fixed << std::setprecision(2) << bestBid << std::endl;
            std::cout << "Best Ask: $" << std::fixed << std::setprecision(2) << bestAsk << std::endl;
            std::cout << "Spread: $" << std::fixed << std::setprecision(2) << spread 
                      << " (" << std::setprecision(4) << spreadPercent << "%)" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <SYMBOL>" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " BTCUSDT" << std::endl;
        std::cout << "  " << argv[0] << " ETHUSD" << std::endl;
        std::cout << "  " << argv[0] << " XBTUSD" << std::endl;
        return 1;
    }
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    std::string symbol = argv[1];
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
    
    KrakenOrderBook orderBook(symbol);
    
    std::cout << "Fetching order book for " << symbol << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl << std::endl;
    
    // Main loop - fetch and display order book every 3 seconds
    while (true) {
        if (orderBook.fetchOrderBook()) {
            orderBook.display();
        } else {
            std::cerr << "Failed to fetch order book data" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    curl_global_cleanup();
    return 0;
}