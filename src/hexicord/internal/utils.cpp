#include "utils.hpp"
#include <iterator>     // std::back_inserter
#include <algorithm>    // std::copy
#include <cctype>       // std::isalnum
#include <stdexcept>    // std::invalid_argument
#include <cassert>      // assert
#include <sstream>      // std::ostringstream
#include <iomanip>      // std::setw

namespace Hexicord { namespace Utils {
    namespace Magic {
        bool isGif(const std::vector<uint8_t>& bytes) {
            // according to http://fileformats.archiveteam.org/wiki/GIF
            return bytes.size() >= 6 &&  // TODO: check against minimal headers size
                bytes[0] == 'G' &&  // should begin with 'GIF'
                bytes[1] == 'I' &&
                bytes[2] == 'F' &&
                ( // then GIF version, '87a' or '89a'
                    (
                        bytes[3] == '8' &&
                        bytes[4] == '7' &&
                        bytes[5] == 'a'
                    ) || (
                        bytes[3] == '8' &&
                        bytes[4] == '9' &&
                        bytes[5] == 'a'
                    )
                ); 
        }

        bool isJfif(const std::vector<uint8_t>& bytes) {
            // according to http://fileformats.archiveteam.org/wiki/JFIF
            return bytes.size() >= 3 && // TODO: check against minimal headers size
                bytes[0] == 0xFF &&
                bytes[1] == 0xD8 &&
                bytes[2] == 0xFF;
        }

        bool isPng(const std::vector<uint8_t>& bytes) {
            // according to https://www.w3.org/TR/PNG/#5PNG-file-signature
            return bytes.size() >= 12 && // signature + single no-data chunk size
                bytes[0] == 137 &&
                bytes[1] == 'P' &&
                bytes[2] == 'N' &&
                bytes[3] == 'G' &&
                bytes[4] == 13  && // CR
                bytes[5] == 10  && // LF
                bytes[6] == 26  && // SUB
                bytes[7] == 10;    // LF
        }
    }

	std::string domainFromUrl(const std::string& url) {
	   enum State {
	       ReadingSchema,
	       ReadingSchema_Colon,
	       ReadingSchema_FirstSlash,
	       ReadingSchema_SecondSlash,
	       ReadingDomain,
	       End 
	   } state = ReadingSchema;

	   std::string result;
	   for (char ch : url) {
	       switch (state) {
	       case ReadingSchema:
	           if (ch == ':') {
	               state = ReadingSchema_Colon;
	           } else if (!std::isalnum(ch)) {
	               throw std::invalid_argument("Missing colon after schema.");
	           }
	           break;
	       case ReadingSchema_Colon:
	           if (ch == '/') {
	               state = ReadingSchema_FirstSlash;
	           } else {
	               throw std::invalid_argument("Missing slash after schema.");
	           }
	           break;
	       case ReadingSchema_FirstSlash:
	           if (ch == '/') {
	               state = ReadingSchema_SecondSlash;
	           } else {
	               throw std::invalid_argument("Missing slash after schema.");
	           }
	           break;
	       case ReadingSchema_SecondSlash:
	           if (std::isalnum(ch) || ch == '-' || ch == '_') {
	               state = ReadingDomain;
	               result += ch;
	           } else {
	               throw std::invalid_argument("Invalid first domain character.");
	           }
	           break;
	       case ReadingDomain:
	           if (ch == '/') {
	               state = End;
	           } else {
	               result += ch;
	           }
	       case End:
	           break;
	       }
	       if (state == End) break;
	   }
	   return result;
	}

	std::string base64Encode(const std::vector<uint8_t>& data)
	{
	   static constexpr uint8_t base64Map[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	   // Use = signs so the end is properly padded.
	   std::string result((((data.size() + 2) / 3) * 4), '=');
	   size_t outpos = 0;
	   int bits_collected = 0;
	   unsigned accumulator = 0;

	   for (uint8_t byte : data) {
	      accumulator = (accumulator << 8) | (byte & 0xffu);
	      bits_collected += 8;
	      while (bits_collected >= 6) {
	         bits_collected -= 6;
	         result[outpos++] = base64Map[(accumulator >> bits_collected) & 0x3fu];
	      }
	   }
	   if (bits_collected > 0) { // Any trailing bits that are missing.
	      assert(bits_collected < 6);
	      accumulator <<= 6 - bits_collected;
	      result[outpos++] = base64Map[accumulator & 0x3fu];
	   }
	   assert(outpos >= (result.size() - 2));
	   assert(outpos <= result.size());
	   return result;
	}

    std::string urlEncode(const std::string& raw) {
        std::ostringstream resultStream;

        resultStream.fill('0');
        resultStream << std::hex;
        
        for (char ch : raw) {
            if (std::isalnum(ch) || ch == '.' || ch == '~' || ch == '_' || ch == '-') {
                resultStream << ch;
            } else {
                resultStream << std::uppercase;
                resultStream << '%' << std::setw(2) << int(ch);
                resultStream << std::nouppercase;
            }
        }
        return resultStream.str();
    }
}} // namespace Hexicord::Utils
