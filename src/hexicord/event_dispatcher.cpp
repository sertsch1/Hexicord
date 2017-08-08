#include "hexicord/event_dispatcher.hpp"
#include <iostream>

namespace Hexicord {
    const std::unordered_map<std::string, Event> EventDispatcher::stringToEnum {
        { "READY",                          Event::Ready                },
        { "RESUMED",                        Event::Resumed              },
        { "CHANNEL_CREATE",                 Event::ChannelCreate        },
        { "CHANNEL_UPDATE",                 Event::ChannelUpdate        },
        { "CHANNEL_DELETE",                 Event::ChannelDelete        },
        { "CHANNEL_PINS_CHANGE",            Event::ChannelPinsChange    },
        { "GUILD_CREATE",                   Event::GuildCreate          },
        { "GUILD_UPDATE",                   Event::GuildUpdate          },
        { "GUILD_DELETE",                   Event::GuildDelete          },
        { "GUILD_BAN_ADD",                  Event::GuildBanAdd          },
        { "GUILD_BAN_REMOVE",               Event::GuildBanRemove       },
        { "GUILD_EMOJIS_UPDATE",            Event::GuildEmojisUpdate    },
        { "GUILD_INTEGRATIONS_UPDATE",      Event::GuildIntegrationsUpdate },
        { "GUILD_MEMBER_ADD",               Event::GuildMemberAdd       },
        { "GUILD_MEMBER_REMOVE",            Event::GuildMemberRemove    },
        { "GUILD_MEMBER_UPDATE",            Event::GuildMemberUpdate    },
        { "GUILD_MEMBERS_CHUNK",            Event::GuildMembersChunk    },
        { "GUILD_ROLE_CREATE",              Event::GuildRoleCreate      },
        { "GUILD_ROLE_UPDATE",              Event::GuildRoleUpdate      },
        { "GUILD_ROLE_DELETE",              Event::GuildRoleDelete      },
        { "MESSAGE_CREATE",                 Event::MessageCreate        },
        { "MESSAGE_UPDATE",                 Event::MessageUpdate        },
        { "MESSAGE_DELETE",                 Event::MessageDelete        },
        { "MESSAGE_DELETE_BULK",            Event::MessageDeleteBulk    },
        { "MESSAGE_REACTION_ADD",           Event::MessageReactionAdd   },
        { "MESSAGE_REACTION_REMOVE_ALL",    Event::MessageReactionRemoveAll },
        { "PRESENSE_UPDATE",                Event::PresenseUpdate       },
        { "TYPING_START",                   Event::TypingStart          },
        { "USER_UPDATE",                    Event::UserUpdate           },
        { "VOICE_STATE_UPDATE",             Event::VoiceStateUpdate     },
        { "VOICE_SERVER_UPDATE",            Event::VoiceServerUpdate    },
        { "WEBHOOKS_UPDATE",                Event::WebhooksUpdate       }
    };

    void EventDispatcher::addHandler(Event eventType, EventDispatcher::EventHandler handler) {
        handlers[eventType].push_back(handler);
    }

    void EventDispatcher::addUnknownEventHandler(EventDispatcher::UnknownEventHandler handler) {
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
