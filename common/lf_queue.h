#pragma once

#include <iostream>
#include <vector>
#include <atomic>

#include "macros.h"
using namespace std;

// Lock free circular queue, using standard locks is too slow
// read, write index, number of elements are all atomic<size_t>

namespace Common {
    template<typename T> 
    class LFQueue final {
        public:

        // only takes size as param
        LFQueue(size_t num_elems): store_(num_elems, T()){}

        auto getNextToWriteTo() noexcept {
            // returns pointer to space at next write index
            return &store_[next_write_index_];
        }

        auto updateWriteIndex() noexcept {
            next_write_index_ = (next_write_index_ + 1) % store_.size();
            num_elements_++;
        }

        auto getNextToRead() const noexcept -> const T * {
            // if the queue is empty there is nothing to consume
            // returns pointer to actual element at next_read_index
            return (size() ? &store_[next_read_index_] : nullptr);
        }

        auto updateReadIndex() noexcept {
            next_read_index_ = (next_read_index_ + 1) % store_.size();
            ASSERT(num_elements_ != 0, "Read an invalid element in:" + std::to_string(pthread_self()));
            num_elements_--;
        }

        auto size() const noexcept {
            return num_elements_.load();
        }

        LFQueue() = delete;
        LFQueue(const LFQueue &) = delete;
        LFQueue(const LFQueue &&) = delete;
        LFQueue &operator=(const LFQueue &) = delete;
        LFQueue &operator=(const LFQueue &&) = delete;

        private:

        vector<T> store_; // memory pre allocated in vector
        atomic<size_t> next_write_index_ = {0};
        atomic<size_t> next_read_index_ = {0};
        atomic<size_t> num_elements_ = {0};
    };
}