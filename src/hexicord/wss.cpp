#include <hexicord/wss.hpp>

using namespace Hexicord;

TLSWebSocket::TLSWebSocket(IOService& ioService, const std::string& servername, const std::string& path) 
    : tlsContext(ssl::context::tlsv12_client)
    , wsStream(ioService, tlsContext) {

    // DNS resolution.
    tcp::resolver resolver(ioService);
    auto lookupResult = resolver.resolve({ servername, "https" });

    // TCP hanshake.
    boost::asio::connect(wsStream.next_layer().next_layer(), lookupResult);

    for (const auto cert : caCerts) {
        tlsContext.add_certificate_authority(boost::asio::buffer(cert.data(), cert.size()));
    }
    tlsContext.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);

    // TLS handshake.
    wsStream.next_layer().handshake(ssl::stream_base::client);

    // WS handshake.
    wsStream.handshake(servername, "/");
}

TLSWebSocket::~TLSWebSocket() {
    if (wsStream.next_layer().next_layer().is_open()) this->close();
}

void TLSWebSocket::sendMessage(const std::vector<uint8_t>& message) {
    wsStream.write(boost::asio::buffer(message.data(), message.size()));
}

std::vector<uint8_t> TLSWebSocket::readMessage() {
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

void TLSWebSocket::close(websocket::close_code reason) {
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

