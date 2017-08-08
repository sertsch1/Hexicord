#include "hexicord/client.hpp"
#include <thread>
#include <boost/date_time/posix_time/posix_time.hpp>

#ifndef NDEBUG // TODO: Replace with flag that affects only Hexicord.
    #include <iostream>
    #define DEBUG_MSG(msg) do { std::cerr << (msg) << '\n'; } while (false)
#else
    #define DEBUG_MSG(msg)
#endif

namespace Hexicord {
    Client::Client(boost::asio::io_service& ioService, const std::string& token) 
        : restConnection(new REST::GenericHTTPConnection<BeastHTTPS>("discordapp.com", ioService))
        , gatewayConnection(new TLSWebSocket(ioService))
        , token(token)
        , ioService(ioService)
        , keepaliveTimer(ioService, boost::posix_time::seconds(5))
        , heartbeatTimer(ioService) {

        // It's strange but Discord API requires "DiscordBot" user-agent for any connections
        // including non-bots. Referring to https://discordapp.com/developers/docs/reference#user-agent
        restConnection->connectionHeaders.insert({ "User-Agent", "DiscordBot (" HEXICORD_GITHUB ", " HEXICORD_VERSION ")" });
    }

    Client::~Client() {
        restKeepalive = false;
        keepaliveTimer.cancel();

        if (gatewayConnection) disconnectFromGateway();
    }

    std::string Client::getGatewayUrl() {
        restConnection->connectionHeaders.insert({ "Authorization", std::string("Bearer ") + token });

        nlohmann::json response = sendRestRequest("GET", "/gateway");
        return response["url"];
    }

    std::pair<std::string, int> Client::getGatewayUrlBot() {
        restConnection->connectionHeaders.insert({ "Authorization", std::string("Bot ") + token });

        nlohmann::json response = sendRestRequest("GET", "/gateway/bot");
        return { response["url"], response["shards"] };
    }

    void Client::resumeGatewaySession(const std::string& gatewayUrl, const std::string& token, std::string sessionId, int lastSeq) {
        DEBUG_MSG(std::string("Resuming interrupted gateway session. sessionId=") + sessionId + " lastSeq=" + std::to_string(lastSeq));
       
        if (!gatewayConnection) {
            gatewayConnection = std::unique_ptr<TLSWebSocket>(new TLSWebSocket(ioService));
        }

        DEBUG_MSG("Performing WebSocket handshake...");
        gatewayConnection->handshake(domainFromUrl(gatewayUrl), gatewayPathSuffix, 443);

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

        DEBUG_MSG("Waiting for Resume event...");
        nlohmann::json resumedMsg = readGatewayMessage();
        if (resumedMsg["op"] == GatewayOpCodes::InvalidSession) {
            throw GatewayError("Invalid session ID.");            
        } else if (resumedMsg["op"] == GatewayOpCodes::EventDispatch && resumedMsg["t"] == "RESUMED") {
            DEBUG_MSG("Session resumed.");
        } else {
            // gateway sent something else? WAT. TODO: we have to handle it somehow.
        }

        DEBUG_MSG("Starting gateway polling...");
        startGatewayPolling();
    }

