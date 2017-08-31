#include <hexicord/gateway_client.hpp>
#include <hexicord/internal/utils.hpp>

#if defined(HEXICORD_DEBUG_LOG) && defined(HEXICORD_DEBUG_CLIENT)
    #include <iostream>
    #define DEBUG_MSG(msg) do { std::cerr <<  "gateway_client.cpp:" << __LINE__ << " " << (msg) << '\n'; } while (false)
#else
    #define DEBUG_MSG(msg)
#endif

// All we need is C++11 compatibile compiler and boost libraries so we can probably run on a lot of platforms.
#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__GNU__)
    #define OS_STR "linux"
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    #define OS_STR "bsd"
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #define OS_STR "win32"
#elif defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
    #define OS_STR "macos"
#elif defined(__unix__) || defined (__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE)
    #define OS_STR "unix"
#else
    #define OS_STR "unknown"
#endif 

namespace Hexicord {

GatewayClient::GatewayClient(boost::asio::io_service& ioService, const std::string& token) 
    : ioService(ioService), token(token), heartbeatTimer(ioService) {}

void GatewayClient::resumeGatewaySession(const std::string& gatewayUrl, const std::string& token,
                                  std::string sessionId, int lastSeq) {
    DEBUG_MSG(std::string("Resuming interrupted gateway session. sessionId=") + sessionId +
              " lastSeq=" + std::to_string(lastSeq));
   
    if (!gatewayConnection) {
        gatewayConnection = std::unique_ptr<TLSWebSocket>(new TLSWebSocket(ioService));
    }

    DEBUG_MSG("Performing WebSocket handshake...");
    gatewayConnection->handshake(Utils::domainFromUrl(gatewayUrl), gatewayPathSuffix, 443);

    DEBUG_MSG("Reading Hello message.");
    nlohmann::json gatewayHello = readGatewayMessage();
    heartbeatIntervalMs = gatewayHello["d"]["heartbeat_interval"];
    DEBUG_MSG(std::string("Gateway heartbeat interval: ") + std::to_string(heartbeatIntervalMs) + " ms.");
    heartbeatTimer.expires_from_now(boost::posix_time::milliseconds(heartbeatIntervalMs));

    DEBUG_MSG("Sending Resume message...");
    sendGatewayMsg(GatewayOpCodes::Resume, {
            { "token", token },
            { "session_id", sessionId },
            { "seq", lastSeq }
    });

    DEBUG_MSG("Starting gateway polling...");
    startGatewayPolling();
}

void GatewayClient::connectToGateway(const std::string& gatewayUrl, int shardId, int shardCount,
                              const nlohmann::json& initialPresense) {
    if (!gatewayConnection) {
        gatewayConnection = std::unique_ptr<TLSWebSocket>(new TLSWebSocket(ioService));
    }

    DEBUG_MSG("Performing WebSocket handshake...");
    gatewayConnection->handshake(Utils::domainFromUrl(gatewayUrl), gatewayPathSuffix, 443);
   
    DEBUG_MSG("Reading Hello message...");
    nlohmann::json gatewayHello = readGatewayMessage();
    heartbeatIntervalMs = gatewayHello["d"]["heartbeat_interval"];
    DEBUG_MSG(std::string("Gateway heartbeat interval: ") + std::to_string(heartbeatIntervalMs) + " ms.");
    heartbeatTimer.expires_from_now(boost::posix_time::milliseconds(heartbeatIntervalMs));

    nlohmann::json message = {
        { "token" , token },
        { "properties", {
            { "os", OS_STR },
            { "browser", "hexicord" },
            { "device", "hexicord" }
        }},
        { "compress", false }, // TODO: compression support
        { "large_threshold", 250 }, // should be changeble
        { "presense", initialPresense }
    };


    if (shardId != NoSharding && shardCount != NoSharding) {
        message.push_back({ "shard", { shardId, shardCount }});
    }

    DEBUG_MSG("Sending Identify message...");
    sendGatewayMsg(GatewayOpCodes::Identify, message);

    sendGatewayMsg(GatewayOpCodes::Heartbeat, nlohmann::json(lastSeqNumber_));
    startGatewayHeartbeat();

    eventDispatcher.addHandler(Event::Ready, [this](const nlohmann::json& payload) {
        DEBUG_MSG("Connected to gateway successfully.");
        // save session_id for usage in OP 6 Resume.
        this->sessionId_ = payload["session_id"];
    });

    lastUsedGatewayUrl = gatewayUrl;
    startGatewayPolling();
}

void GatewayClient::disconnectFromGateway(int code) {
    DEBUG_MSG(std::string("Disconnecting from gateway... code=") + std::to_string(code));
    try {
        sendGatewayMsg(GatewayOpCodes::EventDispatch, nlohmann::json(code), "CLOSE");
    } catch (...) { // whatever happened - we don't care.
    }

    heartbeat = false;
    heartbeatTimer.cancel();

    gatewayConnection->shutdown();
    gatewayConnection.reset(nullptr);
}

void GatewayClient::startGatewayPolling() {
        gatewayConnection->asyncReadMessage([this](TLSWebSocket&, const std::vector<uint8_t>& body,
                                                   boost::system::error_code ec) {
            if (!gatewayPoll) return;

            if (ec == boost::asio::error::broken_pipe) { // unexpected disconnect.
                disconnectFromGateway(5000);
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
                return;
            }

            nlohmann::json message;
            try {
                message = vectorToJson(body);
            } catch (nlohmann::json::parse_error& excp) {
                // we may fail here because of partially readen message (what means gateway dropped our connection).
                disconnectFromGateway(5000);
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
                startGatewayPolling();
                return;
            }   

            switch (message["op"].get<int>()) {
            case GatewayOpCodes::EventDispatch:
                DEBUG_MSG(std::string("Gateway Event: t=") + message["t"].get<std::string>() +
                          " s=" + std::to_string(message["s"].get<int>()));
                lastSeqNumber_ = message["s"];
         
                eventDispatcher.dispatchEvent(message["t"].get<std::string>(), message["d"]);
                break;
            case GatewayOpCodes::HeartbeatAck:
                DEBUG_MSG("Gateway heartbeat answered.");
                --unansweredHeartbeats;
                break;
            case GatewayOpCodes::Heartbeat:
                DEBUG_MSG("Received heartbeat request.");
                sendGatewayMsg(GatewayOpCodes::Heartbeat, nlohmann::json(lastSeqNumber_));
                ++unansweredHeartbeats;
                break;
            case GatewayOpCodes::Reconnect:
                DEBUG_MSG("Gateway asked us to reconnect...");
                disconnectFromGateway();
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
                break;
            case GatewayOpCodes::InvalidSession:
                DEBUG_MSG("Invalid session error.");
                throw GatewayError("Invalid session.");
                break;
            default:
                DEBUG_MSG("Unexpected gateway message.");
                DEBUG_MSG(message);
            }

            startGatewayPolling();
        });
    }

    void GatewayClient::startGatewayHeartbeat() {
        heartbeatTimer.cancel();
        heartbeatTimer.expires_from_now(boost::posix_time::milliseconds(heartbeatIntervalMs));
        heartbeatTimer.async_wait([this](const boost::system::error_code& ec){
            if (ec == boost::asio::error::operation_aborted) return;
            if (!heartbeat) return;

            sendHeartbeat();

            startGatewayHeartbeat();
        });
    }

    void GatewayClient::sendHeartbeat() {
        if (unansweredHeartbeats >= 2) {
            DEBUG_MSG("Missing gateway heartbeat answer. Reconnecting...");
            disconnectFromGateway(5000);
            resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
            return;
        }

        DEBUG_MSG("Gateway heartbeat sent.");
        sendGatewayMsg(GatewayOpCodes::Heartbeat, nlohmann::json(lastSeqNumber_));
        ++unansweredHeartbeats;
    }

} // namespace Hexicord
