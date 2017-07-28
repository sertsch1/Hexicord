#include "hexicord/https.hpp"

#include <boost/asio.hpp>       // boost::asio::ip::tcp
#include <boost/asio/ssl.hpp>   // boost::asio::ssl
#include <beast/core.hpp>       // beast::flat_buffer
#include <beast/http.hpp>       // beast::http

#include "ca_certs.hpp"

using namespace Hexicord::REST;

// Shut up YCM.
#ifndef HEXICORD_VERSION
    #define HEXICORD_VERSION "0.0.0"
#endif

// Because nobody likes long names.
using tcp  = boost::asio::ip::tcp;
namespace ssl  = boost::asio::ssl;
namespace http = beast::http;

HTTPResponse httpsRequest(IOService& ioService, const std::string& servername,
                          const http::verb& method, const std::string& path,
                          const Headers& headers, const Bytes& body) {

    tcp::resolver resolver(ioService);
    tcp::socket   socket(ioService);
    ssl::context  tlsctx(ssl::context::tlsv12_client);

    const auto lookup_result = resolver.resolve({ servername, "https" });
        
    boost::asio::connect(socket, lookup_result);

    for (const auto& cert : caCerts) {
        tlsctx.add_certificate_authority(boost::asio::buffer(cert.data(), cert.size()));
    }
        
    ssl::stream<tcp::socket&> stream(socket, tlsctx);
    stream.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
    stream.handshake(ssl::stream_base::client);
    
    http::request<http::vector_body<uint8_t> > request;
    request.method(method);
    request.target(path);
    request.version = 10;
    if (body.size()) {
        request.body = body;
    }
    for (const auto& header : headers) {
        request.set(header.first, header.second);
    }
    request.prepare_payload();

    http::write(stream, request);
        
    beast::flat_buffer buffer;
    http::response<http::vector_body<uint8_t> > response;

    HTTPResponse response_struct;
    response_struct.status_code = response.result_int();
    response_struct.body        = response.body;

    stream.shutdown();
        
    return response_struct;
}

HTTPResponse httpsGet(IOService& ioService, const std::string& servername,
                      const std::string& path, Headers headers) {
    headers.insert({http::field::user_agent, "DiscordBot (https://github.com/HexwellC/Hexicord, 0.0)"});

    return httpsRequest(ioService, servername, http::verb::get, path, headers, /* body: */ {});
}

HTTPResponse httpsPost(IOService& ioService, const std::string& servername,
                       const std::string& path, Headers headers, const Bytes& body) {
    headers.insert({http::field::user_agent, "DiscordBot (https://github.com/HexwellC/Hexicord, " HEXICORD_VERSION ")"});
    if (body.size() != 0) {
        headers.insert({http::field::content_length, std::to_string(body.size())}); // XXX: This will not compile on MinGW.
    }

    return httpsRequest(ioService, servername, http::verb::post, path, headers, body);
}

HTTPResponse httpsPut(IOService& ioService, const std::string& servername,
                      const std::string& path, Headers headers, const Bytes& body) {
    headers.insert({http::field::user_agent, "DiscordBot (https://github.com/HexwellC/Hexicord, " HEXICORD_VERSION ")"});
    if (body.size() != 0) {
        headers.insert({http::field::content_length, std::to_string(body.size())}); // XXX: This will not compile on MinGW.
    }

    return httpsRequest(ioService, servername, http::verb::put, path, headers, body);
}

HTTPResponse httpsDelete(IOService& ioService, const std::string& servername,
                         const std::string& path, Headers headers, const Bytes& body) {
    headers.insert({http::field::user_agent, "DiscordBot (https://github.com/HexwellC/Hexicord, " HEXICORD_VERSION ")"});
    
    return httpsRequest(ioService, servername, http::verb::delete_, path, headers, body);
}

