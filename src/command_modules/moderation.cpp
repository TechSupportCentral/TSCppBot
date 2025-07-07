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

        auto user_created = static_cast<time_t>(user.get_creation_time());
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
        auto user_created = static_cast<time_t>(user.get_creation_time());
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

dpp::task<> moderation::warn(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Get context variables
    dpp::guild_member user;
    try {
        user = event.command.get_resolved_member(std::get<dpp::snowflake>(event.get_parameter("user")));
    } catch (const dpp::logic_exception&) {
        user.user_id = 0;
    }
    if (user.user_id == 0) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("User <@{}> is not a member of the server",
                                     std::get<dpp::snowflake>(event.get_parameter("user")).str())));
        co_return;
    }
    std::string reason = std::get<std::string>(event.get_parameter("reason"));
    // Check hierarchy
    if (! co_await util::check_perms(event.owner, config, event.command.get_issuing_user().id, user.user_id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(user.get_mention() + "'s rank is higher than or equal to yours, cannot warn."));
        co_return;
    }

    // Create warning message and send DM to user
    dpp::embed dm_embed = dpp::embed().set_color(dpp::colors::red).set_title("You have been warned.").add_field("Reason", reason, false);
    dpp::confirmation_callback_t dm_conf = co_await event.owner->co_direct_message_create(user.user_id, dpp::message(dm_embed));

    // Send empty embed to log channel and get the message ID
    dpp::confirmation_callback_t log_conf = co_await event.owner->co_message_create(dpp::message(config["log_channel_ids"]["mod_log"], dpp::embed().set_title(".")));
    if (log_conf.is_error()) {
        co_await thinking;
        if (dm_conf.is_error()) {
            event.edit_original_response(dpp::message("Failed to send warning message."));
        } else {
            event.edit_original_response(dpp::message("User warned successfully, but failed to create logs."));
        }
        co_return;
    }
    dpp::message log_message = std::get<dpp::message>(log_conf.value);
    // Edit the message with warning info
    log_message.embeds[0].set_color(dpp::colors::red).set_title("Warning").set_thumbnail(user.get_user()->get_avatar_url())
    .set_description(std::format("Use `/unwarn {} <reason>` to remove this warning. Note: This is not the user's ID, rather the ID of this message.", log_message.id.str()))
    .add_field("User warned", user.get_nickname(), true)
    .add_field("User ID", user.user_id.str(), true)
    .add_field("Warned by", event.command.get_issuing_user().global_name, false)
    .add_field("Reason", reason, false);
    if (dm_conf.is_error()) {
        log_message.embeds[0].set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    event.owner->message_edit(log_message);

    // Add warning to DB
    char* error_message;
    sqlite3_exec(db, std::format("INSERT INTO mod_records VALUES ('{}', 'Warning', '{}', '{}', {}, 'true', NULL);",
                                 log_message.id.str(),
                                 event.command.get_issuing_user().id.str(),
                                 user.user_id.str(),
                                 util::sql_escape_string(reason, true)
                     ).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message("User warned successfully, but failed to add DB entry."));
        co_return;
    }
    co_await thinking;
    event.edit_original_response(dpp::message("User warned successfully."));
}

