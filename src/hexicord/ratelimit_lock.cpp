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

#ifndef HEXICORD_RATELIMIT_CACHE_SIZE
    #define HEXICORD_RATELIMIT_CACHE_SIZE 100
#endif

int Hexicord::RatelimitLock::remaining(const std::string& route) {
    auto it = ratelimitPointers.find(route);
    return it != ratelimitPointers.end() ? it->second->remaining : -1;
}

int Hexicord::RatelimitLock::total(const std::string& route) {
    auto it = ratelimitPointers.find(route);
    return it != ratelimitPointers.end() ? it->second->total : -1;
}

time_t Hexicord::RatelimitLock::resetTime(const std::string& route) {
    auto it = ratelimitPointers.find(route);
    return it != ratelimitPointers.end() ? it->second->resetTime : -1;
}

void Hexicord::RatelimitLock::down(const std::string& route, std::function<void(time_t)> busyWaiter) {
    auto it = ratelimitPointers.find(route);

    // We can't predict limit hit in this case, so assume we don't hit it.
    if (it == ratelimitPointers.end()) return;

    RatelimitInfo& routeInfo = *it->second;

    --routeInfo.remaining;

    DEBUG_MSG(std::string("Ratelimit semaphore acquire for route ") + route +
              " total=" + std::to_string(routeInfo.total) + 
              ", remaining=" + std::to_string(routeInfo.remaining));

    if (routeInfo.remaining == 0) {
        DEBUG_MSG(std::string("Ratelimit hit for route ") + route + ", blocking until " +
                  std::to_string(routeInfo.resetTime));
        while (time(nullptr) <= routeInfo.resetTime) {
            busyWaiter(routeInfo.resetTime - time(nullptr));
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
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

    auto ratelimitItIt = ratelimitPointers.find(route);
    if (ratelimitItIt != ratelimitPointers.end()) {
        *ratelimitItIt->second = { route, remaining, total, resetTime };
    } else {
        if (queue.size() == HEXICORD_RATELIMIT_CACHE_SIZE) {
            ratelimitPointers.erase(ratelimitPointers.find(queue.front().route));
            
            DEBUG_MSG("Ratelimit cache hit HEXICORD_RATELIMIT_CACHE_SIZE, earsing information about oldest route...");
            DEBUG_MSG(std::string("Route removed: ") + queue.front().route);

            queue.pop_front();
        }

        queue.push_back({ route, remaining, total, resetTime });
        ratelimitPointers.insert({route, queue.end() - 1 });
    }
}
