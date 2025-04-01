

#include <iostream>
#include <chrono>
#include <curl/curl.h>
#include <string>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

struct Symbol {
    std::string baseAsset;
    std::string quoteAsset;
    std::string symbol;
    double makerFee;
    double takerFee;
    bool isActive;
};

std::vector<Symbol> get_all_symbols(const std::string& url, const std::string& bearer_token) {
    CURL* curl;
    CURLcode res;
    std::string response;
    std::vector<Symbol> symbols;

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL initialization failed\n";
        return symbols;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + bearer_token).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200) {
            try {
                json j = json::parse(response);
                for (const auto& item : j) {
                    Symbol sym;
                    sym.baseAsset = item.value("baseAsset", "");
                    sym.quoteAsset = item.value("quoteAsset", "");
                    sym.symbol = item.value("symbol", "");
                    sym.makerFee = item.value("makerFee", 0.0);
                    sym.takerFee = item.value("takerFee", 0.0);
                    sym.isActive = item.value("isActive", false);
                    symbols.push_back(sym);
                }
            } catch (const json::exception& e) {
                std::cerr << "JSON parsing error: " << e.what() << "\n";
            }
        } else {
            std::cerr << "Failed to fetch symbols. HTTP code: " << http_code << ". Response: " << response << "\n";
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return symbols;
}

double measure_api_latency(const std::string& url, const std::string& post_data, const std::string& bearer_token) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL initialization failed\n";
        return -1;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + bearer_token).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::cout << "Sending order: " << post_data << "\n";

    auto start = std::chrono::high_resolution_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    auto end = std::chrono::high_resolution_clock::now();

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code == 201) {
        std::cout << "Order created successfully. Response: " << response << "\n";
    } else {
        std::cerr << "Order failed with HTTP code " << http_code << ". Response: " << response << "\n";
        return -1;
    }

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count();
}

int main() {
    std::string bearer_token = "FyzTCpEy1pDffMLP02DP0chRHqosgGxHvoHDtb8TpsKKNkBbA0SqdD4XZatPUUocnY936yfvyk0f33vROxsesGYA1okiPtOTC6JeqISNgcX640UkB9j8mAZxe13zwsxe";

    std::string symbols_url = "https://api.mazdax.ir/market/symbols";
    std::vector<Symbol> symbols = get_all_symbols(symbols_url, bearer_token);

    std::cout << "Retrieved " << symbols.size() << " symbols:\n";
    for (const auto& sym : symbols) {
        std::cout << "Symbol: " << sym.symbol << ", Base: " << sym.baseAsset
                  << ", Quote: " << sym.quoteAsset << ", Active: " << (sym.isActive ? "true" : "false")
                  << ", MakerFee: " << sym.makerFee << ", TakerFee: " << sym.takerFee << "\n";
    }

    std::string order_url = "https://api.mazdax.ir/orders";
    std::string default_symbol = "AHRM1IRR"; // Fallback if no symbols retrieved
    double total_amount = 60000.0; 

    if (!symbols.empty()) {
        for (const auto& sym : symbols) {
            if (sym.isActive && sym.quoteAsset == "IRR") { // just IRR token
                default_symbol = sym.symbol;
                break;
            }
        }
    }

    std::string post_data = R"({"orderType":"market","side":"BUY","symbol":")" + default_symbol + R"(","totalAmount":)" + std::to_string(total_amount) + "}";

    int trials = 5;
    double total_latency = 0.0;
    int successful_trials = 0;

    for (int i = 0; i < trials; ++i) {
        std::cout << "\nRunning trial " << i + 1 << "...\n";
        double latency = measure_api_latency(order_url, post_data, bearer_token);
        if (latency >= 0) {
            std::cout << "Trial " << i + 1 << ": " << latency << " microseconds\n";
            total_latency += latency;
            ++successful_trials;
        } else {
            std::cout << "Trial " << i + 1 << ": Failed\n";
        }
    }

    if (successful_trials > 0) {
        std::cout << "\nAverage latency: " << total_latency / successful_trials << " microseconds\n";
    } else {
        std::cout << "\nNo successful trials completed.\n";
    }

    return 0;
}
