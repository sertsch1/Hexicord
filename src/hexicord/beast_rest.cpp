#include "hexicord/beast_rest.hpp"
#include <boost/asio/ssl/rfc2818_verification.hpp>  // boost::asio::ssl::rfc2818_verification.hpp
#include <boost/asio/connect.hpp>
#include <beast/http/write.hpp>                     // beast::http::write
#include <beast/http/read.hpp>                      // beast::http::read
#include <beast/core/flat_buffer.hpp>               // beast::flat_buffer

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
        beast::error_code ec;

        requestType.prepare_payload();
        alive = false;
        beast::http::write(stream, requestType, ec);
        if (ec && ec != beast::http::error::end_of_stream) throw beast::system_error(ec);

        beast::http::response<beast::http::vector_body<uint8_t> > response;
        beast::flat_buffer buffer;
        beast::http::read(stream, buffer, response);

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
        beast::error_code ec;
        stream.shutdown(ec);
        if (ec && ec != boost::asio::error::eof && ec != boost::asio::ssl::error::stream_truncated) {
            throw beast::system_error(ec);
        }
        stream.next_layer().close();
        alive = false;
    }

    bool BeastHTTPS::isOpen() const {
        return stream.next_layer().is_open();
    }
} // namespace Hexicord
