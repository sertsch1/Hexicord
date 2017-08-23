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

#ifndef HEXICORD_BEAST_REST_HPP
#define HEXICORD_BEAST_REST_HPP 

#include <string>                           // std::string
#include <boost/asio/io_service.hpp>        // boost::asio::io_service
#include <boost/asio/ip/tcp.hpp>            // boost::asio::ip::tcp::socket, boost::asio::ip::tcp::resolver
#include <boost/asio/ssl.hpp>               // boost::asio::ssl
#include <boost/asio/ssl/stream.hpp>        // boost::asio::ssl::stream
#include <boost/asio/ssl/context.hpp>       // boost::asio::ssl::context
#include <boost/beast/core/error.hpp>       // boost::system::system_error, boost::system::error_code
#include <boost/beast/http/vector_body.hpp> // boost::beast::http::vector_body
#include <hexicord/internal/rest.hpp>       // Hexicord::REST::HTTPResponse

/**
 *  \file beast_rest.hpp
 *  \internal
 *
 *  Implementation of HTTP for \ref GenericHTTPConnection.
 */

namespace Hexicord {
    class BeastHTTPBase {
    public:
        using ErrorType   = boost::system::error_code;
        using Exception   = boost::system::system_error;
        using RequestType = boost::beast::http::request<boost::beast::http::vector_body<uint8_t> >;

        inline static void setMethod(RequestType& request, const std::string& method) {
            request.method_string(method);
        }

        inline static void setPath(RequestType& request, const std::string& path) {
            request.target(path);
        }

        inline static void setVersion(RequestType& request, unsigned version) {
            request.version = version;
        }

        inline static void setHeader(RequestType& request, const std::string& key, const std::string& value) {
            request.set(key, value);
        }

        inline static void setBody(RequestType& request, const std::vector<uint8_t>& body) {
            request.body = body;
        }
    };

    /// Satisfies requirements of \ref StreamProvider.
    class BeastHTTPS : public BeastHTTPBase {
    public:
        static const unsigned short DefaultPort;

        BeastHTTPS(const std::string& servername, boost::asio::io_service& ioService, unsigned short port = BeastHTTPS::DefaultPort);

        REST::HTTPResponse request(RequestType request);
        //void asyncRequest(RequestType&& request, GenericHTTPConnection<BeastHTTPS>::AsyncRequestCallback callback);

        void openConnection();
        void closeConnection();
        bool isOpen() const;
        
        boost::asio::ssl::context tlsctx;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream;
    private:
        boost::asio::ip::tcp::resolver::iterator resolutionResult;
        bool alive = false;
    };
} // namespace Hexicord

#endif // HEXICORD_BEAST_REST_HPP