dpp::task<> moderation::unwarn(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    std::string reason = std::get<std::string>(event.get_parameter("reason"));
    // Get ID and make sure it's a number
    std::string id = std::get<std::string>(event.get_parameter("id"));
    if (std::ranges::any_of(id, [](const char& c){return !std::isdigit(c);})) {
        co_await thinking;
        event.edit_original_response(dpp::message("Invalid warning message ID."));
        co_return;
    }
    // Make sure warning exists in DB and get warn reason and associated user ID
    dpp::snowflake user_id = 0;
    std::string original_reason;
    std::pair callback_data = {&user_id, &original_reason};
    char* error_message;
    sqlite3_exec(db, std::format("SELECT user, reason FROM mod_records WHERE id='{}';", id).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto data = static_cast<std::pair<dpp::snowflake*, std::string*>*>(output);
            *data->first = strtoull(column_values[0], nullptr, 10);
            data->second->append(column_values[1]);
            return 0;
        },
    &callback_data, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message("Failed to get warning from DB."));
        co_return;
    }
    if (user_id == 0) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::string("Could not find warning with ID ") + id));
        co_return;
    }
    // Check hierarchy
    if (! co_await util::check_perms(event.owner, config, event.command.get_issuing_user().id, user_id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Cannot remove warning on <@{}>, whose rank is higher than or equal to yours.", user_id.str())));
        co_return;
    }

    // Set warning inactive in DB
    sqlite3_exec(db, std::format("UPDATE mod_records SET active = 'false' WHERE id='{}';", id).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message("Failed to set warning inactive in DB."));
        co_return;
    }
    // Create unwarn message and send DM to user
    dpp::embed dm_embed = dpp::embed().set_color(dpp::colors::green).set_title("Your warning has been removed.")
                                      .add_field("Original warning reason", original_reason, false)
                                      .add_field("Reason for removal", reason, false);
    dpp::confirmation_callback_t dm_conf = co_await event.owner->co_direct_message_create(user_id, dpp::message(dm_embed));

    // Find user
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(user_id);
    if (user_conf.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message("Could not find warned user on Discord."));
        co_return;
    }
    dpp::user_identified user = std::get<dpp::user_identified>(user_conf.value);
    // Create and send log message
    dpp::embed log_embed = dpp::embed().set_color(dpp::colors::green).set_thumbnail(user.get_avatar_url())
    .set_title("Warning Removed")
    .add_field("User warned", user.username, true)
    .add_field("User ID", user_id.str(), true)
    .add_field("Warning removed by", event.command.get_issuing_user().global_name, false)
    .add_field("Original warn reason", original_reason, false)
    .add_field("Reason for removal", reason, false);
    if (dm_conf.is_error()) {
        log_embed.set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["mod_log"], log_embed));
    // Send confirmation
    co_await thinking;
    event.edit_original_response(dpp::message("Warning removed successfully."));
}

