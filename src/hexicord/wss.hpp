#ifndef HEXICORD_WSS_HPP
#define HEXICORD_WSS_HPP

#include <string>                       // std::string
#include <vector>                       // std::vector
#include <memory>                       // std::enable_shared_from_this
#include <mutex>                        // std::mutex, std::lock_guard
#include <beast/core/error.hpp>         // beast::error_code, beast::system_error 
#include <beast/websocket/stream.hpp>   // websocket::stream
#include <beast/websocket/ssl.hpp>      // required to use ssl::stream beyond websocket
#include <boost/asio/ip/tcp.hpp>        // tcp::socket, tcp::resolver::iterator 
#include <boost/asio/io_service.hpp>    // asio::io_service
#include <boost/asio/ssl/context.hpp>   // ssl::context
#include <boost/asio/ssl/stream.hpp>    // ssl::stream

/**
 *  \file wss.hpp
 *  \internal
 *
 *  Better interface for WebSockets on top of low-level boost.beast.
 */

namespace Hexicord {
    namespace ssl       = boost::asio::ssl;
    namespace websocket = beast::websocket;

    /**
     *  \internal
     *
     *  High-level beast WebSockets wrapper. Provides basic I/O operations:
     *  read, send, async read, async write.
     */
    class TLSWebSocket : std::enable_shared_from_this<TLSWebSocket> {
        using TLSStream = ssl::stream<boost::asio::ip::tcp::socket>;
        using WSSStream = websocket::stream<TLSStream>;
        using IOService = boost::asio::io_service;
        using tcp = boost::asio::ip::tcp;
    public:
        using AsyncReadCallback = std::function<void(TLSWebSocket&, const std::vector<uint8_t>&, beast::error_code)>;
        using AsyncSendCallback = std::function<void(TLSWebSocket&, beast::error_code)>;

        /**
         *  \internal
         *
         *  Construct unconnected WebSocket. use handshake for connection.
         */
        TLSWebSocket(IOService& ioService); 

        /**
         *  \internal
         *
         *  Calls shutdown().
         */
        ~TLSWebSocket();

        /**
         *  \internal
         *
         *  Send message and block until transmittion finished.
         *
         *  \throws beast::system_error on any error.
         *
         *  This method is thread-safe.
         */
        void sendMessage(const std::vector<uint8_t>& message);

        /**
         *  \internal
         *
         *  Read message if any, blocks if there is no message.
         *
         *  \throws beast::system_error on any error.
         *
         *  This method is thread-safe.
         */
        std::vector<uint8_t> readMessage();

        /**
         *  \internal
         *
         *  Asynchronously read message and call callback when done (or error occured).
         *
         *  \warning For now there is no way to cancel this operation.
         *  \warning TLSWebSocket *MUST* be allocated in heap and stored
         *          in std::shared_ptr for this function to work correctly.
         *          Otherwise UB will occur.
         *
         *  This method is NOT thread-safe.
         */
        void asyncReadMessage(AsyncReadCallback callback);

        /**
         *  \internal
         *
         *  Asynchronusly send message and call callback when done (or error occured).
         *
         *  \warning For now there is no way to cancel this operation.
         *  \warning TLSWebSocket *MUST* be allocated in heap and stored
         *          in std::shared_ptr for this function to work correctly.
         *          Otherwise UB will occur.
         *
         *  This method is NOT thread-safe.
         */
        void asyncSendMessage(const std::vector<uint8_t>& message, AsyncSendCallback callback);

        /**
         *  \internal
         *
         *  Perform TCP handshake, TLS handshake and WS handshake.
         *
         *  \throws beast::system_error on any error.
         *
         *  This method is thread-safe.
         */
        void handshake(const std::string& servername, const std::string& path, unsigned short port = 443, const std::unordered_map<std::string, std::string>& additionalHeaders = {});

        /**
         *  \internal
         *
         *  Discard any remaining messages until close frame and teardown TCP connection.
         *  Error occured while when closing is ignored. TLSWebSocket instance is no longer
         *  usable after this call.
         *
         *  This method is thread-safe.
         */
        void shutdown(websocket::close_code reason = websocket::close_code::normal);

        bool isSocketOpen() const {
            return wsStream.lowest_layer().is_open();
        }

        ssl::context tlsContext;
        WSSStream wsStream;
    private:
        tcp::resolver::iterator resolutionResult;

        const std::string servername;
        std::mutex connectionMutex;
    };
}

#endif // HEXICORD_WSS_HPP