    void Client::connectToGateway(const std::string& gatewayUrl, int shardId, int shardCount, const nlohmann::json& initialPresense) {
        // Set if we didn't done it already. Since Client may be actually used without prior call
        // to getGatewayUrlBot because of sharding.
        restConnection->connectionHeaders.insert({ "Authorization", std::string("Bot ") + token });

        if (!gatewayConnection) {
            gatewayConnection = std::unique_ptr<TLSWebSocket>(new TLSWebSocket(ioService));
        }

        DEBUG_MSG("Performing WebSocket handshake...");
        gatewayConnection->handshake(domainFromUrl(gatewayUrl), gatewayPathSuffix, 443);
       
        DEBUG_MSG("Reading Hello message...");
        nlohmann::json gatewayHello = readGatewayMessage();
        heartbeatIntervalMs = gatewayHello["d"]["heartbeat_interval"];
        DEBUG_MSG(std::string("Gateway heartbeat interval: ") + std::to_string(heartbeatIntervalMs) + " ms.");
        heartbeatTimer.expires_from_now(boost::posix_time::milliseconds(heartbeatIntervalMs));

        nlohmann::json message = {
            { "token" , token },
            { "properties", {
                { "os", "linux" }, // TODO: determine using preprocessing magic
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
        DEBUG_MSG("Starting gateway polling...");
        startGatewayPolling();
    }

    void Client::disconnectFromGateway(int code) {
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

    nlohmann::json Client::sendRestRequest(const std::string& method, const std::string& endpoint, const nlohmann::json& payload) {
        if (!restConnection->isOpen()) {
            restConnection->open();
            startRestKeepaliveTimer();
        }

        REST::HTTPRequest request;

        request.method  = method;
        request.path    = restBasePath + endpoint;
        request.version = 11;
        if (payload != nullptr) {
            request.body = jsonToVector(payload);
        }

        REST::HTTPResponse response;
        try {
            DEBUG_MSG(std::string("Sending REST request: ") + method + " " + endpoint);
            response = restConnection->request(request);
        } catch (beast::system_error& excp) {
            if (excp.code() != beast::http::error::end_of_stream) throw;

            DEBUG_MSG("HTTP Connection closed by remote. Reopenning and retrying.");
            restConnection->close();
            // we should not reuse socket.
            restConnection = std::unique_ptr<REST::GenericHTTPConnection<BeastHTTPS> >(new REST::GenericHTTPConnection<BeastHTTPS>("discordapp.com", ioService));
            restConnection->open();
            
            response = restConnection->request(request);
        }

        nlohmann::json message = vectorToJson(response.body);

        if (response.statusCode != 200) {
            DEBUG_MSG("Got non-200 HTTP status code.");
            throw APIError(message["message"].get<std::string>(), message["code"], response.statusCode);
        }

        return message;
    }

    void Client::sendGatewayMsg(int opCode, const nlohmann::json& payload, const std::string& t) {
        nlohmann::json message = {
            { "op", opCode },
            { "d",  payload }
        };

        if (!t.empty()) {
            message.push_back({ "t", t });
        }
        
        try {
            gatewayConnection->sendMessage(jsonToVector(message));
        } catch (beast::system_error& excp) {
            if (excp.code() == boost::asio::error::broken_pipe) { // unexpected disconnect
                disconnectFromGateway();
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
            }
        }
    }

    void Client::run() {
        std::exception_ptr eptr;
        std::thread second_thread([this, &eptr]() { 
            try {
                this->ioService.run();
            } catch (...) {
                eptr = std::current_exception();
            }
        });
        ioService.run();
        second_thread.join();
        if (eptr) std::rethrow_exception(eptr);
    }

    std::string Client::domainFromUrl(const std::string& url) {
        enum State {
            ReadingSchema,
            ReadingSchema_Colon,
            ReadingSchema_FirstSlash,
            ReadingSchema_SecondSlash,
            ReadingDomain,
            End 
        } state = ReadingSchema;

        std::string result;
        for (char ch : url) {
            switch (state) {
            case ReadingSchema:
                if (ch == ':') {
                    state = ReadingSchema_Colon;
                } else if (!std::isalnum(ch)) {
                    throw std::invalid_argument("Missing colon after schema.");
                }
                break;
            case ReadingSchema_Colon:
                if (ch == '/') {
                    state = ReadingSchema_FirstSlash;
                } else {
                    throw std::invalid_argument("Missing slash after schema.");
                }
                break;
            case ReadingSchema_FirstSlash:
                if (ch == '/') {
                    state = ReadingSchema_SecondSlash;
                } else {
                    throw std::invalid_argument("Missing slash after schema.");
                }
                break;
            case ReadingSchema_SecondSlash:
                if (std::isalnum(ch) || ch == '-' || ch == '_') {
                    state = ReadingDomain;
                    result += ch;
                } else {
                    throw std::invalid_argument("Invalid first domain character.");
                }
                break;
            case ReadingDomain:
                if (ch == '/') {
                    state = End;
                } else {
                    result += ch;
                }
            case End:
                break;
            }
            if (state == End) break;
        }
        return result;
    }

    void Client::startGatewayPolling() {
        gatewayConnection->asyncReadMessage([this](TLSWebSocket&, const std::vector<uint8_t>& body, beast::error_code ec) {
            if (!gatewayPoll) return;

            if (ec == boost::asio::error::broken_pipe) { // unexpected disconnect.
                disconnectFromGateway(5000);
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
                return;
            }

            nlohmann::json message = vectorToJson(body);

            switch (message["op"].get<int>()) {
            case GatewayOpCodes::EventDispatch:
                DEBUG_MSG(std::string("Gateway Event: t=\"") + message["t"].get<std::string>() + "\" s=" + std::to_string(message["s"].get<int>()));
                lastSeqNumber_ = message["s"];
                eventDispatcher.dispatchEvent(message["t"].get<std::string>(), message["d"]);
                break;
            case GatewayOpCodes::HeartbeatAck:
                DEBUG_MSG("Gateway heartbeat answered.");
                --unansweredHeartbeats;
                break;
            case GatewayOpCodes::Reconnect:
                DEBUG_MSG("Gateway asked us to reconnect...");
                disconnectFromGateway();
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
                break;
            default:
                DEBUG_MSG("Unexpected gateway message.");
                DEBUG_MSG(message);
            //GatewayOpCodes::InvalidSession handled outside of this handler.
            }

            startGatewayPolling();
        });
    }

    void Client::startGatewayHeartbeat() {
        heartbeatTimer.expires_from_now(boost::posix_time::milliseconds(heartbeatIntervalMs));
        heartbeatTimer.async_wait([this](const boost::system::error_code& ec){
            if (ec == boost::asio::error::operation_aborted) return;
            if (!heartbeat) return;

            if (unansweredHeartbeats == 2) {
                DEBUG_MSG("Missing gateway heartbeat answer. Reconnecting...");
                disconnectFromGateway(5000);
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
            }

            DEBUG_MSG("Gateway heartbeat sent.");
            sendGatewayMsg(GatewayOpCodes::Heartbeat, nlohmann::json(lastSeqNumber_));
            ++unansweredHeartbeats;

            startGatewayHeartbeat();
        });
    }

    void Client::startRestKeepaliveTimer() {
        keepaliveTimer.expires_from_now(boost::posix_time::seconds(10));
        keepaliveTimer.async_wait([this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted) return;
            if (!restKeepalive) return;

            try {
                restConnection->request({ "HEAD", "/", 11, {}, {} });
            } catch (beast::system_error& excp) {
                if (excp.code() == beast::http::error::end_of_stream) {
                    restConnection->close();
                    // we should not reuse socket.
                    restConnection = std::unique_ptr<REST::GenericHTTPConnection<BeastHTTPS> >(new REST::GenericHTTPConnection<BeastHTTPS>("discordapp.com", ioService));
                    restConnection->open();
                } else {
                    throw;
                }
            }

            startRestKeepaliveTimer();
        });
    }
} // namespace Hexicord
