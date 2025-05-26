#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <string>
#include <ctime>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

#define SHA256_DIGEST_LENGTH 32

using json = nlohmann::json;

std::string base64Decode(const std::string &in) {
    BIO *bio, *b64;
    int decodeLen = in.length() * 3 / 4;
    std::vector<char> buffer(decodeLen);

    bio = BIO_new_mem_buf(in.data(), in.length());
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines
    bio = BIO_push(b64, bio);
    int len = BIO_read(bio, buffer.data(), in.length());
    BIO_free_all(bio);

    return std::string(buffer.data(), len);
}

std::string base64Encode(const unsigned char* buffer, size_t length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return result;
}

std::string generateSignature(const std::string &secret, const std::string &timestamp,
                              const std::string &method, const std::string &request_path,
                              const std::string &body) {
    std::string prehash = timestamp + method + request_path + body;
    std::string decodedSecret = base64Decode(secret);

    unsigned char* digest;
    digest = HMAC(EVP_sha256(), decodedSecret.data(), decodedSecret.size(),
                  reinterpret_cast<const unsigned char*>(prehash.c_str()), prehash.size(), NULL, NULL);

    return base64Encode(digest, SHA256_DIGEST_LENGTH);
}

const std::string API_KEY = "organizations/6bda13bd-9218-4a55-9091-d95fcb69c9e8/apiKeys/6d4809cc-1969-4805-95ce-25ce5508799c";
const std::string SECRET = "-----BEGIN EC PRIVATE KEY-----\nMHcCAQEEIDVrWbBj2HlO+X1aMYLFgpYENt/84sKGY8fotfhB/Q6BoAoGCCqGSM49\nAwEHoUQDQgAEvXAROT3pHY5oH+daC3W0Oc/4JAomY+ei15GYL5MeA3H4S30YJoVL\nTzAaSp5uCy+MyIKuzfaLJ5qpNW3PI+UX6w==\n-----END EC PRIVATE KEY-----\n";
const std::string PASSPHRASE = "";


class FastOrderBook {
private:
    std::string symbol;
    std::map<double, double, std::greater<double>> bids;
    std::map<double, double> asks;
    std::mutex mtx;
    std::atomic<bool> running{false};
    std::atomic<int> updateCount{0};
    std::chrono::steady_clock::time_point lastUpdate;
    
public:
    FastOrderBook(const std::string& sym) : symbol(sym) {
        lastUpdate = std::chrono::steady_clock::now();
    }
    
    void updateOrderBook(const json& data) {
        std::lock_guard<std::mutex> lock(mtx);
        
        try {
            // Handle Coinbase WebSocket format
            if (data.contains("type") && data["type"] == "l2update") {
                // Process changes
                if (data.contains("changes")) {
                    for (const auto& change : data["changes"]) {
                        if (change.size() >= 3) {
                            std::string side = change[0];
                            double price = std::stod(change[1].get<std::string>());
                            double size = std::stod(change[2].get<std::string>());
                            
                            if (side == "buy") {
                                if (size == 0.0) {
                                    bids.erase(price);
                                } else {
                                    bids[price] = size;
                                }
                            } else if (side == "sell") {
                                if (size == 0.0) {
                                    asks.erase(price);
                                } else {
                                    asks[price] = size;
                                }
                            }
                        }
                    }
                }
            }
            // Handle Coinbase snapshot
            else if (data.contains("type") && data["type"] == "snapshot") {
                bids.clear();
                asks.clear();
                
                if (data.contains("bids")) {
                    for (const auto& bid : data["bids"]) {
                        if (bid.size() >= 2) {
                            double price = std::stod(bid[0].get<std::string>());
                            double size = std::stod(bid[1].get<std::string>());
                            bids[price] = size;
                        }
                    }
                }
                
                if (data.contains("asks")) {
                    for (const auto& ask : data["asks"]) {
                        if (ask.size() >= 2) {
                            double price = std::stod(ask[0].get<std::string>());
                            double size = std::stod(ask[1].get<std::string>());
                            asks[price] = size;
                        }
                    }
                }
            }
            
            updateCount++;
            lastUpdate = std::chrono::steady_clock::now();
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing update: " << e.what() << std::endl;
        }
    }
    
