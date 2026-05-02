#pragma once

#include <array>
#include <cstddef>

#include "huira/assets/primitive.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/volumes/medium.hpp"

namespace huira {

/**
 * @brief A small fixed-capacity stack tracking which media-bearing primitives a ray is inside.
 *
 * The active medium for free-flight sampling is the medium on top of the stack.  The stack is 
 * modified only by transmission events.
 */
template <IsSpectral TSpectral>
class MediumStack {
  public:
    static constexpr std::size_t CAPACITY = 8;

    MediumStack() = default;

    /**
     * @brief Return the active medium, or nullptr if the stack is empty.
     */
    [[nodiscard]] const Medium<TSpectral>* top() const noexcept
    {
        return (size_ == 0) ? nullptr : entries_[size_ - 1].medium;
    }

    /**
     * @brief True if no media-bearing primitives are currently on the stack.
     */
    [[nodiscard]] bool is_empty() const noexcept { return size_ == 0; }

    /**
     * @brief Number of entries on the stack.
     */
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /**
     * @brief Toggle membership of a primitive on the stack.
     */
    void toggle(const Primitive<TSpectral>* primitive) noexcept
    {
        if (primitive == nullptr) {
            return;
        }
        const Medium<TSpectral>* medium = primitive->medium.get();
        if (medium == nullptr) {
            return;
        }

        if (size_ > 0 && entries_[size_ - 1].primitive == primitive) {
            --size_;
        } else if (size_ < CAPACITY) {
            entries_[size_] = {primitive, medium};
            ++size_;
        }
    }

  private:
    struct Entry {
        const Primitive<TSpectral>* primitive{nullptr};
        const Medium<TSpectral>* medium{nullptr};
    };

    std::array<Entry, CAPACITY> entries_{};
    std::size_t size_{0};
};

} // namespace huira
