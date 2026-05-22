#include "index_loader.h"
#include "metadata.h"
#include "hnswlib/hnswlib.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <queue>

IndexLoader::IndexLoader() {}

IndexLoader::~IndexLoader() {
    if (index_) {
        delete static_cast<hnswlib::HierarchicalNSW<float>*>(index_);
        index_ = nullptr;
    }
    if (space_) {
        delete static_cast<hnswlib::SpaceInterface<float>*>(space_);
        space_ = nullptr;
    }
}

bool IndexLoader::load_index(const std::string& index_path) {
    try {
        // Load HNSW index
        std::cout << "Loading HNSW index from " << index_path << std::endl;
        
        // Check if file exists
        std::ifstream f(index_path, std::ios::binary);
        if (!f) {
            std::cerr << "Index file does not exist: " << index_path << std::endl;
            return false;
        }
        f.close();
        
        int dim = 14;
        auto* space = new hnswlib::L2Space(dim);
        auto* index = new hnswlib::HierarchicalNSW<float>(space, index_path, false);
        
        // Configure search quality: efSearch=50 for 3M vectors
        index->setEf(50);
        
        space_ = space;
        index_ = index;
        ready_.store(true, std::memory_order_release);
        std::cout << "Index loaded successfully, ef=" << index->ef_ << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load index: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Failed to load index: unknown error" << std::endl;
        return false;
    }
}

bool IndexLoader::load_metadata(const std::string& metadata_path) {
    // Load labels from binary file
    return MetadataLoader::load_labels(metadata_path, labels_, reference_count_);
}

float IndexLoader::search_fraud_score(const float* query_vector, int k) {
    if (!is_ready() || !index_) {
        return 0.0f; // Default to legit if not ready
    }
    
    auto* index = static_cast<hnswlib::HierarchicalNSW<float>*>(index_);
    
    // Search for k nearest neighbors
    std::priority_queue<std::pair<float, hnswlib::labeltype>> result;
    result = index->searchKnn(query_vector, k);
    
    // Count frauds among top k
    int frauds = 0;
    int found = 0;
    
    while (!result.empty() && found < k) {
        auto item = result.top();
        result.pop();
        
        hnswlib::labeltype label = item.second;
        if (label >= 0 && static_cast<size_t>(label) < labels_.size()) {
            if (labels_[label] == 1) {
                frauds++;
            }
            found++;
        }
    }
    
    if (found == 0) return 0.0f;
    return static_cast<float>(frauds) / static_cast<float>(found);
}