    void display() {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto now = std::chrono::steady_clock::now();
        auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
        
        std::cout << "\033[2J\033[H"; // Clear screen
        std::cout << "=== FAST Order Book for " << symbol << " ===" << std::endl;
        std::cout << "Updates: " << updateCount << " | Last update: " << timeSinceUpdate << "ms ago" << std::endl;
        std::cout << "Connected via WebSocket - Real-time updates" << std::endl << std::endl;
        
        std::cout << std::left << std::setw(15) << "Bid Price" 
                  << std::setw(15) << "Bid Size" 
                  << std::setw(5) << " | "
                  << std::setw(15) << "Ask Price" 
                  << std::setw(15) << "Ask Size" << std::endl;
        
        std::cout << std::string(70, '-') << std::endl;
        
        auto bidIt = bids.begin();
        auto askIt = asks.begin();
        
        for (int i = 0; i < 20 && (bidIt != bids.end() || askIt != asks.end()); ++i) {
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
    
    void setRunning(bool state) { running = state; }
    bool isRunning() const { return running; }
};

// WebSocket connection using websocat in a more efficient way
class WebSocketClient {
private:
    std::string symbol;
    FastOrderBook& orderBook;
    std::thread wsThread;
    std::atomic<bool> running{false};
    
public:
    WebSocketClient(const std::string& sym, FastOrderBook& ob) 
        : symbol(sym), orderBook(ob) {}
    
    bool start() {
        if (running) return true;
        
        running = true;
        orderBook.setRunning(true);
        
        wsThread = std::thread([this]() {
            // Convert symbol for Coinbase (BTC-USD format)
            std::string cbSymbol = symbol;
            if (symbol == "BTCUSDT" || symbol == "BTCUSD") cbSymbol = "BTC-USD";
            else if (symbol == "ETHUSDT" || symbol == "ETHUSD") cbSymbol = "ETH-USD";
            else if (symbol == "ADAUSDT" || symbol == "ADAUSD") cbSymbol = "ADA-USD";

            std::string timestamp = std::to_string(std::time(nullptr));
            std::string method = "GET";
            std::string request_path = "/users/self/verify";
            std::string body = "";

            std::string signature = generateSignature(SECRET, timestamp, method, request_path, body);
            
            // Create WebSocket subscription message
            json subscribe = {
                {"type", "subscribe"},
                {"product_ids", {cbSymbol}},
                {"channels", {"level2"}},
                {"key", API_KEY},
                {"passphrase", PASSPHRASE},
                {"timestamp", timestamp},
                {"signature", signature}
            };
            
            std::string subscribeMsg = subscribe.dump();
            
            // Create a named pipe for better performance
            std::string pipeName = "/tmp/orderbook_" + std::to_string(getpid());
            std::string mkfifoCmd = "mkfifo " + pipeName;
            system(mkfifoCmd.c_str());
            
            // Start websocat with subscription
            std::ostringstream cmd;
            cmd << "echo '" << subscribeMsg << "' | websocat wss://ws-feed.exchange.coinbase.com > " << pipeName << " &";
            
            std::cout << "Starting WebSocket connection..." << std::endl;
            system(cmd.str().c_str());
            
            // Read from the pipe
            std::ifstream pipe(pipeName);
            std::string line;
            
            while (running && std::getline(pipe, line)) {
                if (!line.empty()) {
                    std::cout << line << std::endl;
                    try {
                        json data = json::parse(line);
                        orderBook.updateOrderBook(data);
                    } catch (const std::exception& e) {
                        // Skip invalid JSON lines
                    }
                }
            }
            
            // Cleanup
            pipe.close();
            unlink(pipeName.c_str());
        });
        
        return true;
    }
    
    void stop() {
        if (!running) return;
        
        running = false;
        orderBook.setRunning(false);
        
        // Kill websocat processes
        system("pkill -f websocat");
        
        if (wsThread.joinable()) {
            wsThread.join();
        }
    }
    
    ~WebSocketClient() {
        stop();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <SYMBOL>" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " BTCUSDT" << std::endl;
        std::cout << "  " << argv[0] << " ETHUSDT" << std::endl;
        return 1;
    }
    
    std::string symbol = argv[1];
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
    
    FastOrderBook orderBook(symbol);
    WebSocketClient wsClient(symbol, orderBook);
    
    std::cout << "Starting FAST order book for " << symbol << std::endl;
    std::cout << "This uses WebSocket for real-time updates!" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl << std::endl;
    
    if (!wsClient.start()) {
        std::cerr << "Failed to start WebSocket client" << std::endl;
        return 1;
    }
    
    // Give WebSocket time to connect and get initial data
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Display loop - much faster updates
    while (orderBook.isRunning()) {
        orderBook.display();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Update display 10x per second
    }
    
    wsClient.stop();
    return 0;
}