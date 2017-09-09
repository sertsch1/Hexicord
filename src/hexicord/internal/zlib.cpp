#ifdef HEXICORD_ZLIB
#include <vector>
#include <zlib.h>
#include "zlib.hpp"
#include <hexicord/exceptions.hpp>
namespace Hexicord {
namespace zlib {
    char in_[ZLIB_COMPLETE_CHUNK];
    char out_[ZLIB_COMPLETE_CHUNK];
    z_stream strm_;

    uint8_t* decompress(const std::vector<uint8_t>& input) {
        int retval;
        strm_.zalloc = Z_NULL;
        strm_.zfree = Z_NULL;
        strm_.opaque = Z_NULL;
        strm_.avail_in = 0;
        strm_.next_in = Z_NULL;
        retval = inflateInit2(&strm_, 15);
        if (retval != Z_OK) {
            throw LogicError("zlib initialization failed");
        }
        std::vector<uint8_t> result(ZLIB_COMPLETE_CHUNK);
        for (uint32_t i = 0; i < input.size(); i += ZLIB_COMPLETE_CHUNK) {
            int howManyLeft = input.size() - i;
            int howManyWanted = (howManyLeft > ZLIB_COMPLETE_CHUNK) ? ZLIB_COMPLETE_CHUNK : howManyLeft;
            memcpy(in_, input.data() + i, howManyWanted);
            strm_.avail_in = howManyWanted;
            strm_.next_in = (Bytef *) in_;
            if (strm_.avail_in == 0) {
                break;
            }
            do {
                memset(&out_, 0, ZLIB_COMPLETE_CHUNK);
                strm_.avail_out = ZLIB_COMPLETE_CHUNK;
                strm_.next_out = (Bytef *) out_;
                retval = inflate(&strm_, Z_NO_FLUSH);
                if (retval == Z_STREAM_ERROR) {
                    throw LogicError("zlib stream error");
                }
                for (int j = 0; j < ZLIB_COMPLETE_CHUNK; j++) {
                    result.push_back(out_[j]);
                }
            } while (strm_.avail_out == 0);
        }
        inflateEnd(&strm_);
        return result.data();
    }
}
}
#endif
