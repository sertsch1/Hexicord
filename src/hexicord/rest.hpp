#ifndef HEXICORD_REST_HPP
#define HEXICORD_REST_HPP

#include <vector>                    // std::vector
#include <string>                    // std::string
#include <unordered_map>             // std::unordered_map
#include <boost/asio/io_service.hpp> // asio::io_service
#include <beast/http/field.hpp>      // beast::http::field
#include <beast/http/verb.hpp>       // beast::http::verb

namespace Hexicord { namespace REST {
    using IOService = boost::asio::io_service;
    using Headers   = std::unordered_map<beast::http::field, std::string>;
    using Bytes     = std::vector<uint8_t>;

    struct HTTPResponse {
        unsigned statusCode;
        
        /**
         * \warning I didn't found correct way to extract headers from response.
         * So this field always empty.
         */
        Headers headers;
        Bytes body;
    };

    HTTPResponse httpsRequest(IOService& ioService, const std::string& servername,
                              beast::http::verb method, const std::string& path,
                              const Headers& headers, const Bytes& bytes);

    HTTPResponse get(IOService& ioService, const std::string& servername, 
                     const std::string& path, const Headers& headers = {});

    HTTPResponse post(IOService& ioService, const std::string& servername, 
                      const std::string& path, const Headers& headers = {},
                      const Bytes& body = {});

    HTTPResponse put(IOService& ioService, const std::string& servername, 
                     const std::string& path, const Headers& headers = {},
                     const Bytes& body = {});

    HTTPResponse delete_(IOService& ioService, const std::string& servername,
                         const std::string& path, const Headers& headers = {});
}} // namespace Hexicord::__REST

#endif // HEXICORD_REST_HPP
