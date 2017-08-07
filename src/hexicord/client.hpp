#ifndef HEXICORD_CLIENT_HPP
#define HEXICORD_CLIENT_HPP

#include <utility>
#include <boost/asio/io_service.hpp>
#include <hexicord/event_dispatcher.hpp>
#include <hexicord/wss.hpp>
#include <hexicord/rest.hpp>
#include <hexicord/beast_rest.hpp>

/**
 *  \file client.hpp
 *   
 *   Defines \ref Hexicord::Client class, main class of library.
 */

namespace Hexicord {
    /**
     *  Thrown if REST API error occurs.
     */
    struct APIError : public std::logic_error {
        APIError(const std::string& message, int apiCode = -1, int httpCode = -1) 
            : std::logic_error(message)
            , apiCode(apiCode)
            , httpCode(httpCode) {}

        /**
         *  Contains API error code and HTTP status code.
         *  May be -1 if one is not present.
         */
        const int apiCode, httpCode;
    };

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

    /**
     *  Main Discord API class, represents a client and wraps two persistent
     *  connections: REST and WebSocket.
     *
     *  To get a fully working Client class you need to get gateway URL and
     *  perform handshake with it.
     *
     *  Also you need to create boost::asio::io_service instance and pass
     *  it to constructor.
     *
     *  Most basic bot variant without sharding and other stuff would look 
     *  like this (Hexicord namespace omitted):
     *  ```cpp
     *      boost::asio::io_service ios;
     *      Client client(ios, "TTTTTOOOOOOOOKKKKKEEEEEENNNNN");
     *      
     *      // register your handlers here.
     *      client.eventsDispatcher.addHandler(Event::Ready, [](const nlohmann::json&) {
     *          // do stuff...
     *      });
     *
     *      client.connectToGateway(client.getGatewayUrlBot());
     *      ios.run();
     *  ```
     *
     *  ### Sharding (for bots)
     *
     *  As bots grow and are added to an increasing number of guilds, some 
     *  developers may find it necessary to break or split portions of their
     *  bots operations into separate logical processes. As such, Discord 
     *  gateways implement a method of user-controlled guild-sharding which 
     *  allows for splitting events across a number of gateway connections. 
     *  Guild sharding is entirely user controlled, and requires no 
     *  state-sharing between separate connections to operate.<br>
     *      Quoting https://discordapp.com/developers/docs/topics/gateway#sharding
     *
     *  In Hexicord sharding can be implemented by creating multiple instances
     *  of Client class. You need to run Client::getGatewayUrlBot() once to get
     *  gateway URL and use it for all other instances in Client::gatewayConnectBot
     *  with clientId and clientCount arguments != NoSharding. Shards count from zero,
     *  zero shard (also called master shard) will handle DM.
     *
     *  Example of multi-process sharding using fork() POSIX call.
     *  ```cpp
     *      boost::asio::io_service ios;
     *      Client client(ios, "TTOOOOOOOKKKKEEENNNNNN");
     *
     *      auto urlAndShards = client.getGatewayUrlBot();
     *      std::string gatewayUrl = urlAndShards.first;
     *      int shardsCount = urlAndShards.second;
     *
     *      int pid = -1;
     *      for (int shardId = 1; shardId < shardsCount; ++shardId) {
     *          if ((pid = fork()) == 0) {
     *              // we're in child process
     *              client.connectToGateway(gatewayUrl, shardId, shardsCount);
     *          }
     *      }
     *
     *      // connect master shard
     *      if (pid != 0) {
     *          client.gatewayToGateway(gatewayUrl, 0, shardsCount);
     *      }
     *
     *      ios.run();
     *  ```
     */
    class Client {
    public:
        /**
         *  Don't pass shards array in OP 2 Identify in gatewayConnect.
         */
        static constexpr int NoSharding = -1;

        /**
         *  Construct Client, does nothing network-related to make Client's cheap 
         *  to construct.
         *
         *  \param ioService ASIO I/O service. Should not be destroyed while
         *                   Client exists.
         *  \param token     token string, will be interpreted as OAuth or Bot token
         *                   depending on future calls, don't add "Bearer " or 
         *                   "Bot " prefix.
         */
        Client(boost::asio::io_service& ioService, const std::string& token);

