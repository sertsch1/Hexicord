#include "ratelimit_lock.hpp"
#include <chrono>
#include <thread>
#include <ctime>

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

    if (it->second.remaining == 0) {
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

    ratelimits["route"] = { remaining, total, resetTime };
}
