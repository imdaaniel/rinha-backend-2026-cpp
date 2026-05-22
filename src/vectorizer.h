#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <optional>

struct LastTransaction {
    std::string timestamp;
    double km_from_current;
};

struct Payload {
    std::string id;
    struct {
        double amount;
        int installments;
        std::string requested_at;
    } transaction;
    struct {
        double avg_amount;
        int tx_count_24h;
        std::vector<std::string> known_merchants;
    } customer;
    struct {
        std::string id;
        std::string mcc;
        double avg_amount;
    } merchant;
    struct {
        bool is_online;
        bool card_present;
        double km_from_home;
    } terminal;
    std::optional<LastTransaction> last_transaction;
};

class Vectorizer {
public:
    static constexpr int DIM = 14;
    using Vector = std::array<float, DIM>;

    Vectorizer();
    
    void load_mcc_risk(const std::string& path);
    void load_normalization(const std::string& path);
    
    Vector vectorize(const Payload& p) const;
    void vectorize_to(const Payload& p, Vector& out) const;

private:
    static float clamp(double x);
    static bool contains(const std::vector<std::string>& vec, const std::string& val);
    static int parse_hour(const std::string& iso_time);
    static int parse_day_of_week(const std::string& iso_time);
    static double parse_minutes_diff(const std::string& iso_time1, const std::string& iso_time2);

    // Normalization constants
    double max_amount_{10000.0};
    double max_installments_{12.0};
    double amount_vs_avg_ratio_{10.0};
    double max_minutes_{1440.0};
    double max_km_{1000.0};
    double max_tx_count_24h_{20.0};
    double max_merchant_avg_amount_{10000.0};
    
    // MCC risk map
    std::unordered_map<std::string, float> mcc_risk_;
    float mcc_risk_default_{0.5f};
};
