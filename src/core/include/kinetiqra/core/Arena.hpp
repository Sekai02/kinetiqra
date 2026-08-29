#pragma once

#include <kinetiqra/core/Handle.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace kinetiqra::core {

// Contiguous storage addressed by Handle.
//
// Elements keep their index for as long as they live, so handles stay valid
// across insertions, which is what makes them usable as long-lived references.
// Removal frees the slot for reuse and bumps its generation, so a handle kept
// across the removal is rejected instead of quietly resolving to whichever
// element moved in afterwards.
//
// Iteration is over the dense storage, so it stays cache friendly; the cost is
// that removed slots are skipped rather than compacted away.
template <typename T, typename Tag>
class Arena {
public:
    using Id = Handle<Tag>;

    Id insert(T value) {
        if (!free_.empty()) {
            const std::uint32_t index = free_.back();
            free_.pop_back();
            slots_[index].value = std::move(value);
            slots_[index].alive = true;
            ++alive_count_;
            return Id{index, slots_[index].generation};
        }

        slots_.push_back(Slot{std::move(value), 0, true});
        ++alive_count_;
        return Id{static_cast<std::uint32_t>(slots_.size() - 1), 0};
    }

    // Returns false if the handle was already stale, which callers can treat as
    // a double removal rather than discovering it later.
    bool remove(Id id) {
        if (!contains(id)) {
            return false;
        }

        Slot& slot = slots_[id.index];
        slot.value = T{};
        slot.alive = false;
        ++slot.generation;
        free_.push_back(id.index);
        --alive_count_;
        return true;
    }

    [[nodiscard]] bool contains(Id id) const {
        return id.valid() && id.index < slots_.size() && slots_[id.index].alive &&
               slots_[id.index].generation == id.generation;
    }

    // Null for a stale or unknown handle, so a caller that forgets to check
    // dereferences null rather than reading a neighbouring element.
    [[nodiscard]] T* get(Id id) { return contains(id) ? &slots_[id.index].value : nullptr; }

    [[nodiscard]] const T* get(Id id) const {
        return contains(id) ? &slots_[id.index].value : nullptr;
    }

    [[nodiscard]] std::size_t size() const { return alive_count_; }

    [[nodiscard]] bool empty() const { return alive_count_ == 0; }

    // Total number of slots, live and free. Also the exclusive upper bound of
    // any live index, which is what iteration needs.
    [[nodiscard]] std::size_t slot_count() const { return slots_.size(); }

    [[nodiscard]] bool alive(std::uint32_t index) const {
        return index < slots_.size() && slots_[index].alive;
    }

    [[nodiscard]] Id id_at(std::uint32_t index) const {
        return alive(index) ? Id{index, slots_[index].generation} : Id{};
    }

    void clear() {
        slots_.clear();
        free_.clear();
        alive_count_ = 0;
    }

private:
    struct Slot {
        T value{};
        std::uint32_t generation{0};
        bool alive{false};
    };

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_;
    std::size_t alive_count_{0};
};

}  // namespace kinetiqra::core