        /**
         *  Destroy Client. Blocks if there running callbacks for gateway events.
         */
        ~Client();

        /// \internal
        Client(const Client&) = delete;
        /// \internal
        Client(Client&&) = default;

        /// \internal
        Client& operator=(const Client&) = delete;
        /// \internal
        Client& operator=(Client&&) = default;

        /**
         *  Returns gateway URL to be used with \ref gatewayConnect.
         *
         *  Client expected to cache this URL and request new only
         *  if fails to use old one.
         *
         *  \sa \ref getGatewayUrlBot
         */
        std::string getGatewayUrl();

        /**
         *  Return gateway URL to be used with \ref gatewayConnectBot
         *  if client is a bot. Also returns recommended shards count.
         *
         *  \returns std::pair with gateway URL (first) and recommended
         *           shards count (second).
         *
         *  \sa \ref getGatewayUrl
         */
        std::pair<std::string, int> getGatewayUrlBot();

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

        // REST methods go here.
        

        /**
         *  Send Close event and disconnect.
         *
         *  \param code     code to pass in event payload.
         */
        void disconnectFromGateway(int code = 2000);

        /**
         *  Send raw REST-request and return result json.
         *
         *  \note Newly constructed Client object don't have open REST
         *        connection. It will be openned when restRequest called
         *        first time.
         *
         *  \note This method can be used in case of API update that
         *        adds new events. It's better to use hardcoded methods
         *        instead of raw ones if avaliable.
         *
         *  \note Library defaults to v6 REST API version. You can change
         *        endpoint base path (\ref restBasePath) if you need other version.
         *
         *  \param method    used HTTP method, can be any string without spaces
         *                   but following used by Discord API: "POST", "GET",
         *                   "PATCH", "PUT", "DELETE".
         *  \param endpoint  endpoint URL relative to base URL (including leading slash).
         *  \param payload   JSON payload, pass nullptr (default) if none.
         *
         *  \throws APIError on API error.
         *  \throws beast::system_error on connection problem.
         *
         *  \sa \ref sendGatewayMsg
         */
        nlohmann::json sendRestRequest(const std::string& method, const std::string& endpoint,
                                       const nlohmann::json& payload = nullptr);

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
         *  Can be changed if you need different API version but such changes
         *  should be avoided unless really necessary since it can affect
         *  behavior of hardcoded methods.
         *
         *  \sa \ref sendRestRequest \ref gatewayPathSuffix
         */
        std::string restBasePath = "/api/v6";

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
         *  Used auth. token.
         */
        const std::string token;

        /**
         *  Get current gateway session ID.
         *  Empty if not connected to gateway yet.
         *
         *  \sa \ref resumeGateway \ref lastSeqNumber \ref lastGatewayUrl
         */
        inline const std::string& sessionId() const {
            return sessionId_;
        }

        /**
         *  Get last received event sequence number.
         *  -1 if not connected to gateway yet.
         *
         *  \sa \ref resumeGateway sessionId lastGatewayUrl
         */
        inline int lastSeqNumber() const {
            return lastSeqNumber_;
        }

        /**
         *  Get last used gateway URL. 
         *  Empty if not connected to gateway yet.
         *
         *  \sa \ref resumeGateway \ref lastSeqNumber \ref sessionId
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
        void startRestKeepaliveTimer();

        static std::string domainFromUrl(const std::string& url);

        std::string sessionId_;
        std::string lastUsedGatewayUrl;

        unsigned heartbeatIntervalMs;
        bool heartbeat = true, restKeepalive = true, gatewayPoll = true;
        int unansweredHeartbeats = 0;

        std::unique_ptr<REST::GenericHTTPConnection<BeastHTTPS> > restConnection;
        std::unique_ptr<TLSWebSocket> gatewayConnection;
        boost::asio::io_service& ioService; // non-owning reference to I/O service.

        boost::asio::deadline_timer keepaliveTimer, heartbeatTimer;
    };
}

#endif // HEXICORD_CLIENT_HPP
