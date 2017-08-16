#include <hexicord/internal/beast_rest.hpp>
#include <boost/asio/ssl.hpp>                       // boost::asio::ssl
#include <boost/asio/ssl/error.hpp>                 // boost::asio::ssl::error
#include <boost/asio/ssl/rfc2818_verification.hpp>  // boost::asio::ssl::rfc2818_verification.hpp
#include <boost/asio/connect.hpp>                   // boost::asio::connect
#include <boost/beast/http/write.hpp>               // boost::beast::http::write
#include <boost/beast/http/read.hpp>                // boost::beast::http::read
#include <boost/beast/core/flat_buffer.hpp>         // boost::beast::flat_buffer

namespace Hexicord {
    namespace ssl = boost::asio::ssl;
    using tcp = boost::asio::ip::tcp;

    const unsigned short BeastHTTPS::DefaultPort = 443;

    BeastHTTPS::BeastHTTPS(const std::string& servername, boost::asio::io_service& ioService, unsigned short port) 
        : tlsctx(ssl::context::tlsv12_client) 
        , stream(ioService, tlsctx) {

        tcp::resolver resolver(ioService);
        resolutionResult = resolver.resolve({ servername, std::to_string(port) });

        tlsctx.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        tlsctx.set_verify_callback(ssl::rfc2818_verification(servername));
        tlsctx.set_default_verify_paths();
    }

    REST::HTTPResponse BeastHTTPS::request(BeastHTTPS::RequestType requestType) {
        boost::system::error_code ec;

        requestType.prepare_payload();
        alive = false;
        boost::beast::http::write(stream, requestType, ec);
        if (ec && ec != boost::beast::http::error::end_of_stream) throw boost::system::system_error(ec);

        boost::beast::http::response<boost::beast::http::vector_body<uint8_t> > response;
        boost::beast::flat_buffer buffer;
        boost::beast::http::read(stream, buffer, response);

        REST::HTTPResponse responseStruct;
        responseStruct.statusCode = response.result_int();
        responseStruct.body      = response.body;
        for (const auto& header : response) {
            responseStruct.headers.insert({ header.name_string().to_string(), header.value().to_string() });
        }
        alive = (response["Connection"].to_string() != "close");
        

        return responseStruct;
    }

    //void BeastHTTPS::asyncRequest(BeastHTTPS::RequestType&& requestType) { 
    //}

    void BeastHTTPS::openConnection() {
        boost::asio::connect(stream.next_layer(), resolutionResult);
        stream.next_layer().set_option(tcp::no_delay(true));
        stream.handshake(ssl::stream_base::client);
        alive = true;
    }

    void BeastHTTPS::closeConnection() {
        boost::system::error_code ec;
        stream.shutdown(ec);
        if (ec && ec != boost::asio::error::eof && ec != boost::asio::ssl::error::stream_truncated) {
            throw boost::system::system_error(ec);
        }
        stream.next_layer().close();
        alive = false;
    }

    bool BeastHTTPS::isOpen() const {
        return stream.next_layer().is_open();
    }
} // namespace Hexicord
