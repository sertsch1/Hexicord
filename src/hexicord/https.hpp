#ifndef HEXICORD_REST_HPP
#define HEXICORD_REST_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <boost/asio/io_service.hpp>
#include <beast/http/field.hpp>

namespace Hexicord { namespace __REST {
    using IOService = boost::asio::io_service;
    using Headers   = std::unordered_map<beast::http::field, std::string>;
    using Bytes     = std::vector<uint8_t>;

    struct HTTPResponse {
        unsigned status_code;
        
        /**
         * \warning I didn't found correct way to extract headers from response.
         * So this field always empty.
         */
        Headers headers;
        Bytes body;
    };

    HTTPResponse httpsGet(IOService& ioService, const std::string& servername, 
                          const std::string& path, Headers headers = {});

    HTTPResponse httpPost(IOService& ioService, const std::string& servername, 
                          const std::string& path, Headers headers = {},
                          const Bytes& body = {});

    HTTPResponse httpPut(IOService& ioService, const std::string& servername, 
                         const std::string& path, Headers headers = {},
                         const Bytes& body = {});

    HTTPResponse httpDelete(IOService& ioService, const std::string& servername,
                            const std::string& path, Headers headers = {});
}} // namespace Hexicord::__REST

#endif // HEXICORD_REST_HPP
