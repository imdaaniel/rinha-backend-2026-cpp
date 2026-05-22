#pragma once

#include <vector>
#include <string>
#include <memory>
#include <atomic>

namespace hnswlib {
    template<typename T> class SpaceInterface;
}

class IndexLoader {
public:
    IndexLoader();
    ~IndexLoader();
    
    // Load pre-built HNSW index from file
    bool load_index(const std::string& index_path);
    
    // Load reference metadata (labels)
    bool load_metadata(const std::string& metadata_path);
    
    // Search for k nearest neighbors and return fraud score
    float search_fraud_score(const float* query_vector, int k = 5);
    
    // Check if index is ready
    bool is_ready() const { return ready_.load(std::memory_order_acquire); }
    
    // Get reference count
    size_t reference_count() const { return reference_count_; }

private:
    void* index_{nullptr};  // hnswlib::HierarchicalNSW<float>*
    void* space_{nullptr};   // hnswlib::SpaceInterface<float>*
    std::vector<uint8_t> labels_;  // 0 = legit, 1 = fraud
    std::atomic<bool> ready_{false};
    size_t reference_count_{0};
    int dim_{14};
};
