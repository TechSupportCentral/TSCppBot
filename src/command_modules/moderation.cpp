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

// TODO: Trial mods should be able to warn, mute, and kick, but not ban

/**
 * Make sure a moderator is allowed to run a command against another user
 * @param bot Pointer to bot cluster to find guild members with
 * @param config JSON bot configuration
 * @param issuer ID of user who issued the moderation command
 * @param subject ID of user the command is acting on
 * @return true if the issuer is higher in the role hierarchy than the subject
 */
dpp::task<bool> check_perms(dpp::cluster* bot, const nlohmann::json& config, const dpp::snowflake issuer, const dpp::snowflake subject) {
    const dpp::snowflake hierarchy[3] = {config["role_ids"]["owner"], config["role_ids"]["moderator"], config["role_ids"]["trial_mod"]};
    const dpp::confirmation_callback_t issuer_conf = co_await bot->co_guild_get_member(config["guild_id"], issuer);
    const dpp::confirmation_callback_t subject_conf = co_await bot->co_guild_get_member(config["guild_id"], subject);
    // If the command issuer is not a server member, they do not have permission
    if (issuer_conf.is_error()) {
        co_return false;
    }
    // If the command subject is not a server member, they cannot be in the hierarchy so the issuer has permission
    if (subject_conf.is_error()) {
        co_return true;
    }

    // Get hierarchy index of role of highest rank for issuer and subject
    int issuer_rank_index = 3;
    int subject_rank_index = 3;
    std::vector<dpp::snowflake> issuer_roles = std::get<dpp::guild_member>(issuer_conf.value).get_roles();
    std::vector<dpp::snowflake> subject_roles = std::get<dpp::guild_member>(subject_conf.value).get_roles();
    for (int i = 0; i < 3; i++) {
        if (std::ranges::find(issuer_roles, hierarchy[i]) != issuer_roles.end()) {
            issuer_rank_index = i;
            break;
        }
    }
    for (int i = 0; i < 3; i++) {
        if (std::ranges::find(subject_roles, hierarchy[i]) != subject_roles.end()) {
            subject_rank_index = i;
            break;
        }
    }
    co_return issuer_rank_index < subject_rank_index;
}

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
    if (! co_await check_perms(event.owner, config, event.command.get_issuing_user().id, user.user_id)) {
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
    sqlite3_exec(db, std::format("INSERT INTO mod_records VALUES ('{}', 'warn', '{}', '{}', {}, 'true', NULL);",
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
    if (! co_await check_perms(event.owner, config, event.command.get_issuing_user().id, user_id)) {
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
    if (! co_await check_perms(event.owner, config, event.command.get_issuing_user().id, user.user_id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(user.get_mention() + "'s rank is higher than or equal to yours, cannot mute."));
        co_return;
    }

    // Add muted role to user and get time of this action
    time_t now = time(nullptr);
    dpp::confirmation_callback_t role_add_conf = co_await event.owner->co_guild_member_add_role(config["guild_id"], user.user_id, config["role_ids"]["muted"]);
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
        sqlite3_exec(db, std::format("INSERT INTO mod_records VALUES ('{}', 'mute', '{}', '{}', {}, 'true', {});",
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
    if (! co_await check_perms(event.owner, config, event.command.get_issuing_user().id, user.user_id)) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Cannot unmute <@{}>, whose rank is higher than or equal to yours.", user.user_id.str())));
        co_return;
    }

    // Remove mute role
    dpp::confirmation_callback_t unmute_conf = co_await event.owner->co_guild_member_delete_role(config["guild_id"], user.user_id, config["role_ids"]["muted"]);
    if (unmute_conf.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::string("Failed to remove muted role from ") + user.get_mention()));
        co_return;
    }
    // Get mute ID if it exists in DB
    dpp::snowflake id = 0;
    char* error_message;
    sqlite3_exec(db, std::format("SELECT id FROM mod_records WHERE user='{}' AND active='true';", user.user_id.str()).c_str(),
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

    // Create unwarn message and send DM to user
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
        event.edit_original_response(dpp::message(std::format("User <@{}> is not a member of the server",
                                     std::get<dpp::snowflake>(event.get_parameter("user")).str())));
        co_return;
    }
    std::string reason = std::get<std::string>(event.get_parameter("reason"));
    // Check hierarchy
    if (! co_await check_perms(event.owner, config, event.command.get_issuing_user().id, user.id)) {
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
        co_await thinking;
        if (dm_conf.is_error()) {
            event.edit_original_response(dpp::message("Failed to kick user."));
        } else {
            event.edit_original_response(dpp::message("Failed to kick user, but kick message was sent."));
        }
        co_return;
    }

    // Send empty embed to log channel and get the message ID
    dpp::confirmation_callback_t log_conf = co_await event.owner->co_message_create(dpp::message(config["log_channel_ids"]["mod_log"], dpp::embed().set_title(".")));
    if (log_conf.is_error()) {
        co_await thinking;
        if (dm_conf.is_error()) {
            event.edit_original_response(dpp::message("User kicked successfully, but failed to send kick message."));
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

    // Add warning to DB
    char* error_message;
    sqlite3_exec(db, std::format("INSERT INTO mod_records VALUES ('{}', 'kick', '{}', '{}', {}, 'true', NULL);",
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