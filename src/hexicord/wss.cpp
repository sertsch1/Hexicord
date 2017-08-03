#include "hexicord/wss.hpp"
#include <beast/core/drain_buffer.hpp>
#include <boost/asio/connect.hpp>

using namespace Hexicord;

TLSWebSocket::TLSWebSocket(IOService& ioService, const std::string& servername, unsigned short port) 
    : tlsContext(ssl::context::tlsv12_client)
    , wsStream(ioService, tlsContext) {

    // DNS resolution.
    tcp::resolver resolver(ioService);
    resolutionResult = resolver.resolve({ servername, std::to_string(port) });
    
    tlsContext.set_default_verify_paths();
    tlsContext.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
}

TLSWebSocket::~TLSWebSocket() {
    if (wsStream.lowest_layer().is_open()) this->shutdown();
}

void TLSWebSocket::sendMessage(const std::vector<uint8_t>& message) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    wsStream.write(boost::asio::buffer(message.data(), message.size()));
}

std::vector<uint8_t> TLSWebSocket::readMessage() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    beast::flat_buffer buffer;

    wsStream.read(buffer);

    auto bufferData = boost::asio::buffer_cast<const uint8_t*>(*buffer.data().begin());
    auto bufferSize = boost::asio::buffer_size(*buffer.data().begin());

    return std::vector<uint8_t>(bufferData, bufferData + bufferSize);
}

void TLSWebSocket::asyncReadMessage(TLSWebSocket::AsyncReadCallback callback) {
    std::shared_ptr<beast::flat_buffer> buffer(new beast::flat_buffer);
    auto sharedThis = shared_from_this();

    wsStream.async_read(*buffer, [sharedThis, buffer, callback](beast::error_code ec) {
        // both buffer and shared_this captured by value into lambda. 
        // so they will exist here and hold ownership.
        
        auto bufferData = boost::asio::buffer_cast<const uint8_t*>(*buffer->data().begin());
        auto bufferSize = boost::asio::buffer_size(*buffer->data().begin());

        std::vector<uint8_t> vectorBuffer(bufferData, bufferData + bufferSize);

        callback(*sharedThis, vectorBuffer, ec);

        // however, buffer ownership will be released here and it will
        // removed. shared_this will not.
    });
}

void TLSWebSocket::asyncSendMessage(const std::vector<uint8_t>& message, TLSWebSocket::AsyncSendCallback callback) {
    auto sharedThis = shared_from_this();

    wsStream.async_write(boost::asio::buffer(message.data(), message.size()), [sharedThis, callback] (beast::error_code ec) {
        callback(*sharedThis, ec);
    });
}

void TLSWebSocket::handshake(const std::string& path, const std::unordered_map<std::string, std::string>& additionalHeaders) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    boost::asio::connect(wsStream.lowest_layer(), resolutionResult);
    wsStream.next_layer().handshake(ssl::stream_base::client);
    wsStream.handshake_ex(servername, path, [&additionalHeaders](websocket::request_type& request) {
        for (const auto& header : additionalHeaders) {
            request.set(header.first, header.second);
        }
    });
}

void TLSWebSocket::shutdown(websocket::close_code reason) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    wsStream.close(reason);

    // WebSockets spec. requires us to read all messages until
    // close frame.
    beast::drain_buffer drain;
    beast::error_code ec;
    while (!ec) {
        wsStream.read(drain, ec);
    }

    wsStream.next_layer().shutdown(/* ignored */ ec);
    wsStream.next_layer().next_layer().close();
}

