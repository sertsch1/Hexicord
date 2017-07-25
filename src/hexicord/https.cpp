#include "hexicord/https.hpp"

#include <boost/asio.hpp>       // boost::asio::ip::tcp
#include <boost/asio/ssl.hpp>   // boost::asio::ssl
#include <beast/core.hpp>       // beast::flat_buffer
#include <beast/http.hpp>       // beast::http

using namespace Hexicord::__REST;

// Shut up YCM.
#ifndef HEXICORD_VERSION
    #define HEXICORD_VERSION "0.0.0"
#endif

// Because nobody likes long names.
using tcp  = boost::asio::ip::tcp;
namespace ssl  = boost::asio::ssl;
namespace http = beast::http;

const std::string COMODO_CA = 
"-----BEGIN CERTIFICATE-----\
MIIFTzCCBDegAwIBAgIRALOVDwH1AvQ/pCD9Y0gXxYwwDQYJKoZIhvcNAQELBQAw\
gZAxCzAJBgNVBAYTAkdCMRswGQYDVQQIExJHcmVhdGVyIE1hbmNoZXN0ZXIxEDAO\
BgNVBAcTB1NhbGZvcmQxGjAYBgNVBAoTEUNPTU9ETyBDQSBMaW1pdGVkMTYwNAYD\
VQQDEy1DT01PRE8gUlNBIERvbWFpbiBWYWxpZGF0aW9uIFNlY3VyZSBTZXJ2ZXIg\
Q0EwHhcNMTcwMjI0MDAwMDAwWhcNMjAwMzExMjM1OTU5WjBSMSEwHwYDVQQLExhE\
b21haW4gQ29udHJvbCBWYWxpZGF0ZWQxFDASBgNVBAsTC1Bvc2l0aXZlU1NMMRcw\
FQYDVQQDEw5kaXNjb3JkYXBwLmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC\
AQoCggEBAKEmdstX25Z0h4E7jazhSVsxHQyN9r5rMjs2h08ycA2CdrTFKa/CJIvS\
oVRIDDav4CJv+ebXY2jLfaLRxJBvUkfPdPS3G8oN3+oOppYogsJdlv/K0vxRpW7Y\
n73apOuZMqErq1bA52L17w8pm8igtvGTWMfs0fIXIC0VeEQTlB+YQfW51kV9uKkG\
CVzXUW7mZMeukS2ekRlZas806fmeiJ/KJx9WjmFjW19KS7CQpew7Pz/FI6PPYwix\
F6gVZqBW1EdL9RLQcOsufELvYDmj5cijV2qa4DbAMwWmx7+VFX40JGIz/H3/HNbE\
wYDReWcpcHl3AgHj0CBr0zjhPmVCK0cCAwEAAaOCAd8wggHbMB8GA1UdIwQYMBaA\
FJCvajqUWgvYkOoSVnPfQ7Q6KNrnMB0GA1UdDgQWBBQddlJKOkCu7RA/I8/Id2rp\
tXLNoTAOBgNVHQ8BAf8EBAMCBaAwDAYDVR0TAQH/BAIwADAdBgNVHSUEFjAUBggr\
BgEFBQcDAQYIKwYBBQUHAwIwTwYDVR0gBEgwRjA6BgsrBgEEAbIxAQICBzArMCkG\
CCsGAQUFBwIBFh1odHRwczovL3NlY3VyZS5jb21vZG8uY29tL0NQUzAIBgZngQwB\
AgEwVAYDVR0fBE0wSzBJoEegRYZDaHR0cDovL2NybC5jb21vZG9jYS5jb20vQ09N\
T0RPUlNBRG9tYWluVmFsaWRhdGlvblNlY3VyZVNlcnZlckNBLmNybDCBhQYIKwYB\
BQUHAQEEeTB3ME8GCCsGAQUFBzAChkNodHRwOi8vY3J0LmNvbW9kb2NhLmNvbS9D\
T01PRE9SU0FEb21haW5WYWxpZGF0aW9uU2VjdXJlU2VydmVyQ0EuY3J0MCQGCCsG\
AQUFBzABhhhodHRwOi8vb2NzcC5jb21vZG9jYS5jb20wLQYDVR0RBCYwJIIOZGlz\
Y29yZGFwcC5jb22CEnd3dy5kaXNjb3JkYXBwLmNvbTANBgkqhkiG9w0BAQsFAAOC\
AQEAC8MUE0NSvwnz9pthtitu4l69FUVZ5Gy3s9Bsz4DNDDjPBUm9nBDTvPKNNyqw\
SqADXGPh1xNnpzHTGStjclrkKmU0uxn+ci7oWduHn5fUzBLT/4j5ivI5/k//ErJF\
fHwFjYvXWiVoZHrShL3w0ul9cy1vNeq35bv/ATBct/4nz1qMy4n3Wuwr6w9F0EGF\
4KBWKrxVpcyzz0I5Kzpq7ChFLu0KR26Nvt71dZ2euZ7E/38vNX2xWMAFnlE+fh+l\
/YYUnWEggVnyJkqStKETbWsstAdcEb5RqRdoDzlek9Y+PQs/bBJDc3yGDDxKE3Gn\
W8IJKD+UiW8LGWW2r16MvxLSEg==\
-----END CERTIFICATE-----";

HTTPResponse httpsRequest(IOService& ioService, const std::string& servername,
                          const http::verb& method, const std::string& path,
                          const Headers& headers, const Bytes& body) {

    tcp::resolver resolver(ioService);
    tcp::socket   socket(ioService);
    ssl::context  tlsctx(ssl::context::tlsv12_client);

    const auto lookup_result = resolver.resolve({ servername, "https" });
    // TODO: Gather status code in case of fail and throw exception.
    
    boost::asio::connect(socket, lookup_result);
    // TODO: Handle errors here too.
    
    tlsctx.add_certificate_authority(boost::asio::buffer(COMODO_CA.data(), COMODO_CA.size()));
    // TODO: Handle errors.
    
    ssl::stream<tcp::socket&> stream(socket, tlsctx);
    stream.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
    stream.handshake(ssl::stream_base::client);
    // TODO: Errors.
    
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
    // TODO: Errors.
    
    beast::flat_buffer buffer;
    http::response<http::vector_body<uint8_t> > response;

    HTTPResponse response_struct;
    response_struct.status_code = response.result_int();
    response_struct.body        = response.body;

    stream.shutdown();
    // TODO: Errors may happen here too.
    
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

