#ifndef HEXICORD_UTILS_HPP
#define HEXICORD_UTILS_HPP

#include <vector>
#include <string>
#include <istream>

/**
 *  Reusable code snippets.
 */

namespace Hexicord { namespace Utils {
    /**
     *  Fast but in-percise type identification based on first ("magic") bytes.
     */
    namespace Magic {
        bool isGif(const std::vector<uint8_t>& bytes);
        bool isJfif(const std::vector<uint8_t>& bytes);
        bool isPng(const std::vector<uint8_t>& bytes);
    }

    /**
     *  scheme://domain/otherstuff?aas=b#as -> domain
     */
    std::string domainFromUrl(const std::string& url);

    /**
     *  Encode arbitrary data using base64.
     */
    std::string base64Encode(const std::vector<uint8_t>& bytes);

    std::string urlEncode(const std::string& raw);
}} // namespace Hexicord::Utils

#endif // HEXICORD_UTILS_HPP
