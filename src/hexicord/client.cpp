#include <hexicord/client.hpp>
#include <thread>
#include <boost/date_time/posix_time/posix_time.hpp>

#ifndef NDEBUG // TODO: Replace with flag that affects only Hexicord.
    #include <iostream>
    #define DEBUG_MSG(msg) do { std::cerr << __FILE__ << ":" << __LINE__ << " " << (msg) << '\n'; } while (false)
#else
    #define DEBUG_MSG(msg)
#endif

namespace Hexicord {
    Client::Client(boost::asio::io_service& ioService, const std::string& token) 
        : restConnection(new REST::GenericHTTPConnection<BeastHTTPS>("discordapp.com", ioService))
        , gatewayConnection(new TLSWebSocket(ioService))
        , token(token)
        , ioService(ioService)
        , keepaliveTimer(ioService)
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
        return { response["url"].get<std::string>(), response["shards"].get<unsigned>() };
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
        if (!payload.is_null() && !payload.empty()) {
            request.headers.insert({ "Content-Type", "application/json" });
            request.body = jsonToVector(payload);
        }
        request.headers.insert({ "Accept", "application/json" });

        activeRestRequest = true;
        REST::HTTPResponse response;
        try {
            DEBUG_MSG(std::string("Sending REST request: ") + method + " " + endpoint);
            response = restConnection->request(request);
        } catch (boost::system::system_error& excp) {
            activeRestRequest = false;
            if (excp.code() != boost::beast::http::error::end_of_stream) throw;

            DEBUG_MSG("HTTP Connection closed by remote. Reopenning and retrying.");
            restConnection->close();

            REST::HeadersMap prevHeaders = std::move(restConnection->connectionHeaders);
            // we should not reuse socket.
            restConnection.reset(new REST::GenericHTTPConnection<BeastHTTPS>("discordapp.com", ioService));
            restConnection->connectionHeaders = std::move(prevHeaders);
            restConnection->open();
            
            response = restConnection->request(request);
        }

        if (response.body.empty()) {
            return {};
        }

        nlohmann::json jsonResp = vectorToJson(response.body);

        if (response.statusCode / 100 != 2) {
            DEBUG_MSG("Got non-2xx HTTP status code.");
            int code = -1;
            std::string message;
            if (jsonResp.find("code") != jsonResp.end()) code = jsonResp["code"];
            if (jsonResp.find("message") != jsonResp.end()) {
                message = std::string("API error: ") + jsonResp["message"].get<std::string>();
            } else if (jsonResp.find("_misc") != jsonResp.end() && jsonResp["_misc"].is_array() &&
                       jsonResp["_misc"].size() == 1){
                message = std::string("Internal API error: ") + jsonResp["_misc"][0].get<std::string>();
            } else {
                message = "Unknown API error";
            }
            throw APIError(message, code, response.statusCode);
        }

