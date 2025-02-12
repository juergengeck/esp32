#pragma once

#include <nvs_flash.h>
#include <esp_partition.h>
#include "keys.h"

namespace one {
namespace instance {

class KeyStorage {
public:
    KeyStorage();
    ~KeyStorage();

    // Initialize the secure key storage
    bool initialize();

    // Store keys in encrypted NVS partition
    bool saveKeys(const InstanceKeys& keys, const char* namespace_name = "instance");

    // Load keys from encrypted NVS partition
    bool loadKeys(InstanceKeys& keys, const char* namespace_name = "instance");

private:
    static constexpr char PARTITION_LABEL[] = "nvs_keys";
    static constexpr char PRIVATE_KEY_NAME[] = "priv_key";
    static constexpr char PUBLIC_KEY_NAME[] = "pub_key";

    nvs_handle_t nvs_handle_;
    bool initialized_;

    // Initialize NVS partition
    bool initNVS();

    // Ensure the partition is encrypted
    bool checkEncryption();
};

} // namespace instance
} // namespace one 