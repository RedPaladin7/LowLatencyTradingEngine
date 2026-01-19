#pragma once 

#include <cstdint>
#include <vector>
#include <string>
#include "macros.h"

using namespace std;

namespace Common {
    template<typename T> 
    class MemPool final {
        public:
        explicit MemPool(size_t num_elems) : store_(num_elems, {T(), true}) {
            ASSERT(reinterpret_cast<const ObjectBlock *>(&(store_[0].object_)) == &(store_[0]), "T object should be the first member of the object block.");
        }

        template<typename... Args> 
        T *allocate(Args... args) noexcept {
            auto obj_block = &(store_[next_free_index_]);
                ASSERT(obj_block->is_free_, "Expected free ObjectBlock at index:" + to_string(next_free_index));
                
        }
    };
}