#include "metadata.h"
#include <fstream>
#include <cstring>
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <iostream>

using json = nlohmann::json;

bool MetadataLoader::load_labels(const std::string& bin_path, 
                                  std::vector<uint8_t>& labels,
                                  size_t& count) {
    std::ifstream f(bin_path, std::ios::binary);
    if (!f) return false;
    
    // Read header
    BinaryHeader header;
    f.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (std::strncmp(header.magic, "RINH", 4) != 0) {
        std::cerr << "Invalid magic number" << std::endl;
        return false;
    }
    
    count = header.count;
    
    // Read label section
    std::vector<char> label_data(header.label_data_size);
    f.read(label_data.data(), header.label_data_size);
    
    // Parse labels
    labels.resize(count);
    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        if (pos + 2 > label_data.size()) return false;
        uint16_t len = *reinterpret_cast<uint16_t*>(label_data.data() + pos);
        pos += 2;
        
        if (pos + len > label_data.size()) return false;
        std::string label(label_data.data() + pos, len);
        pos += len;
        
        // Check if label is "fraud"
        labels[i] = (label == "fraud") ? 1 : 0;
    }
    
    return true;
}

bool MetadataLoader::convert_json_to_bin(const std::string& json_path,
                                           const std::string& bin_path) {
    // Open JSON file
    gzFile gz = gzopen(json_path.c_str(), "rb");
    if (!gz) {
        std::cerr << "Failed to open " << json_path << std::endl;
        return false;
    }
    
    // Read and decompress
    std::vector<char> buffer;
    char chunk[8192];
    int bytes_read;
    while ((bytes_read = gzread(gz, chunk, sizeof(chunk))) > 0) {
        buffer.insert(buffer.end(), chunk, chunk + bytes_read);
    }
    gzclose(gz);
    
    // Parse JSON
    try {
        json j = json::parse(buffer.begin(), buffer.end());
        if (!j.is_array()) {
            std::cerr << "JSON is not an array" << std::endl;
            return false;
        }
        
        size_t count = j.size();
        int dim = 14;
        
        // Build label section
        std::vector<char> label_data;
        for (size_t i = 0; i < count; i++) {
            std::string label = j[i]["label"];
            uint16_t len = static_cast<uint16_t>(label.size());
            label_data.insert(label_data.end(), 
                            reinterpret_cast<char*>(&len),
                            reinterpret_cast<char*>(&len) + 2);
            label_data.insert(label_data.end(), label.begin(), label.end());
        }
        
        // Calculate vector offset (4-byte aligned)
        size_t header_size = 16;
        size_t label_data_size = label_data.size();
        size_t padding = (4 - ((header_size + label_data_size) % 4)) % 4;
        size_t vector_offset = header_size + label_data_size + padding;
        
        // Write binary file
        std::ofstream out(bin_path, std::ios::binary);
        
        // Write header placeholder
        BinaryHeader header;
        std::memcpy(header.magic, "RINH", 4);
        header.version = 1;
        header.dim = dim;
        header.count = count;
        header.label_data_size = label_data_size;
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Write label data
        out.write(label_data.data(), label_data_size);
        
        // Write padding
        if (padding > 0) {
            std::vector<char> pad(padding, 0);
            out.write(pad.data(), padding);
        }
        
        // Write vectors
        for (size_t i = 0; i < count; i++) {
            auto vec = j[i]["vector"].get<std::vector<float>>();
            for (int d = 0; d < dim; d++) {
                float val = (d < vec.size()) ? vec[d] : 0.0f;
                out.write(reinterpret_cast<const char*>(&val), sizeof(float));
            }
        }
        
        out.close();
        std::cout << "Converted " << count << " references to " << bin_path << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }
}