dpp::task<> moderation::mute(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Get context variables
    dpp::guild_member user;
    try {
        user = event.command.get_resolved_member(std::get<dpp::snowflake>(event.get_parameter("user")));
    } catch (const dpp::logic_exception&) {
        user.user_id = 0;
    }
    if (user.user_id == 0) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("User <@{}> is not a member of the server",
                                     std::get<dpp::snowflake>(event.get_parameter("user")).str())));
        co_return;
    }
    std::string seconds_str = std::get<std::string>(event.get_parameter("time"));
    time_t seconds = util::time_string_to_seconds(seconds_str);
    if (seconds == -1) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format(
        "Time string `{}` is not in the correct format.\n", seconds_str)
        + "It should look something like `1d2h3m4s` (1 day, 2 hours, 3 minutes, and 4 seconds)."));
        co_return;
    }
    std::string fancytime = util::seconds_to_fancytime(seconds, 2);
    std::string reason;
    try {
        reason = std::get<std::string>(event.get_parameter("reason"));
    } catch (const std::bad_variant_access&) {
        reason = "No reason provided.";
    }
    // Check hierarchy
    if (! co_await util::check_perms(event.owner, config, event.command.get_issuing_user().id, user.user_id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(user.get_mention() + "'s rank is higher than or equal to yours, cannot mute."));
        co_return;
    }

    // Add muted role to user and get time of this action
    time_t now = time(nullptr);
    dpp::confirmation_callback_t role_add_conf = co_await event.owner->co_guild_member_add_role(user.guild_id, user.user_id, config["role_ids"]["muted"]);
    if (role_add_conf.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message("Failed to add muted role to user."));
        co_return;
    }
    util::mute mute = {0, user.user_id, now, now + seconds};

    // Create mute message and send DM to user
    dpp::embed dm_embed = dpp::embed().set_color(dpp::colors::red)
                                      .set_title(std::format("You have been muted for {}.", fancytime))
                                      .add_field("Reason", reason, false);
    dpp::confirmation_callback_t dm_conf = co_await event.owner->co_direct_message_create(mute.user, dpp::message(dm_embed));

    // Send empty embed to log channel and get the message ID
    dpp::confirmation_callback_t log_conf = co_await event.owner->co_message_create(dpp::message(config["log_channel_ids"]["mod_log"], dpp::embed().set_title(".")));
    if (log_conf.is_error()) {
        co_await thinking;
        if (dm_conf.is_error()) {
            event.edit_original_response(dpp::message("User muted successfully, but failed to notify them."));
        } else {
            event.edit_original_response(dpp::message("User muted successfully, but failed to create logs."));
        }
        util::handle_mute(event.owner, db, config, mute);
        co_return;
    }
    dpp::message log_message = std::get<dpp::message>(log_conf.value);
    // Edit the message with mute info
    log_message.embeds[0].set_color(dpp::colors::red).set_thumbnail(user.get_user()->get_avatar_url())
    .set_title(std::string("Mute for ") + fancytime)
    .add_field("User muted", user.get_nickname(), true)
    .add_field("User ID", mute.user.str(), true)
    .add_field("Muted by", event.command.get_issuing_user().global_name, false)
    .add_field("Reason", reason, false);
    if (dm_conf.is_error()) {
        log_message.embeds[0].set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    event.owner->message_edit(log_message);

    // Add mute to DB
    char* error_message;
    sqlite3_exec(db, std::format("INSERT INTO mutes VALUES (NULL, {}, {});",
                                mute.start_time, mute.end_time).c_str(),
                 nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message("User muted successfully, but failed to add DB entry."));
    } else {
        mute.id = sqlite3_last_insert_rowid(db);
        sqlite3_exec(db, std::format("INSERT INTO mod_records VALUES ('{}', 'Mute', '{}', '{}', {}, 'true', {});",
                                     log_message.id.str(),
                                     event.command.get_issuing_user().id.str(),
                                     mute.user.str(),
                                     util::sql_escape_string(reason, true),
                                     mute.id
                         ).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
            co_await thinking;
            event.edit_original_response(dpp::message("User muted successfully, but failed to add DB entry."));
        } else {
            co_await thinking;
            event.edit_original_response(dpp::message("User muted successfully."));
        }
    }
    util::handle_mute(event.owner, db, config, mute);
}

