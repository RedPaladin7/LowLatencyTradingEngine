#pragma once

#include <string>
#include <chrono>
#include <ctime>
using namespace std; 

namespace Common {
    typedef int64_t Nanos;

    constexpr Nanos NANOS_TO_MICROS = 1000;
    constexpr Nanos MICROS_TO_MILLIS = 1000;
    constexpr Nanos MILLIS_TO_SECS = 1000;
    constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
    constexpr Nanos NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

    inline auto getCurrentNanos() noexcept {
        return chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count();
    }

    inline auto& getCurrentTimeStr(string* time_str) {
        const auto time = chrono::system_clock::to_time_t(chrono::system_clock::now());
        *time_str = ctime(&time);
        if(!time_str->empty()) {
            time_str->at(time_str->length()-1) = '\0';
        }
        return *time_str;
    }

}