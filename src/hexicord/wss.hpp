#ifndef HEXICORD_WSS_HPP
#define HEXICORD_WSS_HPP

#include <string>
#include <vector>
#include <memory>
#include <beast/core.hpp>
#include <beast/websocket.hpp>
#include <beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "ca_certs.hpp"

namespace Hexicord {
    namespace ssl = boost::asio::ssl;
    namespace websocket = beast::websocket;
    using tcp = boost::asio::ip::tcp;
    using IOService = boost::asio::io_service;

    /**
     * High-level beast WebSockets wrapper. Provides basic I/O operations:
     * read, send, async read, async write.
     */
    class TLSWebSocket : std::enable_shared_from_this<TLSWebSocket> {
        using TLSStream = ssl::stream<tcp::socket>;
        using WSSStream = websocket::stream<TLSStream>;
    public:
        using AsyncReadCallback = std::function<void(TLSWebSocket&, const std::vector<uint8_t>&, beast::error_code)>;
        using AsyncSendCallback = std::function<void(TLSWebSocket&, beast::error_code)>;

        /**
         * Initialize TCP socket, perform TLS handshake, perform WS handshake. 
         *
         * \throws beast::system_error on any error.
         *
         * \param servername    network address or domain name.
         * \param path          path to WS endpoint.
         */
        TLSWebSocket(IOService& ioService, const std::string& servername, const std::string& path = "/");

        TLSWebSocket(const TLSWebSocket&) = delete;
        TLSWebSocket(TLSWebSocket&&)      = default;

        /**
         * Calls forceClose().
         */
        ~TLSWebSocket();

        /**
         * Send message and block until transmittion finished.
         *
         * \throws beast::system_error on any error.
         */
        void sendMessage(const std::vector<uint8_t>& message);

        /**
         * Read message if any, blocks if there is no message.
         *
         * \throws beast::system_error on any error.
         */
        std::vector<uint8_t> readMessage();

        /**
         * Asynchronously read message and call callback when done (or error occured).
         *
         * \warning For now there is no way to cancel this operation.
         * \warning TLSWebSocket *MUST* be allocated in heap and stored
         *          in std::shared_ptr for this function to work correctly.
         *          Otherwise UB will occur.
         */
        void asyncReadMessage(AsyncReadCallback callback);

        /**
         * Asynchronusly send message and call callback when done (or error occured).
         *
         * \warning For now there is no way to cancel this operation.
         * \warning TLSWebSocket *MUST* be allocated in heap and stored
         *          in std::shared_ptr for this function to work correctly.
         *          Otherwise UB will occur.
         */
        void asyncSendMessage(const std::vector<uint8_t>& message, AsyncSendCallback callback);

        /**
         * Discard any remaining messages until close frame and teardown TCP connection.
         * Error occured while when closing is ignored. TLSWebSocket instance is no longer
         * usable after this call.
         */
        void close(websocket::close_code reason = websocket::close_code::normal);

        ssl::context& getTLSContext() { return tlsContext; }
        const ssl::context& getTLSContext() const { return tlsContext; }

        WSSStream& getWSStream() { return wsStream; }
        const WSSStream& getWSStream() const { return wsStream; }
    private:
        ssl::context tlsContext;
        WSSStream wsStream;
    };
}

#endif // HEXICORD_WSS_HPP
