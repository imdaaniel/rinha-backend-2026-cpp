#include <hnswlib/hnswlib.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <cstring>

using json = nlohmann::json;

int main(int argc, char** argv) {
    const char* json_path = argc > 1 ? argv[1] : "../base/resources/references.json.gz";
    const char* bin_path = argc > 2 ? argv[2] : "data/references.bin";
    const char* index_path = argc > 3 ? argv[3] : "data/hnsw_index.bin";
    
    int dim = 14;
    int M = 16;
    int ef_construction = 100;
    
    std::cout << "Building HNSW index" << std::endl;
    std::cout << "JSON path: " << json_path << std::endl;
    std::cout << "Binary path: " << bin_path << std::endl;
    std::cout << "Index path: " << index_path << std::endl;
    
    // Read and decompress JSON
    gzFile gz = gzopen(json_path, "rb");
    if (!gz) {
        std::cerr << "Failed to open " << json_path << std::endl;
        return 1;
    }
    
    std::vector<char> buffer;
    char chunk[8192];
    int bytes_read;
    while ((bytes_read = gzread(gz, chunk, sizeof(chunk))) > 0) {
        buffer.insert(buffer.end(), chunk, chunk + bytes_read);
    }
    gzclose(gz);
    
    std::cout << "Decompressed " << buffer.size() << " bytes" << std::endl;
    
    // Parse JSON
    json j = json::parse(buffer.begin(), buffer.end());
    if (!j.is_array()) {
        std::cerr << "JSON is not an array" << std::endl;
        return 1;
    }
    
    size_t count = j.size();
    std::cout << "Loaded " << count << " references" << std::endl;
    
    // Build binary format
    std::vector<char> label_data;
    std::vector<float> vectors;
    vectors.reserve(count * dim);
    
    for (size_t i = 0; i < count; i++) {
        std::string label = j[i]["label"];
        uint16_t len = static_cast<uint16_t>(label.size());
        label_data.insert(label_data.end(), 
                        reinterpret_cast<char*>(&len),
                        reinterpret_cast<char*>(&len) + 2);
        label_data.insert(label_data.end(), label.begin(), label.end());
        
        auto vec = j[i]["vector"].get<std::vector<float>>();
        for (int d = 0; d < dim; d++) {
            float val = (d < vec.size()) ? vec[d] : 0.0f;
            vectors.push_back(val);
        }
        
        if (i % 100000 == 0 && i > 0) {
            std::cout << "Processed " << i << "/" << count << " references" << std::endl;
        }
    }
    
    // Write binary file
    std::ofstream out(bin_path, std::ios::binary);
    
    char magic[4] = {'R', 'I', 'N', 'H'};
    uint16_t version = 1;
    uint16_t dim16 = dim;
    uint32_t count32 = count;
    uint32_t label_data_size = label_data.size();
    
    out.write(magic, 4);
    out.write(reinterpret_cast<const char*>(&version), 2);
    out.write(reinterpret_cast<const char*>(&dim16), 2);
    out.write(reinterpret_cast<const char*>(&count32), 4);
    out.write(reinterpret_cast<const char*>(&label_data_size), 4);
    
    out.write(label_data.data(), label_data.size());
    
    size_t padding = (4 - ((16 + label_data_size) % 4)) % 4;
    if (padding > 0) {
        std::vector<char> pad(padding, 0);
        out.write(pad.data(), padding);
    }
    
    out.write(reinterpret_cast<const char*>(vectors.data()), vectors.size() * sizeof(float));
    out.close();
    
    std::cout << "Binary file written to " << bin_path << std::endl;
    
    // Build HNSW index
    std::cout << "Building HNSW index (M=" << M << ", ef_construction=" << ef_construction << ")" << std::endl;
    
    hnswlib::L2Space space(dim);
    hnswlib::HierarchicalNSW<float> index(&space, count, M, ef_construction);
    
    for (size_t i = 0; i < count; i++) {
        index.addPoint(vectors.data() + i * dim, i);
        
        if (i % 100000 == 0 && i > 0) {
            std::cout << "Inserted " << i << "/" << count << " vectors" << std::endl;
        }
    }
    
    std::cout << "Saving index to " << index_path << std::endl;
    index.saveIndex(index_path);
    
    std::cout << "Done!" << std::endl;
    return 0;
}
