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

#ifndef HEXICORD_WSS_HPP
#define HEXICORD_WSS_HPP

#include <string>                       // std::string
#include <vector>                       // std::vector
#include <memory>                       // std::enable_shared_from_this
#include <mutex>                        // std::mutex, std::lock_guard
#include <boost/beast/core/error.hpp>         // boost::system::error_code, boost::system::system_error 
#include <boost/beast/websocket/stream.hpp>   // websocket::stream
#include <boost/beast/websocket/ssl.hpp>      // required to use ssl::stream beyond websocket
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
    namespace websocket = boost::beast::websocket;

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
        using AsyncReadCallback = std::function<void(TLSWebSocket&, const std::vector<uint8_t>&, boost::system::error_code)>;
        using AsyncSendCallback = std::function<void(TLSWebSocket&, boost::system::error_code)>;

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
         *  \throws boost::system::system_error on any error.
         *
         *  This method is thread-safe.
         */
        void sendMessage(const std::vector<uint8_t>& message);

        /**
         *  \internal
         *
         *  Read message if any, blocks if there is no message.
         *
         *  \throws boost::system::system_error on any error.
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
         *  \throws boost::system::system_error on any error.
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
