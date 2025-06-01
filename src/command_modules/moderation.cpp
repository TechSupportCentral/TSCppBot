/* moderation: Commands related to server moderation
 * Copyright 2025 Ben Westover <me@benthetechguy.net>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version. This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "moderation.h"
#include "../util.h"
#include <map>

dpp::task<> moderation::create_ticket(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking(true);
    // Create private thread for ticket
    std::string title = std::get<std::string>(event.get_parameter("title"));
    dpp::snowflake channel_id = config["log_channel_ids"]["tickets"];
    dpp::confirmation_callback_t confirmation = co_await event.owner->co_thread_create
    (title, channel_id, 10080, dpp::CHANNEL_PRIVATE_THREAD, false, 0);
    if (confirmation.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message("Failed to create ticket channel."));
        co_return;
    }
    dpp::thread thread = std::get<dpp::thread>(confirmation.value);
    // Mention moderators to add them to ticket and notify them at the same time
    event.owner->message_create(dpp::message(thread.id, event.command.get_issuing_user().get_mention()
    + std::format(", your ticket has been created. Please explain your rationale and wait for a <@&{}> to respond.",
    config["role_ids"]["moderator"].get<uint64_t>())).set_allowed_mentions(true, true));
    // Notify user that ticket was successfully created
    co_await thinking;
    event.edit_original_response(dpp::message(std::string("Ticket created successfully at ") + thread.get_mention()));
}

dpp::task<> moderation::purge(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking(true);
    // Get context variables
    int64_t limit = std::get<int64_t>(event.get_parameter("messages"));
    dpp::channel channel = event.command.get_resolved_channel(std::get<dpp::snowflake>(event.get_parameter("channel")));
    // Get recent messages from channel
    dpp::confirmation_callback_t messages_conf = co_await event.owner->co_messages_get(channel.id, 0, 0, 0, limit);
    if (messages_conf.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Failed to find {} messages in {}.", limit, channel.get_mention())));
        co_return;
    }
    dpp::message_map message_map = std::get<dpp::message_map>(messages_conf.value);
    // Transform map of messages into vector of their IDs
    std::vector<dpp::snowflake> message_vec(message_map.size());
    std::ranges::transform(message_map, message_vec.begin(), [](const auto& pair){return pair.first;});
    // Delete all messages in list
    dpp::confirmation_callback_t delete_conf = co_await event.owner->co_message_delete_bulk(message_vec, channel.id);
    if (delete_conf.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Failed to delete some messages in {}", channel.get_mention())));
        co_return;
    }
    // Log this action
    dpp::embed embed = dpp::embed().set_color(0x00A0A0)
                                      .set_title(std::to_string(limit) + " Messages Deleted")
                                      .set_thumbnail(event.command.member.get_avatar_url())
                                      .add_field("Deleted by", event.command.member.get_nickname(), true)
                                      .add_field("In channel", channel.get_mention(), true);
    try {
        std::string reason = std::get<std::string>(event.get_parameter("reason"));
        embed.add_field("Reason", reason, false);
    } catch (const std::bad_variant_access&) {}
    event.owner->message_create(dpp::message(config["log_channel_ids"]["mod_log"], embed));
    // Notify user it was successful
    co_await thinking;
    event.edit_original_response(dpp::message(std::format("Deleted {} messages successfully.", limit)));
}

void moderation::userinfo(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    dpp::user user;
    try {
        dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
        user = event.command.get_resolved_user(user_id);
    } catch (const std::bad_variant_access&) {
        user = event.command.get_issuing_user();
    }
    dpp::embed embed = dpp::embed().set_color(0x00A0A0).set_thumbnail(user.get_avatar_url())
                                      .set_title(user.username)
                                      .add_field("User ID", user.id.str(), false)
                                      .add_field("Display name", user.global_name, true);

    try {
        dpp::guild_member member;
        if (user.id == event.command.member.user_id) {
            member = event.command.member;
        } else {
            member = event.command.get_resolved_member(user.id);
        }
        if (!member.get_nickname().empty()) {
            embed.add_field("Nickname", member.get_nickname(), true);
        }

        std::string roles;
        for (const auto& id : member.get_roles()) {
            if (id != config["role_ids"]["everyone"].get<dpp::snowflake>()) {
                roles += std::format(", <@&{}>", id.str());
            }
        }
        embed.add_field("Roles", roles.substr(2), false);

        time_t user_created = user.get_creation_time();
        time_t time_since_created = time(nullptr) - user_created;
        std::string fancytime_since_created;
        if (time_since_created > 604800) {
            fancytime_since_created = util::seconds_to_fancytime(time_since_created, 1);
        } else {
            fancytime_since_created = util::seconds_to_fancytime(time_since_created, 2);
        }
        embed.add_field("Account created:", std::format("<t:{}> ({} ago)", user_created, fancytime_since_created), true);

        time_t time_since_joined = time(nullptr) - member.joined_at;
        std::string fancytime_since_joined;
        if (time_since_joined > 604800) {
            fancytime_since_joined = util::seconds_to_fancytime(time_since_joined, 1);
        } else {
            fancytime_since_joined = util::seconds_to_fancytime(time_since_joined, 2);
        }
        embed.add_field("Joined server:", std::format("<t:{}> ({} ago)", member.joined_at, fancytime_since_joined), true);
    } catch (const dpp::logic_exception&) {
        time_t user_created = user.get_creation_time();
        time_t time_since_created = time(nullptr) - user_created;
        std::string fancytime_since_created;
        if (time_since_created > 604800) {
            fancytime_since_created = util::seconds_to_fancytime(time_since_created, 1);
        } else {
            fancytime_since_created = util::seconds_to_fancytime(time_since_created, 2);
        }
        embed.add_field("Account created:", std::format("<t:{}> ({} ago)", user_created, fancytime_since_created), true);
    }
    event.reply(dpp::message(embed));
}

dpp::task<> moderation::inviteinfo(const dpp::slashcommand_t &event) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking();
    // Get invite code and make sure it's not a link
    std::string code = std::get<std::string>(event.get_parameter("code"));
    if (code.substr(0, 8) == "https://") {
        co_await thinking;
        event.edit_original_response(dpp::message("Please send just the invite code, not the entire link."));
        co_return;
    }
    // Get invite from code
    dpp::confirmation_callback_t confirmation = co_await event.owner->co_invite_get(code);
    if (confirmation.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message("Failed to find invite."));
        co_return;
    }
    dpp::invite invite = std::get<dpp::invite>(confirmation.value);
    // Build embed with invite info
    dpp::embed embed = dpp::embed().set_color(0x00A0A0).set_thumbnail(invite.destination_guild.get_icon_url())
                                   .set_url("https://discord.gg/" + invite.code)
                                   .set_title("Invite to " + invite.destination_guild.name)
                                   .add_field("Members total", std::to_string(invite.approximate_member_count), true)
                                   .add_field("Members online", std::to_string(invite.approximate_presence_count), true)
                                   .add_field("Invite creator", invite.inviter.username, false)
                                   .add_field("Server ID", invite.guild_id.str(), true)
                                   .add_field("Channel name", invite.destination_channel.name, true)
                                   .add_field("Invite uses", std::to_string(invite.uses), false)
                                   .set_timestamp(invite.created_at);
    if (invite.expires_at == 0) {
        embed.add_field("Expires", "Never", false);
    } else {
        embed.add_field("Expires", std::format("<t:{}>", invite.expires_at), false);
    }
    co_await thinking;
    event.edit_original_response(dpp::message(embed));
}