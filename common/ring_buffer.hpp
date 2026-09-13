#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

template <std::size_t Capacity>
class RingBuffer {
public:
    bool push(uint8_t byte) {
        if (full()) {
            dropped_++;
            return false;
        }
        data_[head_] = byte;
        head_ = (head_ + 1) % Capacity;
        size_++;
        return true;
    }

    bool pop(uint8_t& byte) {
        if (empty()) {
            return false;
        }
        byte = data_[tail_];
        tail_ = (tail_ + 1) % Capacity;
        size_--;
        return true;
    }

    bool peek(std::size_t offset, uint8_t& byte) const {
        if (offset >= size_) {
            return false;
        }
        byte = data_[(tail_ + offset) % Capacity];
        return true;
    }

    void drop(std::size_t count) {
        while (count-- > 0 && !empty()) {
            uint8_t ignored = 0;
            pop(ignored);
        }
    }

    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }
    std::size_t size() const { return size_; }
    std::size_t dropped() const { return dropped_; }

private:
    std::array<uint8_t, Capacity> data_{};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t size_ = 0;
    std::size_t dropped_ = 0;
};
