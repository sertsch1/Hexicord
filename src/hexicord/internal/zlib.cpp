// Hexicord - Discord API library for C++11 using boost libraries.
// Copyright © 2017 Maks Mazurov (fox.cpp) <foxcpp@yandex.ru>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
// OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "zlib.hpp"
#ifdef HEXICORD_ZLIB
#include <memory.h>
#include <zlib.h>
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
            throw std::runtime_error("zlib initialization failed");
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
                    throw std::runtime_error("zlib stream error");
                }
                for (char j : out_) {
                    result.push_back(j);
                }
            } while (strm_.avail_out == 0);
        }
        inflateEnd(&strm_);
        return result.data();
    }
}
}
#endif
