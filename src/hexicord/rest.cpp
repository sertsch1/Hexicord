#include "hexicord/rest.hpp"

#include <iostream>             // std::cerr
#include <boost/asio.hpp>       // boost::asio::ip::tcp
#include <boost/asio/ssl.hpp>   // boost::asio::ssl
#include <beast/core.hpp>       // beast::flat_buffer
#include <beast/http.hpp>       // beast::http

namespace Hexicord { namespace REST {

// Shut up YCM.
#ifndef HEXICORD_GITHUB
    #define HEXICORD_GITHUB ""
#endif
#ifndef HEXICORD_VERSION
    #define HEXICORD_VERSION "0.0.0"
#endif

// Because nobody likes long names.
using tcp  = boost::asio::ip::tcp;
namespace ssl  = boost::asio::ssl;
namespace http = beast::http;

HTTPResponse httpsRequest(IOService& ioService, const std::string& servername,
                          http::verb method, const std::string& path,
                          const Headers& headers, const Bytes& body) {
    tcp::resolver resolver(ioService);
    tcp::socket   socket(ioService);

    beast::error_code ec;

    boost::asio::connect(socket, resolver.resolve({ servername, "https" }));
    socket.set_option(tcp::no_delay(true));

    ssl::context tlsctx(ssl::context::tlsv12_client);
    tlsctx.set_default_verify_paths();

    ssl::stream<tcp::socket&> stream(socket, tlsctx);
    stream.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
    stream.set_verify_callback(ssl::rfc2818_verification(servername));
    stream.handshake(ssl::stream_base::client);

    http::request<http::vector_body<uint8_t> > request;
    request.method(method);
    request.target(path);
    request.version = 11;
    request.set(http::field::host, servername);
    request.set(http::field::user_agent, "DiscordBot (" HEXICORD_GITHUB ", " HEXICORD_VERSION ")");
    request.set(http::field::connection, "close"); // prevent server from expecting that we will not break connection, sadly most servers just ignore it.
    request.set(http::field::accept,     "*/*"); // "we accept anything", expected to be replaced in headers arguments.
    if (body.size()) {
        request.body = body;
        request.set(http::field::content_length, std::to_string(body.size()));
        request.set(http::field::content_type,   "application/octet-stream"); // fallback type, expected to be replaced in headers argument by real type.
    }
    for (const auto& header : headers) {
        request.set(header.first, header.second);
    }
    request.prepare_payload();

    http::write(stream, request, ec);
    if (ec && ec != beast::http::error::end_of_stream) throw beast::system_error(ec);

    beast::flat_buffer buffer;
    http::response<http::vector_body<uint8_t> > response;
    http::read(stream, buffer, response);

    stream.shutdown(ec);
    // stream_truncated is ignored as a workaround for non-fixed boost.asio bug.
    // Boost developers is retarded ~~ fox.cpp.
    // https://svn.boost.org/trac10/ticket/12710
    if (ec && ec != boost::asio::error::eof && ec != boost::asio::ssl::error::stream_truncated) throw beast::system_error(ec); 

    HTTPResponse responseStruct;
    responseStruct.statusCode = response.result_int();
    responseStruct.body       = response.body;
    for (const auto& header : response) {
        responseStruct.headers.insert({ header.name_string().to_string(), header.value().to_string() });
    }

    return responseStruct;
}

HTTPResponse get(IOService& ioService, const std::string& servername,
                 const std::string& path, const Headers& headers) {

    return httpsRequest(ioService, servername, http::verb::get, path, headers, /* body: */ {});
}

HTTPResponse post(IOService& ioService, const std::string& servername,
                  const std::string& path, const Headers& headers, const Bytes& body) {

    return httpsRequest(ioService, servername, http::verb::post, path, headers, body);
}

HTTPResponse put(IOService& ioService, const std::string& servername,
                 const std::string& path, const Headers& headers, const Bytes& body) {

    return httpsRequest(ioService, servername, http::verb::put, path, headers, body);
}

HTTPResponse delete_(IOService& ioService, const std::string& servername,
                     const std::string& path, const Headers& headers) {

    return httpsRequest(ioService, servername, http::verb::delete_, path, headers, /* body */ {});
}

}} // namespace Hexicord::REST
