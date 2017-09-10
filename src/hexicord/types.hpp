#ifndef HEXICORD_TYPES_HPP
#define HEXICORD_TYPES_HPP 

#include <string>                      // std::string
#include <iostream>                    // std::istream
#include <fstream>                     // std::ifstream
#include <iterator>                    // std::istreambuf_iterator
#include <vector>                      // std::vector
#include <functional>                  // std::hash
#include <hexicord/internal/utils.hpp> // Utils::split
#include <hexicord/json.hpp>           // nlohmann::json

#if _WIN32
    #define HEXICORD_PATH_DELIMITER '\\'
#elif !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
    #define HEXICORD_PATH_DELIMITER '/'
#endif

namespace Hexicord {
    /**
     * Struct for (filename, bytes) pair.
     */
    struct File {
        /**
         * Read file specified by `path` to \ref bytes.
         * Last component of path used as filename.
         */
        File(const std::string& path)
            : filename(Utils::split(path, HEXICORD_PATH_DELIMITER).back())
            , bytes(std::istreambuf_iterator<char>(std::ifstream(path).rdbuf()),
                    std::istreambuf_iterator<char>()) {}

        /**
         * Read stream until EOF to \ref bytes.
         */
        File(const std::string& filename, std::istream&& stream)
            : filename(filename)
            , bytes(std::istreambuf_iterator<char>(stream.rdbuf()),
                    std::istreambuf_iterator<char>()) {}

        /**
         * Use passed vector as file contents.
         */
        File(const std::string& filename, const std::vector<uint8_t>& bytes)
            : filename(filename)
            , bytes(bytes) {}

        const std::string filename;
        const std::vector<uint8_t> bytes;
    };

    struct Snowflake {
        Snowflake() : value(0) {}
        Snowflake(uint64_t value) : value(value) {}
        explicit Snowflake(const std::string& strvalue) : value(std::stoull(strvalue)) {}
        explicit Snowflake(const char* strvalue) : value(std::stoull(strvalue)) {}

        struct Parts {
            unsigned      counter       : 12;
            unsigned      processId     : 5;
            unsigned      workerId      : 5;
            unsigned long timestamp     : 42;
        };

        union {
            uint64_t value;
            Parts parts;
        };

        static constexpr uint64_t discordEpochMs = 1420070400000;

        inline time_t unixTimestamp() const {
            return this->parts.timestamp + discordEpochMs / 1000;
        }

        inline unsigned long long unixTimestampMs() const {
            return this->parts.timestamp + discordEpochMs;
        }

        inline operator uint64_t() const { return value; }
    };

    void to_json(nlohmann::json& json, const Snowflake& snowflake) {
        json = uint64_t(snowflake);
    }

    void from_json(const nlohmann::json& json, Snowflake& snowflake) {
        snowflake.value = json.get<uint64_t>();
    }
} // namespace Hexicord

namespace std {
    template<>
    class hash<Hexicord::Snowflake> {
    public:
        inline size_t operator()(const Hexicord::Snowflake& snowflake) const {
            return std::hash<uint64_t>()(snowflake);
        }
    };
}

#endif // HEXICORD_TYPES_HPP
