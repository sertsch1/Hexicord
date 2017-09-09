// Hexicord - Discord API library for C++11 using boost libraries.
// Copyright © 2017 Maks Mazurov (fox.cpp) <foxcpp@yandex.ru>
// 
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
// OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef HEXICORD_REST_CLIENT_HPP
#define HEXICORD_REST_CLIENT_HPP

#include <utility>
#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include <hexicord/permission.hpp>
#include <hexicord/json.hpp>
#include <hexicord/internal/rest.hpp>
#include <hexicord/config.hpp>
#ifdef HEXICORD_RATELIMIT_PREDICTION 
    #include <hexicord/ratelimit_lock.hpp>
#endif

/**
 *  \file client.hpp
 *   
 *   Defines \ref Hexicord::RestClient class, main class of library.
 */

namespace Hexicord {
    class RestClient {
    public:
        /**
         *  Construct RestClient, does nothing network-related to make RestClient's cheap 
         *  to construct.
         *
         *  \param ioService ASIO I/O service. Should not be destroyed while
         *                   RestClient exists.
         *  \param token     token string, will be interpreted as OAuth or Bot token
         *                   depending on future calls, don't add "Bearer " or 
         *                   "Bot " prefix.
         */
        RestClient(boost::asio::io_service& ioService, const std::string& token);

        RestClient(const RestClient&) = delete;
        RestClient(RestClient&&) = default;

        RestClient& operator=(const RestClient&) = delete;
        RestClient& operator=(RestClient&&) = default;

        /**
         *  Returns gateway URL to be used with \ref GatewayClient::connect.
         *
         *  RestClient expected to cache this URL and request new only
         *  if fails to use old one.
         *
         *  \sa \ref getGatewayUrlBot
         */
        std::string getGatewayUrl();

        /**
         *  Return gateway URL to be used with \ref GatewayClient::connect.
         *  if client is a bot. Also returns recommended shards count.
         *
         *  \returns std::pair with gateway URL (first) and recommended
         *           shards count (second).
         *
         *  \sa \ref getGatewayUrl
         */
        std::pair<std::string, int> getGatewayUrlBot();

        /**
         *  Send raw REST-request and return result json.
         *
         *  \note Newly constructed RestClient object don't have open REST
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
         *  \param query     GET request query.
         *  \param multipart Pass one or more MultipartEntity to perform multipart request.
         *                   If payload is also present, it will be first entity.
         *
         *  \throws RESTError on API error.
         *  \throws boost::system::system_error on connection problem.
         *
         *  \ingroup REST
         */
        nlohmann::json sendRestRequest(const std::string& method, const std::string& endpoint,
                                       const nlohmann::json& payload = {},
                                       const std::unordered_map<std::string, std::string>& query = {},
                                       const std::vector<REST::MultipartEntity>& multipart = {});

        /** \defgroup REST REST methods
         *
         *
         *  Functions for performing requests to REST endpoints.
         *
         *  All methods in this group is thread-safe and stateless if not stated otherwise.
         *
         *  @{
         */
        
        /**
         *  \defgroup REST_channels Channel operations
         *
         *  Methods related to channels. 
         *
         *  Most require MANAGE_CHANNELS permission if operating on guild channel.
         *
         *  @{
         */
        
        /**
         *  Get a channel by ID. Returns a guild channel or dm channel object.
         *
         *  \throws RESTError on API error (invalid channel).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getChannel(uint64_t channelId);

        /**
         *  Update a channels settings.
         *
         *  Requires the MANAGE_CHANNELS permission for the guild.
         *  Fires ChannelUpdate event.
         *
         *  \param channelId  Snowflake ID of target channel.
         *  \param name       2-100 character channel name.
         *  \param position   The position of the channel in the left-hand listing.
         *  \param topic      0-1024 character channel topic (Text channel only).
         *  \param bitrate    The bitrate (in bits) for voice channel; 8000 to 96000 
         *                    (to 128000 for VIP servers).
         *  \param usersLimit The user limit for voice channels; 0-99, 0 means no limit.
         *
         *  \throws RESTError on API error (invalid channel ID, missing permissions).
         *  \throws InvalidParameter if no arguments other than channelId passed,
         *          also thrown when both bitrate and topic passed.
         *  \throws InvalidParameter if arguments out of range.
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
         *  \throws RESTError on API error (invalid channel ID, missing permissions).
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
         *  \param channelId        Snowflake ID of target channel.
         *  \param startMessageId   Return messages around/before/after (depending on mode) this ID
         *  \param mode             See \ref GetMsgMode. Default is After.
         *  \param limit            Max number of messages to return (1-100). Default is 50.
         *
         *  \throws RESTError on API error (invalid ID, missing permissions).
         *  \throws InvalidParameter if limit is bigger than 100 or equals to 0.
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
         *  \param channelId    Snowflake ID of target channel.
         *  \param messageId    Snowflake ID of target message.
         *
         *  \throws RESTError on API error (invalid ID, missing permission).
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Message object.
         */
        nlohmann::json getChannelMessage(uint64_t channelId, uint64_t messageId);

