#include "gridfall/entities/Inventory.hpp"
namespace gridfall {
bool Inventory::consumeHealthPotion() noexcept {
    if (healthPotions <= 0) return false;
    --healthPotions;
    return true;
}
bool Inventory::consumeRegenPotion() noexcept {
    if (regenPotions <= 0) return false;
    --regenPotions;
    return true;
}
} // namespace gridfall
