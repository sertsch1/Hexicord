#ifndef HEXICORD_EVENTDISPATCHER_HPP
#define HEXICORD_EVENTDISPATCHER_HPP 

#include <unordered_map>
#include <hexicord/json.hpp>

namespace Hexicord {
    enum class Event {
        READY,
        RESUMED,
        CHANNEL_CREATE,
        CHANNEL_UPDATE,
        CHANNEL_DELETE,
        CHANNEL_PINS_CHANGE,
        GUILD_CREATE,
        GUILD_UPDATE,
        GUILD_DELETE,
        GUILD_BAN_ADD,
        GUILD_BAN_REMOVE,
        GUILD_EMOJIS_UPDATE,
        GUILD_INTEGRATIONS_UPDATE,
        GUILD_MEMBER_ADD,
        GUILD_MEMBER_REMOVE,
        GUILD_MEMBER_UPDATE,
        GUILD_MEMBERS_CHUNK,
        GUILD_ROLE_CREATE,
        GUILD_ROLE_UPDATE,
        GUILD_ROLE_DELETE,
        MESSAGE_CREATE,
        MESSAGE_UPDATE,
        MESSAGE_DELETE,
        MESSAGE_DELETE_BULK,
        MESSAGE_REACTION_ADD,
        MESSAGE_REACTION_REMOVE_ALL,
        MESSAGE_PRESENSE_UPDATE,
        TYPING_START,
        USER_UPDATE,
        VOICE_STATE_UPDATE,
        VOICE_SERVER_UPDATE,
        WEBHOOKS_UPDATE,
    };

    class EventDispatcher {
    public:
        using EventHandler        = std::function<void(const nlohmann::json&)>;
        using UnknownEventHandler = std::function<void(const std::string&, const nlohmann::json&)>;

        void addHandler(Event eventType, EventHandler handler);
        void addUnkownEventHandler(UnknownEventHandler handler);

        void dispatchEvent(const std::string& type, const nlohmann::json& payload) const;
    private:
        static const std::unordered_map<std::string, Event> stringToEnum;

        std::unordered_map<Event, std::vector<EventHandler> > handlers {
            { Event::READY, {} },
            { Event::RESUMED, {} },
            { Event::CHANNEL_CREATE, {} },
            { Event::CHANNEL_UPDATE, {} },
            { Event::CHANNEL_DELETE, {} },
            { Event::CHANNEL_PINS_CHANGE, {} },
            { Event::GUILD_CREATE, {} },
            { Event::GUILD_UPDATE, {} },
            { Event::GUILD_DELETE, {} },
            { Event::GUILD_BAN_ADD, {} },
            { Event::GUILD_BAN_REMOVE, {} },
            { Event::GUILD_EMOJIS_UPDATE, {} },
            { Event::GUILD_INTEGRATIONS_UPDATE, {} },
            { Event::GUILD_MEMBER_ADD, {} },
            { Event::GUILD_MEMBER_REMOVE, {} },
            { Event::GUILD_MEMBER_UPDATE, {} },
            { Event::GUILD_MEMBERS_CHUNK, {} },
            { Event::GUILD_ROLE_CREATE, {} },
            { Event::GUILD_ROLE_UPDATE, {} },
            { Event::GUILD_ROLE_DELETE, {} },
            { Event::MESSAGE_CREATE, {} },
            { Event::MESSAGE_UPDATE, {} },
            { Event::MESSAGE_DELETE, {} },
            { Event::MESSAGE_DELETE_BULK, {} },
            { Event::MESSAGE_REACTION_ADD, {} },
            { Event::MESSAGE_REACTION_REMOVE_ALL, {} },
            { Event::MESSAGE_PRESENSE_UPDATE, {} },
            { Event::TYPING_START, {} },
            { Event::USER_UPDATE, {} },
            { Event::VOICE_STATE_UPDATE, {} },
            { Event::VOICE_SERVER_UPDATE, {} },
            { Event::WEBHOOKS_UPDATE, {} }
        };

        std::vector<UnknownEventHandler> unknownEventHandlers;

    };
} // namespace Hexicord

#endif // HEXICORD_EVENTDISPATCHER_HPP