        return jsonResp;
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
        } catch (boost::system::system_error& excp) {
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

    nlohmann::json Client::getChannel(uint64_t channelId) {
        return sendRestRequest("GET", std::string("/channels/") + std::to_string(channelId));
    }

    nlohmann::json Client::modifyChannel(uint64_t channelId,
                                         boost::optional<std::string> name,
                                         boost::optional<int> position,
                                         boost::optional<std::string> topic,
                                         boost::optional<unsigned> bitrate,
                                         boost::optional<unsigned short> usersLimit) {
        nlohmann::json payload;
        if (name) {
            if (name->size() > 100 || name->size() < 2) {
                throw std::out_of_range("name size out of range (should be 2-100).");
            }
            payload.emplace("name", *name);
        }
        if (position) {
            payload.emplace("position", *position);
        }
        if (topic && (bitrate || usersLimit)) {
            throw std::invalid_argument("Passing both voice-only and text-only channel arguments.");
        }
        if (topic) {
            if (topic->size() > 1024) {
                throw std::out_of_range("topic size out of range (should be 0-1024).");
            }
            payload.emplace("topic", *topic);
        }
        if (bitrate) {
            if (*bitrate < 8000 || *bitrate > 128000) {
                throw std::out_of_range("bitrate out of range (should be 8000-128000).");
            }
            payload.emplace("bitrate", *bitrate);
        }
        if (usersLimit) {
            if (*usersLimit > 99) {
                throw std::out_of_range("usersLimit out of range (should be 0-99).");
            }
            payload.emplace("users_limit", *usersLimit);
        }
        if (payload.empty()) {
            throw std::invalid_argument("No arguments passed to modifyChannel.");
        }

        return sendRestRequest("POST", std::string("/channels/") + std::to_string(channelId), payload);
    }

    nlohmann::json Client::deleteChannel(uint64_t channelId) {
        return sendRestRequest("DELETE", std::string("/channels/") + std::to_string(channelId));
    }

    nlohmann::json Client::getChannelMessages(uint64_t channelId, uint64_t startMessageId,
                                              GetMsgMode mode, unsigned short limit) {

        std::string queryString;
        queryString.reserve(/* approximate length of query keys */ 9 + 
                            /* approximate snowflake length */ 20 + 
                            /* limit value length */ 3);
        queryString.push_back('?'); // appended after reserve to avoid first allocation.

        switch (mode) {
        case Around:
            queryString += "around=";
            break;
        case Before:
            queryString += "before=";
            break;
        case After:
            queryString += "after=";
            break;
        }
        queryString += std::to_string(startMessageId);

        if (limit != 50) { // 50 is default value in v6, no need to pass it explicitly.
            if (limit > 100 || limit == 0) {
                throw std::out_of_range("limit out of range (should be 1-100).");
            }
            if (mode == After && limit == 1) {
                throw std::out_of_range("limit out of range (should be 2-100 for After mode).");
            }
            queryString += "&limit=";
            queryString += std::to_string(limit);
        }

        return sendRestRequest("GET", std::string("/channels/") + std::to_string(channelId) +
                               "/messages" + queryString);
    }

    nlohmann::json Client::getChannelMessage(uint64_t channelId, uint64_t messageId) {
        return sendRestRequest("GET", std::string("/channels/") + std::to_string(channelId) + 
                               "/messsages/" + std::to_string(messageId));
    }

    nlohmann::json Client::sendMessage(uint64_t channelId, const std::string& text, bool tts,
                                       boost::optional<uint64_t> nonce) {
        if (text.size() > 2000) {
            throw std::out_of_range("text out of range (should be 0-1024).");
        }

        nlohmann::json payload;
        payload.emplace("content", text);
        if (tts) payload.emplace("tts", tts);
        if (nonce) payload.emplace("nonce", *nonce);

        return sendRestRequest("POST", std::string("/channels/") + std::to_string(channelId) + "/messages",
                               payload);
    }

    nlohmann::json Client::editMessage(uint64_t channelId, uint64_t messageId, const std::string& text) {
        if (text.size() > 2000) {
            throw std::out_of_range("text size out of range (should be 0-1024)");
        }
        return sendRestRequest("PATCH", std::string("/channels/") + std::to_string(channelId) +
                               "/messages/" + std::to_string(messageId),
                               {{ "content", text }});
    }

    void Client::deleteMessage(uint64_t channelId, uint64_t messageId) {
        sendRestRequest("DELETE", std::string("/channels/") + std::to_string(channelId) + "/messages/" + std::to_string(messageId));
    }

    void Client::deleteMessages(uint64_t channelId, const std::vector<uint64_t>& messageIds) {
        sendRestRequest("DELETE", std::string("/channels/") + std::to_string(channelId) + "/messages/bulk-delete", {{ "messages", messageIds }});
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
        gatewayConnection->asyncReadMessage([this](TLSWebSocket&, const std::vector<uint8_t>& body, boost::system::error_code ec) {
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
                if (!activeRestRequest) {
                    restConnection->request({ "HEAD", "/", 11, {}, {} });
                }
            } catch (boost::system::system_error& excp) {
                if (excp.code() == boost::beast::http::error::end_of_stream) {
                    restConnection->close();
                    // we should not reuse socket.
                    REST::HeadersMap prevHeaders = std::move(restConnection->connectionHeaders);
                    restConnection.reset(new REST::GenericHTTPConnection<BeastHTTPS>("discordapp.com", ioService));
                    restConnection->connectionHeaders = std::move(prevHeaders);
                    restConnection->open();
                } else {
                    throw;
                }
            }

            startRestKeepaliveTimer();
        });
    }
} // namespace Hexicord