dpp::task<> moderation::unmute(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Get context variables
    dpp::guild_member user;
    try {
        user = event.command.get_resolved_member(std::get<dpp::snowflake>(event.get_parameter("user")));
    } catch (const dpp::logic_exception&) {
        user.user_id = 0;
    }
    if (user.user_id == 0) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("User <@{}> is not a member of the server",
                                     std::get<dpp::snowflake>(event.get_parameter("user")).str())));
        co_return;
    }
    std::string reason;
    try {
        reason = std::get<std::string>(event.get_parameter("reason"));
    } catch (const std::bad_variant_access&) {
        reason = "No reason provided.";
    }
    // Make sure user is muted
    std::vector<dpp::snowflake> roles = user.get_roles();
    if (std::ranges::find(roles, config["role_ids"]["muted"].get<dpp::snowflake>()) == roles.end()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("User {} is not muted.", user.get_mention())));
        co_return;
    }
    // Check hierarchy
    if (! co_await util::check_perms(event.owner, config, event.command.get_issuing_user().id, user.user_id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Cannot unmute {}, whose rank is higher than or equal to yours.", user.get_mention())));
        co_return;
    }

    // Remove mute role
    dpp::confirmation_callback_t unmute_conf = co_await event.owner->co_guild_member_delete_role(user.guild_id, user.user_id, config["role_ids"]["muted"]);
    if (unmute_conf.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::string("Failed to remove muted role from ") + user.get_mention()));
        co_return;
    }
    // Get mute ID if it exists in DB
    dpp::snowflake id = 0;
    char* error_message;
    sqlite3_exec(db, std::format("SELECT id FROM mod_records WHERE user='{}' AND type='Mute' AND active='true';", user.user_id.str()).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto id = static_cast<dpp::snowflake*>(output);
            *id = strtoull(column_values[0], nullptr, 10);
            return 0;
        },
    &id, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
    }
    if (id != 0) {
        // Set mute inactive in DB
        sqlite3_exec(db, std::format("UPDATE mod_records SET active = 'false' WHERE id='{}';", id.str()).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
        }
    }

    // Create unmute message and send DM to user
    dpp::embed dm_embed = dpp::embed().set_color(dpp::colors::green).set_title("You have been unmuted.")
                                      .add_field("Reason", reason, false);
    dpp::confirmation_callback_t dm_conf = co_await event.owner->co_direct_message_create(user.user_id, dpp::message(dm_embed));
    // Create and send log message
    dpp::embed log_embed = dpp::embed().set_color(dpp::colors::green).set_thumbnail(user.get_user()->get_avatar_url())
    .set_title("Mute Removed")
    .add_field("User unmuted", user.get_nickname(), true)
    .add_field("User ID", user.user_id.str(), true)
    .add_field("Mute removed by", event.command.get_issuing_user().global_name, false)
    .add_field("Reason", reason, false);
    if (dm_conf.is_error()) {
        log_embed.set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["mod_log"], log_embed));
    // Send confirmation
    co_await thinking;
    event.edit_original_response(dpp::message("Mute removed successfully."));
}

dpp::task<> moderation::kick(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Get context variables
    dpp::user user = event.command.get_resolved_user(std::get<dpp::snowflake>(event.get_parameter("user")));
    try {
        dpp::guild_member test_member = event.command.get_resolved_member(user.id);
    } catch (const dpp::logic_exception&) {
        user.id = 0;
    }
    if (user.id == 0) {
        co_await thinking;
        event.edit_original_response(dpp::message(user.get_mention() + " is not a member of the server."));
        co_return;
    }
    std::string reason;
    try {
        reason = std::get<std::string>(event.get_parameter("reason"));
    } catch (const std::bad_variant_access&) {
        reason = "No reason provided.";
    }
    // Check hierarchy
    if (! co_await util::check_perms(event.owner, config, event.command.get_issuing_user().id, user.id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(user.get_mention() + "'s rank is higher than or equal to yours, cannot kick."));
        co_return;
    }

    // Create kick message and send DM to user
    dpp::embed dm_embed = dpp::embed().set_color(dpp::colors::red).set_title("You have been kicked.").add_field("Reason", reason, false);
    dpp::confirmation_callback_t dm_conf = co_await event.owner->co_direct_message_create(user.id, dpp::message(dm_embed));
    // Kick user
    dpp::confirmation_callback_t kick_conf = co_await event.owner->co_guild_member_kick(config["guild_id"], user.id);
    if (kick_conf.is_error()) {
        // If user was DMed about being kicked but wasn't actually kicked, delete the notification
        if (!dm_conf.is_error()) {
            dpp::message kick_dm = std::get<dpp::message>(kick_conf.value);
            event.owner->message_delete(kick_dm.id, kick_dm.channel_id);
        }
        co_await thinking;
        event.edit_original_response(dpp::message("Failed to kick user."));
        co_return;
    }

    // Send empty embed to log channel and get the message ID
    dpp::confirmation_callback_t log_conf = co_await event.owner->co_message_create(dpp::message(config["log_channel_ids"]["mod_log"], dpp::embed().set_title(".")));
    if (log_conf.is_error()) {
        co_await thinking;
        if (dm_conf.is_error()) {
            event.edit_original_response(dpp::message("User kicked successfully, but failed to notify them."));
        } else {
            event.edit_original_response(dpp::message("User kicked successfully, but failed to create logs."));
        }
        co_return;
    }
    dpp::message log_message = std::get<dpp::message>(log_conf.value);
    // Edit the message with kick info
    log_message.embeds[0].set_color(dpp::colors::red).set_title("Kick").set_thumbnail(user.get_avatar_url())
    .add_field("User kicked", user.username, true)
    .add_field("User ID", user.id.str(), true)
    .add_field("Kicked by", event.command.get_issuing_user().global_name, false)
    .add_field("Reason", reason, false);
    if (dm_conf.is_error()) {
        log_message.embeds[0].set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    event.owner->message_edit(log_message);

    // Add kick to DB
    char* error_message;
    sqlite3_exec(db, std::format("INSERT INTO mod_records VALUES ('{}', 'Kick', '{}', '{}', {}, 'true', NULL);",
                                 log_message.id.str(),
                                 event.command.get_issuing_user().id.str(),
                                 user.id.str(),
                                 util::sql_escape_string(reason, true)
                     ).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message("User kicked successfully, but failed to add DB entry."));
        co_return;
    }
    co_await thinking;
    event.edit_original_response(dpp::message("User kicked successfully."));
}