        /// \defgroup REST_pins Pinned messages operations
        /// Methods related to pinned messages.
        /// @{
        
        /**
         *  Returns JSON array of all pinned messages in channel
         *  specified by channelId.
         *
         *  May throw RESTError if ID is invalid and boost::system::system_error
         *  if client can't connect to REST API server.
         */
        nlohmann::json getPinnedMessages(uint64_t channelId);

        /**
         *  Pin message messageId in channel channelId.
         *
         *  Requires MANAGE_MESSAGES permission.
         *
         *  May throw RESTError if ID is invalid or you don't have MANAGE_MESSAGES
         *  permission. Also can throw boost::system::system_error if client can't
         *  connect to REST API server.
         */
        void pinMessage(uint64_t channelId, uint64_t messageId);

        /**
         *  Unpin message messageId in channel channelId.
         *
         *  Requires MANAGE_MESSAGES permission.
         *
         *  May throw RESTError if ID is invalid or you don't have MANAGE_MESSAGES
         *  permission. Also can throw boost::system::system_error if client can't
         *  connect to REST API server.
         */
        void unpinMessage(uint64_t channelId, uint64_t messageId);

        /// @} REST_pins
        
        /**
         *  Change existing or add new channel permission override for a кщду.
         *  
         *  Only usable for guild channels. Requires the 'MANAGE_ROLES' permission.
         *
         *  If you don't specify permission in either allow or deny parmeter it will
         *  inherit global value (no override). Adding permission to both allow and
         *  deny is not allowed and will throw RESTError.
         *
         *  \note
         *  Note that passing default-constructed value to both allow and deny will
         *  not remove override (it will still exist, but have no effect). To
         *  completely remove override you should use \ref deleteChannelPermissions.
         *
         *  May throw RESTError if ID is invalid or you don't have MANAGE_MESSAGES
         *  permission. Also can throw boost::system::system_error if client can't
         *  connect to REST API server.
         */
        void editChannelRolePermissions(uint64_t channelId, uint64_t roleId,
                                        Permissions allow, Permissions deny);

        /**
         *  Change existing or add new channel permission override for a user.
         *  
         *  Only usable for guild channels. Requires the 'MANAGE_ROLES' permission.
         *
         *  If you don't specify permission in either allow or deny parmeter it will
         *  inherit global value (no override). Adding permission to both allow and
         *  deny is not allowed and will throw RESTError.
         *
         *  \note
         *  Note that passing default-constructed value to both allow and deny will
         *  not remove override (it will still exist, but have no effect). To
         *  completely remove override you should use \ref deleteChannelPermissions.
         *
         *  May throw RESTError if ID is invalid or you don't have MANAGE_MESSAGES
         *  permission. Also can throw boost::system::system_error if client can't
         *  connect to REST API server.
         */
        void editChannelUserPermissions(uint64_t channelId, uint64_t userId,
                                        Permissions allow, Permissions deny);

        /**
         *  Delete a channel permission overwrite for a user or role in a channel. 
         *  overrideId may be role ID or user ID, depending on override type.
         *
         *  Only usable for guild channels. Requires the 'MANAGE_ROLES' permission. 
         *
         *  May throw RESTError if ID is invalid or you don't have MANAGE_MESSAGES
         *  permission. Also can throw boost::system::system_error if client can't
         *  connect to REST API server.
         */
        void deleteChannelPermissions(uint64_t channelId, uint64_t overrideId);

