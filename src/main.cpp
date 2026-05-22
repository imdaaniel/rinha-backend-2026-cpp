#include "vectorizer.h"
#include "index_loader.h"
#include "http_server.h"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <memory>
#include <thread>
#include <chrono>

std::unique_ptr<HttpServer> g_server;

void signal_handler(int signal) {
    (void)signal;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Get paths from environment or use defaults
    const char* index_path = std::getenv("INDEX_PATH");
    const char* metadata_path = std::getenv("METADATA_PATH");
    const char* mcc_risk_path = std::getenv("MCC_RISK_PATH");
    const char* normalization_path = std::getenv("NORMALIZATION_PATH");
    
    std::string idx_path = index_path ? index_path : "/app/data/hnsw_index.bin";
    std::string meta_path = metadata_path ? metadata_path : "/app/data/references.bin";
    std::string mcc_path = mcc_risk_path ? mcc_risk_path : "/app/resources/mcc_risk.json";
    std::string norm_path = normalization_path ? normalization_path : "/app/resources/normalization.json";
    
    std::cout << "Starting Rinha C++ API" << std::endl;
    std::cout << "Index path: " << idx_path << std::endl;
    std::cout << "Metadata path: " << meta_path << std::endl;
    
    // Initialize vectorizer
    auto vectorizer = std::make_unique<Vectorizer>();
    vectorizer->load_mcc_risk(mcc_path);
    vectorizer->load_normalization(norm_path);
    std::cout << "Vectorizer initialized" << std::endl;
    
    // Initialize index loader
    auto index_loader = std::make_unique<IndexLoader>();
    
    // Load metadata first (labels)
    if (!index_loader->load_metadata(meta_path)) {
        std::cerr << "Failed to load metadata from " << meta_path << std::endl;
        return 1;
    }
    std::cout << "Loaded " << index_loader->reference_count() << " references" << std::endl;
    
    // Load HNSW index
    if (!index_loader->load_index(idx_path)) {
        std::cerr << "Failed to load index from " << idx_path << std::endl;
        std::cerr << "Continuing without index - will return default responses" << std::endl;
        // Don't return 1, continue without index for now
    }
    
    // Start HTTP server
    int port = 9999;
    g_server = std::make_unique<HttpServer>(port, index_loader.get(), vectorizer.get());
    
    if (!g_server->start()) {
        return 1;
    }
    
    std::cout << "Server ready, waiting for requests..." << std::endl;
    
    // Wait forever (server runs in background threads)
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
