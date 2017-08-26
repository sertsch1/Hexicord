#include "ratelimit_lock.hpp"
#include <chrono>
#include <thread>
#include <ctime>

#ifndef NDEBUG // TODO: Replace with flag that affects only Hexicord.
    #include <iostream>
    #define DEBUG_MSG(msg) do { std::cerr << "ratelimit_lock.cpp:" << __LINE__ << " " << (msg) << '\n'; } while (false)
#else
    #define DEBUG_MSG(msg)
#endif

int Hexicord::RatelimitLock::remaining(const std::string& route) {
    auto it = ratelimits.find(route);
    return it != ratelimits.end() ? it->second.remaining : -1;
}

int Hexicord::RatelimitLock::total(const std::string& route) {
    auto it = ratelimits.find(route);
    return it != ratelimits.end() ? it->second.total : -1;
}

time_t Hexicord::RatelimitLock::resetTime(const std::string& route) {
    auto it = ratelimits.find(route);
    return it != ratelimits.end() ? it->second.resetTime : -1;
}

void Hexicord::RatelimitLock::down(const std::string& route, std::function<void()> busyWaiter) {
    auto it = ratelimits.find(route);

    // We can't predict limit hit in this case, so assume we don't hit it.
    if (it == ratelimits.end()) return;

    --it->second.remaining;

    DEBUG_MSG(std::string("Ratelimit semaphore acquire for route ") + route +
              " total=" + std::to_string(it->second.total) + 
              ", remaining=" + std::to_string(it->second.remaining));

    if (it->second.remaining == 0) {
        DEBUG_MSG(std::string("Ratelimit hit for route ") + route + ", blocking until " +
                  std::to_string(it->second.resetTime));
        while (time(nullptr) <= it->second.resetTime) {
            busyWaiter();
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
        }
    }
}

void Hexicord::RatelimitLock::refreshInfo(const std::string& route,
                                          unsigned remaining,
                                          unsigned total,
                                          time_t   resetTime) {

    DEBUG_MSG(std::string("Route ") + route +
              ": remaining=" + std::to_string(remaining) +
              ", total=" + std::to_string(total) +
              ", resetTime=" + std::to_string(resetTime));
    ratelimits[route] = { remaining, total, resetTime };
}
