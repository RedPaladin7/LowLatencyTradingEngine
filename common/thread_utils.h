#pragma once 

#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <sys/syscall.h>

using namespace std;

// Pinning thread to CPU core to minimize cache misses, and scheduling overhead

namespace Common {
    inline auto setThreadCore(int core_id) noexcept {
        cpu_set_t cpuset; // bitmask representing CPU cores
        CPU_ZERO(&cpuset); // clearing all the bits
        CPU_SET(core_id, &cpuset); // setting the core id bit to 1

        // setting affinity to that thread
        return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
    }

    template<typename T, typename... A> // pass any function with any number of variables
    inline auto createAndStartThread(int core_id, const string &name, T &&func, A &&...args) noexcept {
        // heap allocation so that the thread pointer can outlive scope
        auto t = new thread([&]() {
            // [&] to capture all the local variables
            // [&](){ code body that will run after the thread starts}
            if(core_id >= 0 && !setThreadCore(core_id)){
                cerr << "Failed to set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;
                exit(EXIT_FAILURE);
            }
            cerr << "Set core affinity for " << name << " " << pthread_self() << " to " << core_id << endl;
            forward<T>(func)((forward<A>(args))...); // forwarding the function
        });

        using namespace std::literals::chrono_literals;
        this_thread::sleep_for(1s); // give some time to bind to the core 

        return t;
    }
}

