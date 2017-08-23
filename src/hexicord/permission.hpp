#ifndef HEXICORD_PERMISSION_HPP
#define HEXICORD_PERMISSION_HPP

#include <hexicord/flags.hpp>

/**
 *  \file permissions.hpp
 *
 *  Defines enumeration Permission and Flags<Permission> alias.
 */

namespace Hexicord {
    /**
     *  \brief Permissions enumeration.
     *
     *  Permissions in Discord are a way to limit and grant certain abilities 
     *  to users. A set of base permissions can be configured at the guild level
     *  for different roles, when these roles are attached to users they grant
     *  or revoke specific privileges within the guild. Along with the global 
     *  guild-level permissions, Discord also supports role overwrites which
     *  can be set at the channel level allowing customization of permissions
     *  on a per-role, per-channel basis.
     *
     *  \internal
     *
     *  Filled according to https://discordapp.com/developers/docs/topics/permissions#permissions
     *
     *  Permissions in Discord are stored within a 53-bit integer and are
     *  calculated using bitwise operations. Permissions for a user in a
     *  given channel can be calculated by ORing together their guild-level 
     *  role permission integers, and their channel-level role 
     *  permission integers.
     */
    enum Permission {
        CreateInstantInvite = 0x00000001, /// Allows creation of instant invites.
        KickMembers         = 0x00000002, /// Allows kicking members.
        BanMembers          = 0x00000004, /// Allows banning members.
        Administrator       = 0x00000008, /// Allows all permissions and bypasses channel permission overwrites.
        ManageChannels      = 0x00000010, /// Allows management and editing of channels.
        ManageGuild         = 0x00000020, /// Allows management and editing of the guild.
        AddReactions        = 0x00000040, /// Allows for the addition of reactions to messages.
        ViewAuditLog        = 0x00000080, /// Allows for viewing of audit logs.
        ReadMessages        = 0x00000400, /// Allows reading messages in a channel. The channel will not appear for users without this permission.
        SendMessages        = 0x00000800, /// Allows for sending messages in a channel.
        SendTtsMessages     = 0x00001000, /// Allows for sending of /tts messages.
        ManageMessages      = 0x00002000, /// Allows for deletion of other users messages.
        EmbedLinks          = 0x00004000, /// Links sent by this user will be auto-embedded.
        AttachFiles         = 0x00008000, /// Allows for uploading images and files.
        ReadMessageHistory  = 0x00010000, /// Allows for reading of message history
        MentionEveryone     = 0x00020000, /// Allows for using the `@everyone` tag to notify all users in a channel, and the `@here` tag to notify all online users in a channel.
        UseExternalEmoji    = 0x00040000, /// Allows the usage of custom emojis from other servers.
        Connect             = 0x00100000, /// Allows for joining of a voice channel.
        Speak               = 0x00200000, /// Allows for speaking in a voice channel.
        MuteMembers         = 0x00400000, /// Allows for muting members in a voice channel.
        DeafenMembers       = 0x00800000, /// Allows for deafening of members in a voice channel.
        MoveMembers         = 0x01000000, /// Allows for moving of members between voice channels.
        UseVad              = 0x02000000, /// Allows for using voice-activity-detection in a voice channel.
        ChangeNickname      = 0x04000000, /// Allows for modification of own nickname.
        ManageNicknames     = 0x08000000, /// Allows for modification of other users nicknames.
        ManageRoles         = 0x10000000, /// Allows management and editing of roles.
        ManageWebhooks      = 0x20000000, /// Allows management and editing of webhooks.
        ManageEmoji         = 0x40000000, /// Allows management and editing of emojis
    };

    using Permissions = Flags<Permission, uint64_t>;
    DECLARE_FLAGS_OPERATORS(Permission);
} // namespace Hexicord

#endif // HEXICORD_PERMISSION_HPP
