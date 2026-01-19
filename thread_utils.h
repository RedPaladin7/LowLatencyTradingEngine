#pragma once 

#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <sys/syscall.h>

using namespace std;

namespace Common {
    inline auto setThreadCore(int core_id) noexcept {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
    }

    template<typename T, typename... A>
    inline auto createAndStartThread(int core_id, const string &name, T &&func, A &&...args) noexcept {
        auto t = new thread([&]() {
            if(core_id >= 0 && !setThreadCore(core_id)){
                cerr << "Failed to set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;
                exit(EXIT_FAILURE);
            }
            cerr << "Set core affinity for " << name << " " << pthread_self() << " to " << core_id << endl;
            forward<T>(func)((forward<A>(args))...);
        });

        using namespace std::literals::chrono_literals;
        this_thread::sleep_for(1s);

        return t;
    }
}