dpp::task<> moderation::ban(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Get context variables
    dpp::user user = event.command.get_resolved_user(std::get<dpp::snowflake>(event.get_parameter("user")));
    std::string reason;
    try {
        reason = std::get<std::string>(event.get_parameter("reason"));
    } catch (const std::bad_variant_access&) {
        reason = "No reason provided.";
    }
    uint32_t seconds;
    try {
        seconds = std::get<int64_t>(event.get_parameter("delete_message_hours")) * 3600UL;
    } catch (const std::bad_variant_access&) {
        seconds = 0;
    }
    // Make sure user is not already banned
    dpp::confirmation_callback_t get_ban = co_await event.owner->co_guild_get_ban(config["guild_id"], user.id);
    if (!get_ban.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(user.get_mention() + " is already banned from the server."));
        co_return;
    }
    // Check hierarchy
    if (! co_await util::check_perms(event.owner, config, event.command.get_issuing_user().id, user.id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(user.get_mention() + "'s rank is higher than or equal to yours, cannot ban."));
        co_return;
    }

    // Create ban message and send DM to user
    dpp::embed dm_embed = dpp::embed().set_color(dpp::colors::red).set_title("You have been banned.").add_field("Reason", reason, false);
    dpp::confirmation_callback_t dm_conf = co_await event.owner->co_direct_message_create(user.id, dpp::message(dm_embed));
    // Ban user
    dpp::confirmation_callback_t ban_conf = co_await event.owner->co_guild_ban_add(config["guild_id"], user.id, seconds);
    if (ban_conf.is_error()) {
        // If user was DMed about being banned but wasn't actually banned, delete the notification
        if (!dm_conf.is_error()) {
            dpp::message ban_dm = std::get<dpp::message>(ban_conf.value);
            event.owner->message_delete(ban_dm.id, ban_dm.channel_id);
        }
        co_await thinking;
        event.edit_original_response(dpp::message("Failed to ban user."));
        co_return;
    }

    // Send empty embed to log channel and get the message ID
    dpp::confirmation_callback_t log_conf = co_await event.owner->co_message_create(dpp::message(config["log_channel_ids"]["mod_log"], dpp::embed().set_title(".")));
    if (log_conf.is_error()) {
        co_await thinking;
        if (dm_conf.is_error()) {
            event.edit_original_response(dpp::message("User banned successfully, but failed to notify them."));
        } else {
            event.edit_original_response(dpp::message("User banned successfully, but failed to create logs."));
        }
        co_return;
    }
    dpp::message log_message = std::get<dpp::message>(log_conf.value);
    // Edit the message with ban info
    log_message.embeds[0].set_color(dpp::colors::red).set_title("Ban").set_thumbnail(user.get_avatar_url())
    .add_field("User banned", user.username, true)
    .add_field("User ID", user.id.str(), true)
    .add_field("Banned by", event.command.get_issuing_user().global_name, false)
    .add_field("Reason", reason, false);
    if (seconds != 0) {
        log_message.embeds[0].set_description(std::format("User's messages from the last {} were deleted.", util::seconds_to_fancytime(seconds, 2)));
    }
    if (dm_conf.is_error()) {
        log_message.embeds[0].set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    event.owner->message_edit(log_message);

    // Add ban to DB
    char* error_message;
    sqlite3_exec(db, std::format("INSERT INTO mod_records VALUES ('{}', 'Ban', '{}', '{}', {}, 'true', {});",
                                 log_message.id.str(),
                                 event.command.get_issuing_user().id.str(),
                                 user.id.str(),
                                 util::sql_escape_string(reason, true),
                                 seconds
                     ).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message("User banned successfully, but failed to add DB entry."));
        co_return;
    }
    co_await thinking;
    event.edit_original_response(dpp::message("User banned successfully."));
}