        /**
         *  Remove (kick) member from group DM.
         *
         *  May throw RESTError if ID is invalid. Also can throw 
         *  boost::system::system_error if client can't connect
         *  to REST API server.
         */
        void kickFromGroupDm(uint64_t groupDmId, uint64_t userId);

        /**
         *  Add member to group DM.
         *
         *  You should pass access token of a user that has granted
         *  your app the gdm.join scope and nickname in order
         *  to add him.
         *
         *  May throw RESTError if parameters is invalid. Also can throw 
         *  boost::system::system_error if client can't connect
         *  to REST API server.
         */
        void addToGroupDm(uint64_t groupDmId, uint64_t userId,
                          const std::string& accessToken, const std::string& nick);

        void triggerTypingIndicator(uint64_t channelId);

        /// @} Channel methods
        
        /**
         *  \defgroup REST_messages Messages operations
         *
         *  Methods related to messages.
         *
         *  @{
         */

        /**
         *  Post a message to a guild text or DM channel. 
         *
         *  Requires SEND_MESSAGE permission if operating on guild channel.
         *  Fires MessageCreate event.
         *
         *  \param channelId    Snowflake ID of target channel.
         *  \param text         The message text content (up to 2000 characters).
         *  \param tts          Set whatever this is TTS message. Default is false.
         *  \param nonce        Nonce that can be used for optimistic message sending. Default is none.
         *
         *  \throws RESTError on API error (missing permission, invalid ID).
         *  \throws InvalidParameter if text is bigger than 2000 characters.
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
         *  \param channelId    Snowflake ID of target channel.
         *  \param messageId    Snowflake ID of target message.
         *  \param text         New text.
         *
         *  \throws RESTError on API error (editing other user's messages, invalid ID).
         *  \throws InvalidParameter if text is bigger than 2000 characters.
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
         *  \param channelId    Snowflake ID of target channel.
         *  \param messageId    Snowflake ID of target message.
         *
         *  \throws RESTError on API error (missing permission, invalid ID).
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
         *  \param channelId    Snowflake ID of target channel.
         *  \param messageIds   Vector of message IDs.
         *
         *  \throws RESTError on API error (missing permission, invalid ID, older than 2 weeks).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        void deleteMessages(uint64_t channelId, const std::vector<uint64_t>& messageIds);

        /// \defgroup REST_reactions Reactions
        /// Methods related to message reactions.
        /// @{
        
        /**
         *  Add reaction to message.
         *
         *  This method requires READ_MESSAGE_HISTORY permission and,
         *  additionally, ADD_REACTIONS permissions if nobody else reacted
         *  using this emoji.
         *
         *  May throw RESTError if invalid IDs specified or current user don't
         *  have required permissions. Also may throw 
         *  boost::system::system_error if client fails to connect 
         *  to REST API server.
         *
         *  \sa \ref removeReaction \ref getReactions
         */
        void addReaction(uint64_t channelId, uint64_t messageId, uint64_t emojiId);

        /**
         *  Remove reaction from message.
         *
         *  By default this method removes current user's reaction, pass userId
         *  to remove other user's reactions, however this requires
         *  MANAGE_MESSAGES permission.
         *
         *  May throw RESTError if invalid IDs specified or current user don't
         *  have required permissions. Also may throw 
         *  boost::system::system_error if client fails to connect 
         *  to REST API server.
         *
         *  \sa \ref addReaction \ref getReactions
         */
        void removeReaction(uint64_t channelId, uint64_t messageId, uint64_t emojiId, uint64_t userId = 0);

        /**
         *  Get a list of users that reacted with this emoji.
         *
         *  May throw RESTError if invalid IDs specified or current user don't
         *  have required permissions. Also may throw 
         *  boost::system::system_error if client fails to connect 
         *  to REST API server.
         *
         *  \sa \ref addReaction \ref removeReaction
         */
        nlohmann::json getReactions(uint64_t channelId, uint64_t messageId, uint64_t emojiId);

        /**
         *  Remove all reactions from message. Requires MANAGE_MESSAGES
         *  permission.
         *
         *  May throw RESTError if invalid IDs specified or current user don't
         *  have required permissions. Also may throw 
         *  boost::system::system_error if client fails to connect 
         *  to REST API server.
         */
        void resetReactions(uint64_t channelId, uint64_t messageId);

