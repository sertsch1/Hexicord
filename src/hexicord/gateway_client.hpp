#ifndef HEXICORD_GATEWAY_CLIENT_HPP
#define HEXICORD_GATEWAY_CLIENT_HPP

#include <string>
#include <vector>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <hexicord/json.hpp>
#include <hexicord/event_dispatcher.hpp>
#include <hexicord/internal/wss.hpp>

namespace Hexicord {
    /**
     *  Thrown if gateway API error occurs (disconnect, OP 9 Invalid Session, unexcepted message, etc).
     */
    struct GatewayError : public std::runtime_error {
        GatewayError(const std::string& message, int disconnectCode = -1)
            : std::runtime_error(message)
            , disconnectCode(disconnectCode) {}

        /**
         *  Contains gateway disconnect code if error caused
         *  by disconnection, -1 otherwise.
         */
        const int disconnectCode;
    };

    /**
     *  Op-codes valid for gateway API.
     */
    namespace GatewayOpCodes {
        /// Event dispatch (sent by server).
        constexpr int EventDispatch = 0;
        /// Ping checking (sent by client).
        constexpr int Heartbeat = 1;
        /// Client handshake (sent by client).
        constexpr int Identify = 2;
        /// Client status update (sent by server).
        constexpr int StatusUpdate = 3;
        /// Join/move/leave voice channels (sent by client).
        constexpr int VoiceStateUpdate = 4;
        /// Voice ping checking (sent by client).
        constexpr int VoiceServerPing = 5;
        /// Resume closed connection (sent by client).
        constexpr int Resume = 6;
        /// Request client to reconnect (sent by server).
        constexpr int Reconnect = 7;
        /// Request guild members (sent by client).
        constexpr int RequestGuildMembers = 8;
        /// Notify client they have an invalid session id.
        constexpr int InvalidSession = 9;
        /// Sent immediately after connecting (sent by server).
        constexpr int Hello = 10;
        /// Sent immediately following a client heartbeat that was received (sent by server).
        constexpr int HeartbeatAck = 11;
    }

    class GatewayClient {


        static constexpr int NoSharding = -1;

        GatewayClient(boost::asio::io_service& ioService, const std::string& token); 

        ~GatewayClient();

        /**
         *  User-induced resume of gateway session. Can be useful if you know that 
         *  connection is lost because of bot crash for example and want to recover
         *  and replay lost events.
         *
         *  \param gatewayUrl       gateway URL to use.
         *  \param token            token used in resuming session.
         *  \param sessionId        resuming session ID.
         *  \param lastSeq          sequence number of last received event.
         *
         *  To resume session from this Client instance you can call resumeGateway as following:
         *  ```cpp
         *      client.resumeGatewaySession(client.lastGatewayUrl(), client.token, client.sessionId(), client.lastSeqNumber());
         *  ```
         *
         *  \sa \ref connectToGateway
         */
        void resumeGatewaySession(const std::string& gatewayUrl, const std::string& token, std::string sessionId, int lastSeq);

        /**
         *  Connect to gateway, identify and start listening for events.
         *
         *  \note Pass NoSharding to shardId and shardCount if you're creating regular client (not bot).
         *
         *  \param gatewayUrl       gateway url string acquired by calling getGatewayUrl.
         *  \param shardId          this shard id, use NoSharding if you don't use sharding.
         *                          Defaults to NoSharding.
         *  \param shardCount       total shards count, use Flags::NoSharding if you don't use sharding.
         *                          Defaults to NoSharding.
         *  \param initialPresense  JSON presense object. 
         *                          See https://discordapp.com/developers/docs/topics/gateway#gateway-status-update
         *                          Defaults to online status without game.
         *
         *  \sa \ref resumeGatewaySession
         */
        void connectToGateway(const std::string& gatewayUrl,
                                 /* sharding info: */ int shardId = NoSharding, int shardCount = NoSharding,
                                const nlohmann::json& initialPresense = {{ "game", nullptr },
                                                                         { "status", "online" },
                                                                         { "since", nullptr },
                                                                         { "afk", false }});

        /**
         *  Send Close event and disconnect.
         *
         *  \param code     code to pass in event payload.
         */
        void disconnectFromGateway(int code = 2000);

        /**
         *  Send raw gateway message.
         *
         *  \note This method can be used in case of API update that
         *        adds new op-codes. It's better to use concrete methods
         *        instead of raw messages if avaliable.
         *
         *  \param opCode   Gateway op-code. See \ref GatewayOpCodes namespace
         *                  for constants.
         *  \param payload  Event payload, defaults to {}.
         *  \param t        Event field type, defaults to empty string.
         *
         *  \sa \ref sendRestRequest
         */
        void sendGatewayMsg(int opCode, const nlohmann::json& payload = {}, const std::string& t = "");

        /**
         *  Can be chagned if you need different gateway API version.
         *  Avoid unless really necessary since it can affect beavior
         *  of event handling.
         *
         *  \sa \ref sendGatewayMsg \ref restBasePath
         */
        std::string gatewayPathSuffix = "/?v=6&encoding=json";

        /**
         *  Events dispatcher used to dispatch gateway events.
         */
        EventDispatcher eventDispatcher;

        /**
         *  Used authorization token.
         */
        const std::string token;

        /**
         *  Get current gateway session ID.
         *  Empty if not connected to gateway yet.
         *
         *  \sa \ref resumeGatewaySession \ref lastSeqNumber \ref lastGatewayUrl
         */
        inline const std::string& sessionId() const {
            return sessionId_;
        }

        /**
         *  Get last received event sequence number.
         *  -1 if not connected to gateway yet.
         *
         *  \sa \ref resumeGatewaySession sessionId lastGatewayUrl
         */
        inline int lastSeqNumber() const {
            return lastSeqNumber_;
        }

        /**
         *  Get last used gateway URL. 
         *  Empty if not connected to gateway yet.
         *
         *  \sa \ref resumeGatewaySession \ref lastSeqNumber \ref sessionId
         */
        inline const std::string& lastGatewayUrl() const {
            return lastUsedGatewayUrl;
        }
private:
        int lastSeqNumber_ = -1;

        inline nlohmann::json readGatewayMessage() {
            // TODO: de-compression of compressed payload may performed here.
            return vectorToJson(gatewayConnection->readMessage());
        }
        
        static inline nlohmann::json vectorToJson(const std::vector<uint8_t>& data) {
            return nlohmann::json::parse(std::string(data.begin(), data.end()));
        }

        static inline std::vector<uint8_t> jsonToVector(const nlohmann::json& json) {
            std::string dump = json.dump();
            return std::vector<uint8_t>(dump.begin(), dump.end());
        }

        void startGatewayPolling();
        void startGatewayHeartbeat();

        void sendHeartbeat();

        std::string sessionId_;
        std::string lastUsedGatewayUrl;

        unsigned heartbeatIntervalMs;
        bool heartbeat = true, gatewayPoll = true;
        // gateway sends us HEARTBEAT_ACK after connection (???).
        unsigned unansweredHeartbeats = 1;

        std::unique_ptr<TLSWebSocket> gatewayConnection;
        boost::asio::io_service& ioService; // non-owning reference to I/O service.

        boost::asio::deadline_timer heartbeatTimer;
    };
}

#endif // HEXICORD_GATEWAY_CLIENT_HPP
