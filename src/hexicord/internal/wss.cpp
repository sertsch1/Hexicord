#include <hexicord/internal/wss.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/connect.hpp>

namespace Hexicord {

    TLSWebSocket::TLSWebSocket(IOService& ioService) 
        : tlsContext(ssl::context::tlsv12_client)
        , wsStream(ioService, tlsContext) {
    
        tlsContext.set_default_verify_paths();
        tlsContext.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
    }

    TLSWebSocket::~TLSWebSocket() {
        try {
            if (wsStream.lowest_layer().is_open()) this->shutdown();
        } catch (...) {
            // it's a destructor, we should not allow any exceptions.
        }
    }
    
    void TLSWebSocket::sendMessage(const std::vector<uint8_t>& message) {
        std::lock_guard<std::mutex> lock(connectionMutex);
        wsStream.write(boost::asio::buffer(message.data(), message.size()));
    }
    
    std::vector<uint8_t> TLSWebSocket::readMessage() {
        std::lock_guard<std::mutex> lock(connectionMutex);
        boost::beast::flat_buffer buffer;
    
        wsStream.read(buffer);
    
        auto bufferData = boost::asio::buffer_cast<const uint8_t*>(*buffer.data().begin());
        auto bufferSize = boost::asio::buffer_size(*buffer.data().begin());

        return std::vector<uint8_t>(bufferData, bufferData + bufferSize);
    }

    void TLSWebSocket::asyncReadMessage(TLSWebSocket::AsyncReadCallback callback) {
        std::shared_ptr<boost::beast::flat_buffer> buffer(new boost::beast::flat_buffer);

        wsStream.async_read(*buffer, [this, buffer, callback](boost::system::error_code ec, unsigned long length) {
            // buffer captured by value into lambda.
            // so they will exist here and hold ownership.
        
            auto bufferData = boost::asio::buffer_cast<const uint8_t*>(*buffer->data().begin());
            std::vector<uint8_t> vectorBuffer(bufferData, bufferData + length);

            callback(*this, vectorBuffer, ec);

            // however, buffer ownership will be released here and it will
            // removed. 
        });
    }

    void TLSWebSocket::asyncSendMessage(const std::vector<uint8_t>& message, TLSWebSocket::AsyncSendCallback callback) {
        wsStream.async_write(boost::asio::buffer(message.data(), message.size()), [this, callback] (boost::system::error_code ec) {
            callback(*this, ec);
        });
    }

    void TLSWebSocket::handshake(const std::string& servername, const std::string& path, unsigned short port, const std::unordered_map<std::string, std::string>& additionalHeaders) {
        std::lock_guard<std::mutex> lock(connectionMutex);

        tcp::resolver resolver(wsStream.get_io_service());
        resolutionResult = resolver.resolve({ servername, std::to_string(port) });

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

        boost::system::error_code ec;
        wsStream.close(reason, ec);
        if (ec != boost::asio::ssl::error::stream_truncated) {
            throw boost::system::system_error(ec);
        }

        wsStream.next_layer().shutdown(/* ignored */ ec);
        wsStream.next_layer().next_layer().close();
    }
} // namespace Hexicord