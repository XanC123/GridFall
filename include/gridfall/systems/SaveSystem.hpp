#pragma once

#include "gridfall/entities/Player.hpp"
#include <string>

namespace gridfall {

struct SaveData {
    int currentLevel{};   // Zero-based sector index.
    int unlockedLevel{1}; // One-based highest unlocked sector.
    Player player{};
};

// Versioned text saves are intentionally human-readable and safe to inspect.
// A loaded run starts at the saved sector's entrance, retaining player progress.
class SaveSystem {
public:
    [[nodiscard]] static bool exists(const std::string& path = "gridfall.save") noexcept;
    [[nodiscard]] static bool save(const SaveData& data, std::string* error = nullptr,
                                   const std::string& path = "gridfall.save");
    [[nodiscard]] static bool load(SaveData& out, std::string* error = nullptr,
                                   const std::string& path = "gridfall.save");
};

} // namespace gridfall
