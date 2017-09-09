#include <hexicord/config.hpp>
#ifdef HEXICORD_ZLIB
#ifndef HEXICORD_ZLIB_HPP
#define HEXICORD_ZLIB_HPP
#define ZLIB_COMPLETE_CHUNK 16384
namespace Hexicord {
    namespace zlib {
        /**
         * \internal
         *
         * Simple zlib decompressor
         */
        uint8_t* decompress(const std::vector<uint8_t>& input);
    }
}
#endif // HEXICORD_ZLIB_HPP
#endif // HEXICORD_ZLIB
