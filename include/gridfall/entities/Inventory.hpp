#pragma once

namespace gridfall {

struct Inventory {
    int healthPotions{2};
    int regenPotions{1};

    void addHealthPotion(int count = 1) noexcept { healthPotions += count; }
    void addRegenPotion(int count = 1) noexcept { regenPotions += count; }
    bool consumeHealthPotion() noexcept;
    bool consumeRegenPotion() noexcept;
};

} // namespace gridfall
