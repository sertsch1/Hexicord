#ifndef HEXICORD_RATELIMIT_LOCK_HPP
#define HEXICORD_RATELIMIT_LOCK_HPP

#include <unordered_map>
#include <string>
#include <functional>
#include <list>

namespace Hexicord
{
    /**
     * Class that implements semaphore-like locking based on Discord's
     * per-route rate limits.
     */
    class RatelimitLock {
    public:
        /**
         * Requests count that can be made using this
         * route until resetTime(route).
         *
         * If not known - returns -1.
         */
        int remaining(const std::string& route);

        /**
         * Latest known maximum count of requests for specified route
         * in current interval. May be outdated due to Discord 
         * __dynamic__ ratelimits.
         *
         * If not known - returns -1.
         */
        int total(const std::string& route);

        /**
         * Unix timestamp when ratelimit will be reset for this route.
         *
         * If not known - returns -1.
         */
        time_t resetTime(const std::string& route);
        
        /**
         * Called before perfoming request, block until reset time if rate limit hit.
         *
         * **Should not be called by user code directly.**
         */
        void down(const std::string& route);

        /**
         * Called after request in order to update information about ratelimits.
         *
         * **Should not be called by user code directly.**
         */
        void refreshInfo(const std::string& route,
                         unsigned remaining,
                         unsigned total,
                         time_t   resetTime);
    private:
        struct RatelimitInfo {
            std::string route;

            unsigned remaining;
            unsigned total;
            time_t resetTime;
        };

        // I need a queue to remove oldest information and 
        // hashmap to provide fast key-indexing.

        std::list<RatelimitInfo> queue;
        std::unordered_map<std::string, decltype(queue)::iterator> ratelimitPointers;
    };
} // namespace Hexicord

#endif // HEXICORD_RATELIMIT_LOCK_HPP