        /// @} REST_reactions

        /// @} REST_messages
        
        /**
         *  \defgroup REST_users Users methods
         *
         *  Methods related to users.
         *
         *  @{
         */

        /**
         *  Returns the user object of the requester's account. 
         *
         *  For OAuth2, this requires the identify scope, which will return
         *  the object without an email, and optionally the email scope,
         *  which returns the object with an email.
         *
         *  \throws RESTError on API error (missing permissions).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getMe();

        /**
         *  Return a user object for gived user ID.
         *
         *  \param id   User snowflake ID.
         *
         *  \throws RESTError on API error (???).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getUser(uint64_t id);

        /**
         *  Change username.
         *
         *  May cause discriminator to be randomized.
         *
         *  \param newUsername  New username.
         *  Discord enforces the following restrictions for usernames and nicknames:
         *  * Names can contain most valid unicode characters. We limit some
         *    zero-width and non-rendering characters.
         *  * Names must be between 2 and 32 characters long.
         *  * Names cannot contain the following substrings: '@', '#',
         *    ':', '```'.
         *  * Names cannot be: 'discordtag', 'everyone', 'here'.
         *  * Names are sanitized and trimmed of leading, trailing, and
         *    exessive internal whitespace.
         *
         *  There are other rules and restrictions not shared here for the sake
         *  of spam and abuse mitigation, but the majority of users won't
         *  encounter them. It's important to properly handle all error 
         *  messages returned by Discord when editing or updating names.
         *
         *  \throws InvalidParameter if name.size() is 1 or > 32.
         *  \throws InvalidParameter if name is 'discordtag', 'everyone',
         *          'here' or contains '@', '#', ':', or '```'.
         *  \throws RESTError on API error (invalid name).
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns User object after change.
         */
        nlohmann::json setUsername(const std::string& newUsername);

        enum AvatarFormat {
            /// Try to detect avatar format. 
            /// Always works for valid PNG/GIF/JPEG but may also "detect"
            /// image in arbitary data.
            Detect,
            
            Png,
            Gif,
            Jpeg
        };

        /**
         *  Change avatar.
         *
         *  \throws RESTError on API error (???).
         *  \throws InvalidParameter if format is Detect and detection failed.
         *  \throws boost::system::system_error on connection problem (rare).
         *                                                                    
         *  \returns User object after change.
         */
        nlohmann::json setAvatar(const std::vector<uint8_t>& avatarBytes, AvatarFormat format = Detect);

        /**
         *  Change avatar.
         *
         *  Convenience overload. Reads stream into vector and passes to first overload.
         *
         *  \throws RESTError on API error (???).
         *  \throws boost::system::system_error on connection problem (rare).
         *                                                                    
         *  \returns User object after change.
         */
        nlohmann::json setAvatar(std::istream&& avatarStream, AvatarFormat format = Detect);

        /**
         *  Returns list of partial guild object the current user is member of.
         *
         *  Requires guilds OAuth2 scope.
         *
         *  Example of partial guild object:
         *  ```json
         *  {
         *      "id": "80351110224678912",
         *      "name": "1337 Krew",
         *      "icon": "8342729096ea3675442027381ff50dfe",
         *      "owner": true,
         *      "permissions": 36953089
         *  }
         *  ```
         *  Fields have same meaning as regular in full guild object.
         *
         *  \note This methods returns 100 guilds by default, which is the
         *  maximum number of guilds a non-bot user can join. Therefore,
         *  pagination is not needed for integrations that need to get a
         *  list of users' guilds.
         *
         *  \param limit    Max number of guilds to return (1-100).
         *  \param startId  Get guilds **after** this guild ID.
         *  \param before   Get guilds **before** startId, not after.
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getUserGuilds(unsigned short limit = 100, uint64_t startId = 0, bool before = false);

        /**
         *  Leave a guild.
         *
         *  \param guildId  Guild snowflake ID.
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        void leaveGuild(uint64_t guildId);

        /**
         *  Returns a list of DM channel objects.
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getUserDms();

        /**
         *  Create a new DM channel with a user.
         *
         *  \param recipientId  Recipient user showflake ID.
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns Created DM channel object.
         */
        nlohmann::json createDm(uint64_t recipientId);

