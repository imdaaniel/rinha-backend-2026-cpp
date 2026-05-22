#pragma once

#include <vector>
#include <string>
#include <cstdint>

// Binary format for reference metadata
// Header: "RINH" + version(2) + dim(2) + count(4) + label_data_size(4) = 16 bytes
// Label section: [len(2) + label_bytes] * count
// Vector section: float32 * dim * count (4-byte aligned)

struct BinaryHeader {
    char magic[4];       // "RINH"
    uint16_t version;   // 1
    uint16_t dim;
    uint32_t count;
    uint32_t label_data_size;
};

class MetadataLoader {
public:
    static bool load_labels(const std::string& bin_path, 
                          std::vector<uint8_t>& labels,
                          size_t& count);
    
    static bool convert_json_to_bin(const std::string& json_path,
                                    const std::string& bin_path);
};
