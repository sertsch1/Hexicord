#ifndef HEXICORD_BEAST_REST_HPP
#define HEXICORD_BEAST_REST_HPP 

#include <string>                           // std::string
#include <boost/asio/io_service.hpp>        // boost::asio::io_service
#include <boost/asio/ip/tcp.hpp>            // boost::asio::ip::tcp::socket, boost::asio::ip::tcp::resolver
#include <boost/asio/ssl/stream.hpp>        // boost::asio::ssl::stream
#include <boost/asio/ssl/context.hpp>       // boost::asio::ssl::context
#include <beast/core/error.hpp>             // beast::system_error, beast::error_code
#include <beast/http/vector_body.hpp>       // beast::http::vector_body
#include <hexicord/rest.hpp>                // Hexicord::REST::HTTPResponse

/**
 *  \file beast_rest.hpp
 *  \internal
 *
 *  Implementation of HTTP for \ref GenericHTTPConnection.
 */

namespace Hexicord {
    class BeastHTTPBase {
    public:
        using ErrorType   = beast::error_code;
        using Exception   = beast::system_error;
        using RequestType = beast::http::request<beast::http::vector_body<uint8_t> >;

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