        /**
         *  Create a new group DM channel with multiple users. Returns 
         *  a DM channel object.
         *
         *  \warning By default this method is limited to 10 active group DMs.
         *  These limits are raised for whitelisted GameBridge applications.
         *
         *  \param accessTokens Access tokens of users that have granted your 
         *                      app the gdm.join scope.
         *  \param nicks        A dictionary of user ids to their respective nicknames.
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json createGroupDm(const std::vector<uint64_t>& accessTokens,
                                     const std::unordered_map<uint64_t, std::string>& nicks);

        /**
         *  Returns a list of connection objects.
         *
         *  Requires the connections OAuth2 scope.
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getConnections();

        /// @} REST_users
        
        /**
         *  \defgroup REST_invites Invites operations
         *
         *  Methods related to invites.
         *
         *  @{
         */

        /**
         *  Returns an invite object for the given code.
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         */
        nlohmann::json getInvite(const std::string& inviteCode);

        /**
         *  Delete (revoke) an invite. 
         *
         *  Requires the MANAGE_CHANNELS permission. 
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns An invite object.
         */
        nlohmann::json revokeInvite(const std::string& inviteCode);

        /**
         *  Accept an invite. 
         *
         *  This requires the guilds.join OAuth2 scope to be able to accept
         *  invites on behalf of normal users (via an OAuth2 Bearer token). Bot
         *  users are disallowed. 
         *
         *  \throws RESTError on API error (missing permissions, invalid ID).
         *  \throws boost::system::system_error on connection problem (rare).
         *
         *  \returns An invite object.
         */
        nlohmann::json acceptInvite(const std::string& inviteCode);

        /**
         *  Get a list of invite objects (with invite metadata) for the channel.
         *
         *  Requires the 'MANAGE_CHANNELS' permission.
         *
         *  May throw RESTError if invalid IDs specified or current user don't
         *  have required permissions. Also may throw 
         *  boost::system::system_error if client fails to connect 
         *  to REST API server.
         */
        nlohmann::json getChannelInvites(uint64_t channelId);

        /**
         *  Create new invite for a channel.
         *
         *  Requires the CREATE_INSTANT_INVITE permission.
         *
         *  By default invite link is usable for 24 hours (86400 seconds), you
         *  can pass maxAgeSecs parameter to override this. Pass 0 to create
         *  invite with unlimited duration (it can be revoked using
         *  \ref revokeInvite). You can also limit times invite can be used by
         *  passing maxUses parameter.
         *
         *  You can also create invite link which give temporary membership.
         *  User will be removed from guild when he will go offline and don't 
         *  got a role.
         *
         *  By default this method tries to reuse similar invite if avaliable,
         *  you can override this by passing unique = true.
         *
         *  This method throws RESTError on any API error (for this method it can be
         *  caused by invalid ID or missing permission).
         *  In addition this method throws boost::system::system_error if REST API
         *  server is not reachable.
         */
        nlohmann::json createInvite(uint64_t channelId, unsigned maxAgeSecs = 86400,
                                    unsigned maxUses = 0,
                                    bool temporaryMembership = false,
                                    bool unique = false);

        /// @} REST_invites
        /// @} REST


#ifdef HEXICORD_RATELIMIT_PREDICTION
        RatelimitLock ratelimitLock;
#endif

        /**
         *  Used authorization token.
         */
        const std::string token;
private:
        static constexpr const char* restBasePath = "/api/v6";

        void prepareRequestBody(REST::HTTPRequest& request,
                                const nlohmann::json& payload,
                                const std::vector<REST::MultipartEntity>& elements);

        // Throws RESTError or inherited class.
        void throwRestError(const REST::HTTPResponse& response, const nlohmann::json& payload);

#ifdef HEXICORD_RATELIMIT_PREDICTION 
        void updateRatelimitsIfPresent(const std::string& endpoint, const REST::HeadersMap& headers);
#endif 

        std::unique_ptr<REST::HTTPSConnection> restConnection;
        boost::asio::io_service& ioService; // non-owning reference to I/O service.
    };
}

#endif // HEXICORD_REST_CLIENT_HPP
