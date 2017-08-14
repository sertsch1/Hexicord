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
            // Build message in format "API error: {message} (apiCode={apiCode}, httpCode={httpCode})"
            : std::logic_error(message + " (apiCode=" + std::to_string(apiCode) + ", httpCode=" + std::to_string(httpCode) + ")")
            , apiCode(apiCode)
            , httpCode(httpCode) {}

        /**
         *  Contains error message as sent by API.
         */
        const std::string message;

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
     *      client.run();
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
     *          client.connectToGateway(gatewayUrl, 0, shardsCount);
     *      }
     *
     *     client.run();
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
         *  \param payload   JSON payload, pass empty object (default) if none.
         *
         *  \throws APIError on API error.
         *  \throws beast::system_error on connection problem.
         *
         *  \sa \ref sendGatewayMsg
         */
        nlohmann::json sendRestRequest(const std::string& method, const std::string& endpoint,
                                       const nlohmann::json& payload = {});

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
         *  Run bot in 2 threads (threading is important, see below).
         *
         *  If you need more than 2 threads, you may execute ioService.run()
         *  manually, it's just wrapper to hide requirement of two threads to
         *  operate correctly.
         *
         *  \b Implementation details:
         *
         *  Actually this method exactly following:
         *  ```cpp
         *      std::thread second_thread([this]() { this->ioService.run(); });
         *      ioService.run();
         *      second_thread.join();
         *  ```
         *  but adding code to "forward" exceptions from second_thread.
         *
         *  This second thread is important to send heartbeats at right time when
         *  main thread is blocked at read. Both threads can't be blocked by async_read
         *  because of "strict ordering" - only one asyncRead can be queued/running
         *  at same time.
         */
        void run();

        /// \defgroup REST helpers.
        /// Functions for easier REST requests to common endpoints.
        /// @{
        
        /**
         *  Get a channel by ID. Returns a guild channel or dm channel object.
         *
         *  \throws APIError on API error (invalid channel).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getChannel(uint64_t channelId);

        /**
         *  Update a channels settings.
         *
         *  Requires the MANAGE_CHANNELS permission for the guild.
         *  Fires ChannelUpdate event.
         *
         *  \param name      2-100 character channel name.
         *  \param position  The position of the channel in the left-hand listing.
         *  \param topic     0-1024 character channel topic (Text channel only).
         *  \param bitrate   The bitrate (in bits) for voice channel; 8000 to 96000 
         *                   (to 128000 for VIP servers).
         *  \param userLimit The user limit for voice channels; 0-99, 0 means no limit.
         *
         *  \throws APIError on API error (invalid channel ID, missing permissions).
         *  \throws std::invalid_argument if no arguments other than channelId passed,
         *          also thrown when both bitrate and topic passed.
         *  \throws std::out_of_range if arguments out of range.
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Guild channel object.
         */
        nlohmann::json modifyChannel(uint64_t channelId,
                                     boost::optional<std::string> name = boost::none,
                                     boost::optional<int> position = boost::none,
                                     boost::optional<std::string> topic = boost::none,
                                     boost::optional<unsigned> bitrate = boost::none,
                                     boost::optional<unsigned short> usersLimit = boost::none);

        /**
         *  Delete a guild channel or close DM.
         *
         *  Requires the MANAGE_CHANNELS permission for guild.
         *  Fires ChannelDelete event.
         *
         *  \throws APIError on API error (invalid channel ID, missing permissions).
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Channel object.
         */
        nlohmann::json deleteChannel(uint64_t channelId);

        enum GetMsgMode {
            /// Get messages **around** start ID.
            Around,
            /// Get messages **before** start ID.
            Before,
            /// Get messages **after** start ID.
            After
        };

        /**
         *  Get messages from channel.
         *
         *  Requires READ_MESSAGES permission if operating on guild channel.
         *
         *  \param startMessagesId  Return messages around/before/after (depending on mode) this ID
         *  \param mode             See \ref GetMsgMode. Default is After.
         *  \param limit            Max number of messages to return (1-100). Default is 50.
         *
         *  \throws APIError on API error (invalid ID, missing permissions).
         *  \throws std::out_of_range if limit is bigger than 100 or equals to 0.
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Array of message objects.
         */
        nlohmann::json getChannelMessages(uint64_t channelId, uint64_t startMessageId,
                                          GetMsgMode mode = After, unsigned short limit = 50);

        /**
         *  Returns a specific message in the channel.
         *
         *  Requires READ_MESSAGES_HISTORY permission if operating on guild channel.
         *
         *  \throws APIError on API error (invalid ID, missing permission).
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Message object.
         */
        nlohmann::json getChannelMessage(uint64_t channelId, uint64_t messageId);

        /**
         *  Post a message to a guild text or DM channel. 
         *
         *  Requires SEND_MESSAGE permission if operating on guild channel.
         *  Fires MessageCreate event.
         *
         *  \param text   The message text content (up to 2000 characters).
         *  \param tts    Set whatever this is TTS message. Default is false.
         *  \param nonce  Nonce that can be used for optimistic message sending. Default is none.
         *
         *  \throws APIError on API error (missing permission, invalid ID).
         *  \throws std::out_of_range if text is bigger than 2000 characters.
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Message object that represents sent message.
         */
        nlohmann::json sendMessage(uint64_t channelId, const std::string& text, bool tts = false,
                                  boost::optional<uint64_t> nonce = boost::none);

        /**
         *  Edit a previously sent message. You can only edit messages that have
         *  been sent by the current user.
         *
         *  Fires MessageUpdate event.
         *
         *  \param text     New text.
         *
         *  \throws APIError on API error (editing other user's messages, invalid ID).
         *  \throws std::out_of_range if text is bigger than 2000 characters.
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Message object.
         */
        nlohmann::json editMessage(uint64_t channelId, uint64_t messageId, const std::string& text);

        /**
         *  Delete a message. If operating on a guild channel and trying to
         *  delete a message that was not sent by the current user,
         *  requires the 'MANAGE_MESSAGES' permission.
         *
         *  Fires MessageDelete event.
         *
         *  \throws APIError on API error (missing permission, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        void deleteMessage(uint64_t channelId, uint64_t messageId);

        /**
         *  Delete multiple messages in a single request. This endpoint can 
         *  only be used on guild channels and requires
         *  the 'MANAGE_MESSAGES' permission.
         *
         *  Fires mulitply MessageDelete event.
         *
         *  \warning This method will not delete messages older than 2 weeks,
         *  and will fail if any message provided is older than that. 
         *
         *  \throws APIError on API error (missing permission, invalid ID, older than 2 weeks).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        void deleteMessages(uint64_t channelId, const std::vector<uint64_t>& messageIds);

        /// @}

        /**
         *  Can be changed if you need different API version but such changes
         *  should be avoided unless really necessary since it can affect
         *  behavior of REST helpers.
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

        // Set whatever there is sendRestRequest executing restConnection->request. 
        // Used to prevent attempts to send keepalive requests during this.
        bool activeRestRequest = false;

        std::unique_ptr<REST::GenericHTTPConnection<BeastHTTPS> > restConnection;
        std::unique_ptr<TLSWebSocket> gatewayConnection;
        boost::asio::io_service& ioService; // non-owning reference to I/O service.

        boost::asio::deadline_timer keepaliveTimer, heartbeatTimer;
    };
}

#endif // HEXICORD_CLIENT_HPP