dpp::task<> moderation::unban(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Get context variables
    dpp::user user = event.command.get_resolved_user(std::get<dpp::snowflake>(event.get_parameter("user")));
    std::string reason;
    try {
        reason = std::get<std::string>(event.get_parameter("reason"));
    } catch (const std::bad_variant_access&) {
        reason = "No reason provided.";
    }
    // Make sure user is banned
    dpp::confirmation_callback_t get_ban = co_await event.owner->co_guild_get_ban(config["guild_id"], user.id);
    if (get_ban.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("User {} is not banned.", user.get_mention())));
        co_return;
    }
    // Check hierarchy
    if (! co_await util::check_perms(event.owner, config, event.command.get_issuing_user().id, user.id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Cannot unmute {}, whose rank is higher than or equal to yours.", user.get_mention())));
        co_return;
    }

    // Remove ban
    dpp::confirmation_callback_t unban_conf = co_await event.owner->co_guild_ban_delete(config["guild_id"], user.id);
    if (unban_conf.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::string("Failed to remove ban of ") + user.get_mention()));
        co_return;
    }
    // Get ban ID if it exists in DB
    dpp::snowflake id = 0;
    char* error_message;
    sqlite3_exec(db, std::format("SELECT id FROM mod_records WHERE user='{}' AND type='Ban' AND active='true';", user.id.str()).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto id = static_cast<dpp::snowflake*>(output);
            *id = strtoull(column_values[0], nullptr, 10);
            return 0;
        },
    &id, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
    }
    if (id != 0) {
        // Set ban inactive in DB
        sqlite3_exec(db, std::format("UPDATE mod_records SET active = 'false' WHERE id='{}';", id.str()).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
        }
    }

    // Create unban message and send DM to user
    dpp::embed dm_embed = dpp::embed().set_color(dpp::colors::green).set_title("Your ban has been removed.")
                                      .add_field("Reason", reason, false);
    dpp::confirmation_callback_t dm_conf = co_await event.owner->co_direct_message_create(user.id, dpp::message(dm_embed));
    // Create and send log message
    dpp::embed log_embed = dpp::embed().set_color(dpp::colors::green).set_thumbnail(user.get_avatar_url())
    .set_title("Ban Removed")
    .add_field("User unbanned", user.global_name, true)
    .add_field("User ID", user.id.str(), true)
    .add_field("Ban removed by", event.command.get_issuing_user().global_name, false)
    .add_field("Reason", reason, false);
    if (dm_conf.is_error()) {
        log_embed.set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["mod_log"], log_embed));
    // Send confirmation
    co_await thinking;
    event.edit_original_response(dpp::message("Ban removed successfully."));
}

