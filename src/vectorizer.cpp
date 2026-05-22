#include "vectorizer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

using json = nlohmann::json;

Vectorizer::Vectorizer() {
    // Initialize with default MCC risk values
    mcc_risk_ = {
        {"5411", 0.15f},
        {"5812", 0.30f},
        {"5912", 0.20f},
        {"5944", 0.45f},
        {"7801", 0.80f},
        {"7802", 0.75f},
        {"7995", 0.85f},
        {"4511", 0.35f},
        {"5311", 0.25f},
        {"5999", 0.50f}
    };
}

void Vectorizer::load_mcc_risk(const std::string& path) {
    try {
        std::ifstream f(path);
        json j = json::parse(f);
        mcc_risk_.clear();
        for (auto& [key, val] : j.items()) {
            mcc_risk_[key] = val.get<float>();
        }
    } catch (...) {
        // Keep defaults if loading fails
    }
}

void Vectorizer::load_normalization(const std::string& path) {
    try {
        std::ifstream f(path);
        json j = json::parse(f);
        if (j.contains("max_amount")) max_amount_ = j["max_amount"];
        if (j.contains("max_installments")) max_installments_ = j["max_installments"];
        if (j.contains("amount_vs_avg_ratio")) amount_vs_avg_ratio_ = j["amount_vs_avg_ratio"];
        if (j.contains("max_minutes")) max_minutes_ = j["max_minutes"];
        if (j.contains("max_km")) max_km_ = j["max_km"];
        if (j.contains("max_tx_count_24h")) max_tx_count_24h_ = j["max_tx_count_24h"];
        if (j.contains("max_merchant_avg_amount")) max_merchant_avg_amount_ = j["max_merchant_avg_amount"];
    } catch (...) {
        // Keep defaults if loading fails
    }
}

float Vectorizer::clamp(double x) {
    if (std::isnan(x) || std::isinf(x)) return 0.0f;
    if (x < 0.0) return 0.0f;
    if (x > 1.0) return 1.0f;
    return static_cast<float>(x);
}

bool Vectorizer::contains(const std::vector<std::string>& vec, const std::string& val) {
    return std::find(vec.begin(), vec.end(), val) != vec.end();
}

int Vectorizer::parse_hour(const std::string& iso_time) {
    // ISO 8601 format: 2026-03-11T18:45:53Z
    // Hour is at position 11-12
    if (iso_time.length() >= 13) {
        return std::stoi(iso_time.substr(11, 2));
    }
    return 0;
}

int Vectorizer::parse_day_of_week(const std::string& iso_time) {
    // Parse ISO 8601 time and get day of week (Mon=0..Sun=6)
    try {
        std::tm tm = {};
        std::istringstream ss(iso_time);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (ss.fail()) {
            // Try with Z suffix
            std::string tz = iso_time;
            if (!tz.empty() && tz.back() == 'Z') tz.pop_back();
            std::istringstream ss2(tz);
            ss2 >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        }
        
        // Convert to time_t
        std::time_t time = std::mktime(&tm);
        std::tm* local_tm = std::gmtime(&time);
        
        int dow = local_tm->tm_wday; // Sunday=0
        // Convert to Monday=0..Sunday=6
        if (dow == 0) return 6;
        return dow - 1;
    } catch (...) {
        return 0;
    }
}

double Vectorizer::parse_minutes_diff(const std::string& iso_time1, const std::string& iso_time2) {
    try {
        std::tm tm1 = {}, tm2 = {};
        std::istringstream ss1(iso_time1), ss2(iso_time2);
        ss1 >> std::get_time(&tm1, "%Y-%m-%dT%H:%M:%S");
        ss2 >> std::get_time(&tm2, "%Y-%m-%dT%H:%M:%S");
        
        std::time_t time1 = std::mktime(&tm1);
        std::time_t time2 = std::mktime(&tm2);
        
        double diff = std::difftime(time1, time2) / 60.0; // minutes
        return std::abs(diff);
    } catch (...) {
        return 0.0;
    }
}

Vectorizer::Vector Vectorizer::vectorize(const Payload& p) const {
    Vector v;
    vectorize_to(p, v);
    return v;
}

void Vectorizer::vectorize_to(const Payload& p, Vector& v) const {
    // dim 0: amount
    v[0] = clamp(p.transaction.amount / max_amount_);
    
    // dim 1: installments
    v[1] = clamp(static_cast<double>(p.transaction.installments) / max_installments_);
    
    // dim 2: amount_vs_avg
    double ratio = 0.0;
    if (p.customer.avg_amount > 0) {
        ratio = (p.transaction.amount / p.customer.avg_amount) / amount_vs_avg_ratio_;
    }
    v[2] = clamp(ratio);
    
    // dim 3: hour_of_day
    int hour = parse_hour(p.transaction.requested_at);
    v[3] = clamp(static_cast<double>(hour) / 23.0);
    
    // dim 4: day_of_week
    int dow = parse_day_of_week(p.transaction.requested_at);
    v[4] = clamp(static_cast<double>(dow) / 6.0);
    
    // dim 5 & 6: minutes_since_last_tx, km_from_last_tx or -1
    if (!p.last_transaction.has_value()) {
        v[5] = -1.0f;
        v[6] = -1.0f;
    } else {
        double minutes = parse_minutes_diff(p.transaction.requested_at, p.last_transaction->timestamp);
        v[5] = clamp(minutes / max_minutes_);
        v[6] = clamp(p.last_transaction->km_from_current / max_km_);
    }
    
    // dim 7: km_from_home
    v[7] = clamp(p.terminal.km_from_home / max_km_);
    
    // dim 8: tx_count_24h
    v[8] = clamp(static_cast<double>(p.customer.tx_count_24h) / max_tx_count_24h_);
    
    // dim 9: is_online
    v[9] = p.terminal.is_online ? 1.0f : 0.0f;
    
    // dim 10: card_present
    v[10] = p.terminal.card_present ? 1.0f : 0.0f;
    
    // dim 11: unknown_merchant (1 = unknown)
    v[11] = contains(p.customer.known_merchants, p.merchant.id) ? 0.0f : 1.0f;
    
    // dim 12: mcc_risk
    auto it = mcc_risk_.find(p.merchant.mcc);
    v[12] = (it != mcc_risk_.end()) ? it->second : mcc_risk_default_;
    
    // dim 13: merchant_avg_amount
    v[13] = clamp(p.merchant.avg_amount / max_merchant_avg_amount_);
}
