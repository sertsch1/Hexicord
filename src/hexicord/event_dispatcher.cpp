#include "hexicord/event_dispatcher.hpp"

namespace Hexicord {
    const std::unordered_map<std::string, Event> EventDispatcher::stringToEnum {
        { "READY",                          Event::READY                },
        { "RESUMED",                        Event::RESUMED              },
        { "CHANNEL_CREATE",                 Event::CHANNEL_CREATE       },
        { "CHANNEL_UPDATE",                 Event::CHANNEL_UPDATE       },
        { "CHANNEL_DELETE",                 Event::CHANNEL_DELETE       },
        { "CHANNEL_PINS_CHANGE",            Event::CHANNEL_PINS_CHANGE  },
        { "GUILD_CREATE",                   Event::GUILD_CREATE         },
        { "GUILD_UPDATE",                   Event::GUILD_UPDATE         },
        { "GUILD_DELETE",                   Event::GUILD_DELETE         },
        { "GUILD_BAN_ADD",                  Event::GUILD_BAN_ADD        },
        { "GUILD_BAN_REMOVE",               Event::GUILD_BAN_REMOVE     },
        { "GUILD_EMOJIS_UPDATE",            Event::GUILD_EMOJIS_UPDATE  },
        { "GUILD_INTEGRATIONS_UPDATE",      Event::GUILD_INTEGRATIONS_UPDATE },
        { "GUILD_MEMBER_ADD",               Event::GUILD_MEMBER_ADD     },
        { "GUILD_MEMBER_REMOVE",            Event::GUILD_MEMBER_REMOVE  },
        { "GUILD_MEMBER_UPDATE",            Event::GUILD_MEMBER_UPDATE  },
        { "GUILD_MEMBERS_CHUNK",            Event::GUILD_MEMBERS_CHUNK  },
        { "GUILD_ROLE_CREATE",              Event::GUILD_ROLE_CREATE    },
        { "GUILD_ROLE_UPDATE",              Event::GUILD_ROLE_UPDATE    },
        { "GUILD_ROLE_DELETE",              Event::GUILD_ROLE_DELETE    },
        { "MESSAGE_CREATE",                 Event::MESSAGE_CREATE       },
        { "MESSAGE_UPDATE",                 Event::MESSAGE_UPDATE       },
        { "MESSAGE_DELETE",                 Event::MESSAGE_DELETE       },
        { "MESSAGE_DELETE_BULK",            Event::MESSAGE_DELETE_BULK  },
        { "MESSAGE_REACTION_ADD",           Event::MESSAGE_REACTION_ADD },
        { "MESSAGE_REACTION_REMOVE_ALL",    Event::MESSAGE_REACTION_REMOVE_ALL },
        { "MESSAGE_PRESENSE_UPDATE",        Event::MESSAGE_PRESENSE_UPDATE },
        { "TYPING_START",                   Event::TYPING_START         },
        { "USER_UPDATE",                    Event::USER_UPDATE          },
        { "VOICE_STATE_UPDATE",             Event::VOICE_STATE_UPDATE   },
        { "VOICE_SERVER_UPDATE",            Event::VOICE_SERVER_UPDATE  },
        { "WEBHOOKS_UPDATE",                Event::WEBHOOKS_UPDATE      }
    };

    void EventDispatcher::addHandler(Event eventType, EventDispatcher::EventHandler handler) {
        handlers[eventType].push_back(handler);
    }

    void EventDispatcher::addUnkownEventHandler(EventDispatcher::UnknownEventHandler handler) {
        unknownEventHandlers.push_back(handler);
    }

    void EventDispatcher::dispatchEvent(const std::string& type, const nlohmann::json& payload) const {
        auto enumIt = stringToEnum.find(type);
        if (enumIt == stringToEnum.cend()) { // we got unknown event.
            for (auto handler : unknownEventHandlers) {
                handler(type, payload);
            }
        } else {
            for (auto handler : handlers.at(enumIt->second)) {
                handler(payload);
            }
        }
    }
}
