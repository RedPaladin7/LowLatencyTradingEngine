#pragma once

#include <string>
#include <fstream>
#include <cstdio>

#include "macros.h"
#include "lf_queue.h"
#include "thread_utils.h"
#include "time_utils.h"
using namespace std;

namespace Common {
    constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;

    enum class LogType: int8_t {
        CHAR = 0,
        INTEGER = 1,
        LONG_INTEGER = 2,
        LONG_LONG_INTEGER = 3,
        UNSIGNED_INTEGER = 4,
        UNSIGNED_LONG_INTEGER = 5,
        UNSIGNED_LONG_LONG_INTEGER = 6,
        FLOAT = 7,
        DOUBLE = 8,
    };

    struct LogElement {
        LogType type_ = LogType::CHAR;
        union {
            char c;
            int i;
            long l;
            long long ll;
            unsigned int ui;
            unsigned long ul;
            unsigned long long ull;
            float f;
            double d;
        } u_;
    };

    class Logger final {
        public:
        auto flushQueue() noexcept {
            while (running_) {
                for (auto next = queue_.getNextToRead(); queue_.size() && next; next = queue_.getNextToRead()) {
                    switch(next->type_) {
                        case LogType::CHAR:
                        file_ << next->u_.c;
                        break;
                        case LogType::INTEGER:
                        file_ << next->u_.i;
                        break;
                        case LogType::LONG_INTEGER:
                        file_ << next->u_.l;
                        break;
                        case LogType::LONG_LONG_INTEGER:
                        file_ << next->u_.ll;
                        break;
                        case LogType::UNSIGNED_INTEGER:
                        file_ << next->u_.ui;
                        break;
                        case LogType::UNSIGNED_LONG_INTEGER:
                        file_ << next->u_.ul;
                        break;
                        case LogType::UNSIGNED_LONG_LONG_INTEGER:
                        file_ << next->u_.ull;
                        break;
                        case LogType::FLOAT:
                        file_ << next->u_.f;
                        break;
                        case LogType::DOUBLE:
                        file_ << next->u_.d;
                        break;
                    }
                    queue_.updateReadIndex();
                }
                file_.flush();

                using namespace literals::chrono_literals;
                this_thread::sleep_for(10ms);
            }
        }

        explicit Logger(const string &file_name): file_name_(file_name), queue_(LOG_QUEUE_SIZE) {
            file_.open(file_name);
            ASSERT(file_.is_open(), "Could not open file:" + file_name);
            logger_thread_ = createAndStartThread(-1, "Common/Logger " + file_name_, [this]() {flushQueue();});
            ASSERT(logger_thread_ != nullptr, "Could not create logger thread");
        } 

        ~Logger() {
            string time_str;
            cerr << Common::getCurrentTimeStr(&time_str) << " flushing and closing logger for " << file_name_ << endl;
            running_ = false;
            logger_thread_->join();
            file_.close();
            cerr << Common::getCurrentTimeStr(&time_str) << " Logger for " << file_name_ << " exiting." << std::endl;
        }

        auto pushValue(const LogElement &log_element) noexcept {
            *(queue_.getNextToWriteTo()) = log_element;
            queue_.updateWriteIndex();
        }

        auto pushValue(const char value) noexcept {
            pushValue(LogElement{LogType::CHAR, {.c = value}});
        }

        auto pushValue(const int value) noexcept {
            pushValue(LogElement{LogType::INTEGER, {.i = value}});
        }

        auto pushValue(const long value) noexcept {
            pushValue(LogElement{LogType::LONG_INTEGER, {.l = value}});
        }

        auto pushValue(const long long value) noexcept {
            pushValue(LogElement{LogType::LONG_LONG_INTEGER, {.ll = value}});
        }

        auto pushValue(const unsigned value) noexcept {
            pushValue(LogElement{LogType::UNSIGNED_INTEGER, {.ui = value}});
        }

        auto pushValue(const unsigned long value) noexcept {
            pushValue(LogElement{LogType::UNSIGNED_LONG_INTEGER, {.ul = value}});
        }

        auto pushValue(const unsigned long long value) noexcept {
            pushValue(LogElement{LogType::UNSIGNED_LONG_LONG_INTEGER, {.ull = value}});
        }

        auto pushValue(const float value) noexcept {
            pushValue(LogElement{LogType::FLOAT, {.f = value}});
        }

        auto pushValue(const double value) noexcept {
            pushValue(LogElement{LogType::DOUBLE, {.d = value}});
        }

        auto pushValue(const char *value) noexcept {
            while(*value) {
                pushValue(*value);
                ++value;
            }
        }

        auto pushValue(const string &value) noexcept {
            pushValue(value.c_str());
        }

        template<typename T, typename... A>
        auto log(const char *s, const T &value, A... args) noexcept {
            while(*s) {
                if (*s == '%') {
                    if (UNLIKELY(*(s+1) == '%')){
                        ++s;
                    } else {
                        pushValue(value);
                        log(s+1, args...);
                        return;
                    }
                }
                pushValue(*s++);
            }
            FATAL("extra arguments provided to log()");
        }

        auto log(const char *s) noexcept {
            while(*s) {
                if(*s == '%') {
                    if (UNLIKELY(*(s+1)=='%')){
                        ++s;
                    } else {
                        FATAL("missing arguments in log");
                    }
                }
                pushValue(*s++);
            }
        }

        Logger() = delete;
        Logger (const Logger &) = delete;
        Logger (const Logger &&) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger &operator=(const Logger &&) = delete;

        private:
        const string file_name_;
        ofstream file_;

        LFQueue<LogElement> queue_;
        atomic<bool> running_ = {true};
        thread *logger_thread_ = nullptr;

    };
}