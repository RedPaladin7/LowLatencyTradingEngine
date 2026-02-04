#pragma once 

#include <cstring>
#include <iostream>
using namespace std;

// Compiler hints to optimize branch predictions

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

inline auto ASSERT(bool cond, const string &msg) noexcept {
    if(UNLIKELY(!cond)) {
        cerr << "ASSERT : " << msg << endl;
        exit(EXIT_FAILURE);
    }
}

inline auto FATAL(const string &msg) noexcept {
    cerr << "FATAL : " << msg << endl;
    exit(EXIT_FAILURE);
}