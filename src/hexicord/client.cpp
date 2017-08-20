#include <hexicord/client.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <hexicord/internal/utils.hpp>

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
        , heartbeatTimer(ioService) {

        // It's strange but Discord API requires "DiscordBot" user-agent for any connections
        // including non-bots. Referring to https://discordapp.com/developers/docs/reference#user-agent
        restConnection->connectionHeaders.insert({ "User-Agent", 
                "DiscordBot (" HEXICORD_GITHUB ", " HEXICORD_VERSION ")" });
    }

    Client::~Client() {
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

    void Client::resumeGatewaySession(const std::string& gatewayUrl, const std::string& token,
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

    void Client::connectToGateway(const std::string& gatewayUrl, int shardId, int shardCount,
                                  const nlohmann::json& initialPresense) {
        // Set if we didn't done it already. Since Client may be actually used without prior call
        // to getGatewayUrlBot because of sharding.
        restConnection->connectionHeaders.insert({ "Authorization", std::string("Bot ") + token });

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

    nlohmann::json Client::sendRestRequest(const std::string& method, const std::string& endpoint,
                                           const nlohmann::json& payload, const std::unordered_map<std::string, std::string>& query) {
        if (!restConnection->isOpen()) {
            restConnection->open();
        }

        // consturct query string if any.
        std::string queryString;
        if (!query.empty()) {
            queryString += '?';
            for (auto queryParam : query) {
                queryString += queryParam.first;
                queryString += '=';
                queryString += Utils::urlEncode(queryParam.second);
                queryString += '&';
            }
            queryString.pop_back(); // remove extra & at end.
        }

        REST::HTTPRequest request;

        request.method  = method;
        request.path    = restBasePath + endpoint + queryString;
        request.version = 11;
        if (!payload.is_null() && !payload.empty()) {
            request.headers.insert({ "Content-Type", "application/json" });
            request.body = jsonToVector(payload);
        }
        request.headers.insert({ "Accept", "application/json" });

        REST::HTTPResponse response;
        try {
            DEBUG_MSG(std::string("Sending REST request: ") + method + " " + endpoint + queryString + " " + payload.dump());
            response = restConnection->request(request);
        } catch (boost::system::system_error& excp) {
            if (excp.code() != boost::beast::http::error::end_of_stream &&
                excp.code() != boost::asio::error::broken_pipe &&
                excp.code() != boost::asio::error::connection_reset) throw;

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
            DEBUG_MSG(jsonResp.dump(4));
            int code = -1;
            std::string message;
            if (jsonResp.find("code") != jsonResp.end()) code = jsonResp["code"];
            if (jsonResp.find("message") != jsonResp.end()) {
                message = std::string("API error: ") + jsonResp["message"].get<std::string>();
            } else {
                for (auto param : payload.get<std::unordered_map<std::string, nlohmann::json> >()) {
                    // TODO: Throw something like "InvalidArgument"
                    auto it = jsonResp.find(param.first);
                    if (it != jsonResp.end() && it->size() != 0) {
                        throw APIError(std::string("Invalid argument: ") + param.first + ": " +
                                       it->at(0).get<std::string>());
                    }
                    throw APIError("Unknown API error", code, response.statusCode);
                }
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
            if (excp.code() == boost::asio::error::broken_pipe ||
                excp.code() == boost::asio::error::connection_reset ||
                excp.code() == boost::beast::websocket::error::closed) { // unexpected disconnect

                disconnectFromGateway();
                resumeGatewaySession(lastUsedGatewayUrl, token, sessionId_, lastSeqNumber_);
            }
        }
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
        
        std::unordered_map<std::string, std::string> query;

        switch (mode) {
        case Around:
            query.insert({ "around", std::to_string(startMessageId) });
            break;
        case Before:
            query.insert({ "before", std::to_string(startMessageId) });
            break;
        case After:
            query.insert({ "after",  std::to_string(startMessageId) }); 
            break;
        }

        if (limit != 50) { // 50 is default value in v6, no need to pass it explicitly.
            if (limit > 100 || limit == 0) {
                throw std::out_of_range("limit out of range (should be 1-100).");
            }
            if (mode == After && limit == 1) {
                throw std::out_of_range("limit out of range (should be 2-100 for After mode).");
            }
            query.insert({ "limit", std::to_string(limit) });
        }

        return sendRestRequest("GET", std::string("/channels/") + std::to_string(channelId) + "/messages",
                               query);
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
        sendRestRequest("DELETE", std::string("/channels/") + std::to_string(channelId) + "/messages/" +
                        std::to_string(messageId));
    }

    void Client::deleteMessages(uint64_t channelId, const std::vector<uint64_t>& messageIds) {
        sendRestRequest("DELETE", std::string("/channels/") + std::to_string(channelId) + "/messages/bulk-delete",
                        {{ "messages", messageIds }});
    }

    nlohmann::json Client::getMe() {
        return sendRestRequest("GET", std::string("/users/@me"));
    }

    nlohmann::json Client::getUser(uint64_t id) {
        return sendRestRequest("GET", std::string("/users/") + std::to_string(id));
    }

    nlohmann::json Client::setUsername(const std::string& newUsername) {
        if (newUsername.size() < 2 || newUsername.size() > 32) {
            throw std::out_of_range("newUseranme size out of range (should be 2-32)");
        }
        if (newUsername == "discordtag" || newUsername == "everyone" || newUsername == "here") {
            throw std::invalid_argument("newUsername should not be 'discordtag', 'everyone' or 'here'");
        }
        unsigned short foundGraves = 0;
        for (char ch : newUsername) {
            if (ch == '@' || ch == '#' || ch == ':') {
                throw std::invalid_argument("newUsername contains foribbden characters ('@', '#' or ':')");
            }
            if (ch == '`') {
                ++foundGraves;
                if (foundGraves == 3) {
                    throw std::invalid_argument("newUsername contains forbidden substring: '```'");
                }
            } else {
                foundGraves = 0;
            }
        }

        return sendRestRequest("PATCH", "/users/@me", {{ "username", newUsername }});
    }

    nlohmann::json Client::setAvatar(const std::vector<uint8_t>& avatarBytes, AvatarFormat format) {
        std::string mimeType;

        if (format == Gif  || (format == Detect && Utils::Magic::isGif(avatarBytes)))  mimeType = "image/gif";
        if (format == Jpeg || (format == Detect && Utils::Magic::isJfif(avatarBytes))) mimeType = "image/jpeg";
        if (format == Png  || (format == Detect && Utils::Magic::isPng(avatarBytes)))  mimeType = "image/png";

        if (mimeType.empty()) {
            throw std::invalid_argument("Failed to detect avatar format.");
        }

        std::string dataUrl = std::string("data:") + mimeType + ";base64," + Utils::base64Encode(avatarBytes);
        return sendRestRequest("PATCH", "/users/@me", {{ "avatar", dataUrl }});
    }

    nlohmann::json Client::setAvatar(std::istream&& avatarStream, AvatarFormat format) {
        // TODO: Implement stream sending.
        std::vector<uint8_t> avatarBytes{std::istreambuf_iterator<char>(avatarStream), std::istreambuf_iterator<char>()};
        return setAvatar(avatarBytes, format);
    }

    nlohmann::json Client::getUserGuilds(unsigned short limit, uint64_t startId, bool before) {
        std::unordered_map<std::string, std::string> query;
        if (limit != 100) {
            query.insert({ "limit", std::to_string(limit) });
        }
        if (startId != 0) {
            query.insert({ before ? "before" : "after", std::to_string(startId) });
        }
        return sendRestRequest("GET", "/users/@me/guilds", {}, query);
    }

    void Client::leaveGuild(uint64_t guildId) {
        sendRestRequest("DELETE", std::string("/users/@me/guilds/") + std::to_string(guildId));
    }

    nlohmann::json Client::getUserDms() {
        return sendRestRequest("GET", "/users/@me/channels");
    }

    nlohmann::json Client::createDm(uint64_t recipientId) {
        return sendRestRequest("POST", "/users/@me/channels", {{ "recipient_id", recipientId }});
    }


    nlohmann::json Client::createGroupDm(const std::vector<uint64_t>& accessTokens,
                                         const std::unordered_map<uint64_t, std::string>& nicks) {

        return sendRestRequest("POST", "/users/@me/channels", {{ "access_tokens", accessTokens },
                                                               { "nicks",         nicks        }});
    }

    nlohmann::json Client::getConnections() {
        return sendRestRequest("GET", "/users/@me/connections");
    }

    nlohmann::json Client::getInvite(const std::string& inviteCode) {
        return sendRestRequest("GET", std::string("/invites/") + inviteCode);
    }

    nlohmann::json Client::revokeInvite(const std::string& inviteCode) {
        return sendRestRequest("DELETE", std::string("/invites/") + inviteCode);
    }

    nlohmann::json Client::acceptInvite(const std::string& inviteCode) {
        return sendRestRequest("POST", std::string("/invites/") + inviteCode);
    }

    void Client::startGatewayPolling() {
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
} // namespace Hexicord
