#ifndef HEXICORD_EVENTDISPATCHER_HPP
#define HEXICORD_EVENTDISPATCHER_HPP 

#include <unordered_map>
#include <hexicord/json.hpp>

namespace Hexicord {
    enum class Event {
        Ready,
        Resumed,
        ChannelCreate,
        ChannelUpdate,
        ChannelDelete,
        ChannelPinsChange,
        GuildCreate,
        GuildUpdate,
        GuildDelete,
        GuildBanAdd,
        GuildBanRemove,
        GuildEmojisUpdate,
        GuildIntegrationsUpdate,
        GuildMemberAdd,
        GuildMemberRemove,
        GuildMemberUpdate,
        GuildMembersChunk,
        GuildRoleCreate,
        GuildRoleUpdate,
        GuildRoleDelete,
        MessageCreate,
        MessageUpdate,
        MessageDelete,
        MessageDeleteBulk,
        MessageReactionAdd,
        MessageReactionRemoveAll,
        PresenceUpdate,
        TypingStart,
        UserUpdate,
        VoiceStateUpdate,
        VoiceServerUpdate,
        WebhooksUpdate,
    };

    struct EventHash {
        inline std::size_t operator()(Event e) const noexcept {
            return static_cast<std::size_t>(e);
        }
    };

    class EventDispatcher {
    public:
        using EventHandler        = std::function<void(const nlohmann::json&)>;
        using UnknownEventHandler = std::function<void(const std::string&, const nlohmann::json&)>;

        void addHandler(Event eventType, EventHandler handler);
        void addUnknownEventHandler(UnknownEventHandler handler);

        void dispatchEvent(const std::string& type, const nlohmann::json& payload) const;
    private:
        static const std::unordered_map<std::string, Event> stringToEnum;

        std::unordered_map<Event, std::vector<EventHandler>, EventHash> handlers {
            { Event::Ready, {} },
            { Event::Resumed, {} },
            { Event::ChannelCreate, {} },
            { Event::ChannelUpdate, {} },
            { Event::ChannelDelete, {} },
            { Event::ChannelPinsChange, {} },
            { Event::GuildCreate, {} },
            { Event::GuildUpdate, {} },
            { Event::GuildDelete, {} },
            { Event::GuildBanAdd, {} },
            { Event::GuildBanRemove, {} },
            { Event::GuildEmojisUpdate, {} },
            { Event::GuildIntegrationsUpdate, {} },
            { Event::GuildMemberAdd, {} },
            { Event::GuildMemberRemove, {} },
            { Event::GuildMemberUpdate, {} },
            { Event::GuildMembersChunk, {} },
            { Event::GuildRoleCreate, {} },
            { Event::GuildRoleUpdate, {} },
            { Event::GuildRoleDelete, {} },
            { Event::MessageCreate, {} },
            { Event::MessageUpdate, {} },
            { Event::MessageDelete, {} },
            { Event::MessageDeleteBulk, {} },
            { Event::MessageReactionAdd, {} },
            { Event::MessageReactionRemoveAll, {} },
            { Event::PresenceUpdate, {} },
            { Event::TypingStart, {} },
            { Event::UserUpdate, {} },
            { Event::VoiceStateUpdate, {} },
            { Event::VoiceServerUpdate, {} },
            { Event::WebhooksUpdate, {} }
        };

        std::vector<UnknownEventHandler> unknownEventHandlers;

    };
} // namespace Hexicord

#endif // HEXICORD_EVENTDISPATCHER_HPP