void moderation::get_mod_actions(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking();
    dpp::user user = event.command.get_resolved_user(std::get<dpp::snowflake>(event.get_parameter("user")));
    // Get all actions against user
    struct action {
        dpp::snowflake id;
        std::string type = "Unknown";
        dpp::snowflake mod;
        std::string reason = "No reason provided.";
        bool active = false;
        time_t mute_seconds = 0; // stays 0 if type != "Mute"
    };
    std::vector<action> actions;
    char* error_message;
    sqlite3_exec(db, std::format("SELECT id, type, moderator, reason, active, extra_data FROM mod_records WHERE user='{}';", user.id.str()).c_str(),
        [](void* action_list, int column_count, char** column_values, char** column_names) -> int {
            auto actions = static_cast<std::vector<action>*>(action_list);
            action action;
            action.id = strtoull(column_values[0], nullptr, 10);
            if (column_values[1] != nullptr) {
                action.type = column_values[1];
            }
            action.mod = strtoull(column_values[2], nullptr, 10);
            if (column_values[3] != nullptr) {
                action.reason = column_values[3];
            }
            if (column_values[4] != nullptr) {
                action.active = strcmp("false", column_values[4]);
            }
            if (column_values[5] != nullptr && action.type == "Mute") {
                // Temporarily store mute ext info ID in mute_seconds
                action.mute_seconds = strtoll(column_values[5], nullptr, 10);
            }
            actions->push_back(action);
            return 0;
        },
    &actions, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to get warnings from DB."));
        return;
    }
    // Get mute time for mute actions
    for (action& mod_action : actions) {
        if (mod_action.type == "Mute") {
            sqlite3_exec(db, std::format("SELECT start_time, end_time FROM mutes WHERE id={};", mod_action.mute_seconds).c_str(),
                [](void* output, int column_count, char** column_values, char** column_names) -> int {
                    auto mod_action = static_cast<action*>(output);
                    time_t start_time = strtoll(column_values[0], nullptr, 10);
                    time_t end_time = strtoll(column_values[1], nullptr, 10);
                    mod_action->mute_seconds = end_time - start_time;
                    return 0;
                },
            &mod_action, &error_message);
            if (error_message != nullptr) {
                util::log("SQL ERROR", error_message);
                sqlite3_free(error_message);
                mod_action.mute_seconds = 0;
            }
        }
    }

    // Print actions in groups of 4 because embeds can only have up to 25 fields
    std::vector<dpp::embed> embeds((actions.size() + 3) / 4);
    // Initialize embeds
    embeds[0] = dpp::embed().set_color(0x00A0A0).set_thumbnail(user.get_avatar_url())
                            .set_title(std::string("Moderation actions against ") + user.username);
    for (size_t i = 1; i < embeds.size(); i++) {
        embeds[i] = dpp::embed().set_color(0x00A0A0).set_title(user.username + "'s warnings continued");
    }
    // Add fields for each action attribute
    for (size_t i = 0; i < actions.size(); i++) {
        embeds[i / 4].add_field("ID", actions[i].id.str(), false)
                     .add_field("Type", actions[i].type, true)
                     .add_field("Reason", actions[i].reason, true)
                     .add_field("Moderator User ID", actions[i].mod.str(), true)
                     .add_field("Active", actions[i].active ? "true" : "false", true);
        if (actions[i].type == "Mute") {
            embeds[i / 4].add_field("Muted for", util::seconds_to_fancytime(actions[i].mute_seconds, 4), true);
        }
    }
    // Send each embed
    event.edit_original_response(dpp::message(event.command.channel_id, embeds[0]));
    for (size_t i = 1; i < embeds.size(); i++) {
        event.owner->message_create(dpp::message(event.command.channel_id, embeds[i]));
    }
}