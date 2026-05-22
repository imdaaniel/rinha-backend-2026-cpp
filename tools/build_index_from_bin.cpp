#include <hnswlib/hnswlib.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

int main(int argc, char** argv) {
    const char* bin_path = argc > 1 ? argv[1] : "data/references.bin";
    const char* index_path = argc > 2 ? argv[2] : "data/hnsw_index.bin";
    
    int dim = 14;
    int M = 16;
    int ef_construction = 100;
    
    std::cout << "Building HNSW index from binary file" << std::endl;
    std::cout << "Binary path: " << bin_path << std::endl;
    std::cout << "Index path: " << index_path << std::endl;
    
    // Read binary file
    std::ifstream fin(bin_path, std::ios::binary);
    if (!fin) {
        std::cerr << "Failed to open binary file" << std::endl;
        return 1;
    }
    
    // Read header
    char magic[4];
    uint16_t version, dim16;
    uint32_t count, label_data_size;
    
    fin.read(magic, 4);
    fin.read(reinterpret_cast<char*>(&version), 2);
    fin.read(reinterpret_cast<char*>(&dim16), 2);
    fin.read(reinterpret_cast<char*>(&count), 4);
    fin.read(reinterpret_cast<char*>(&label_data_size), 4);
    
    if (std::strncmp(magic, "RINH", 4) != 0) {
        std::cerr << "Invalid magic number" << std::endl;
        return 1;
    }
    
    std::cout << "Found " << count << " vectors of dimension " << dim16 << std::endl;
    
    // Skip label section
    fin.seekg(label_data_size, std::ios::cur);
    
    // Calculate padding
    size_t header_size = 16;
    size_t padding = (4 - ((header_size + label_data_size) % 4)) % 4;
    fin.seekg(padding, std::ios::cur);
    
    // Read vectors
    std::vector<float> data(count * dim);
    fin.read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(float));
    fin.close();
    
    std::cout << "Read " << data.size() << " floats" << std::endl;
    
    // Build HNSW index
    std::cout << "Building HNSW index (M=" << M << ", ef_construction=" << ef_construction << ")" << std::endl;
    
    hnswlib::L2Space space(dim);
    hnswlib::HierarchicalNSW<float> index(&space, count, M, ef_construction);
    
    for (size_t i = 0; i < count; i++) {
        index.addPoint(data.data() + i * dim, i);
        
        if (i % 100000 == 0 && i > 0) {
            std::cout << "Inserted " << i << "/" << count << " vectors" << std::endl;
        }
    }
    
    std::cout << "Saving index to " << index_path << std::endl;
    index.saveIndex(index_path);
    
    std::cout << "Done!" << std::endl;
    return 0;
}
