/* members: Handlers for events involving server members
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
#include "members.h"
#include "../util.h"

dpp::task<dpp::invite> members::findinvite(dpp::cluster* bot, const dpp::snowflake guild, std::vector<dpp::invite>& invites) {
    dpp::invite invite_used = dpp::invite();
    invite_used.code = "unknown";

    // Get current invite list
    dpp::confirmation_callback_t invites_conf = co_await bot->co_guild_get_invites(guild);
    if (invites_conf.is_error()) {
        co_return invite_used;
    }
    // Look for any invite with more uses than the cache has
    for (size_t i = 0; i < invites.size(); i++) {
        for (const dpp::invite& invite : std::get<dpp::invite_map>(invites_conf.value) | std::views::values) {
            if (invite.code == invites[i].code && invite.uses > invites[i].uses) {
                // Update invite in cache
                invites[i] = invite;

                invite_used = invite;
                co_return invite_used;
            }
        }
    }
    co_return invite_used;
}

dpp::task<> members::on_kick(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.entry.target_id);
    dpp::confirmation_callback_t mod_conf = co_await event.owner->co_user_get_cached(event.entry.user_id);

    dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_title("Manual Kick");
    if (!user_conf.is_error()) {
        dpp::user_identified user = std::get<dpp::user_identified>(user_conf.value);
        embed.set_thumbnail(user.get_avatar_url()).add_field("User kicked", user.username, true);
    }
    embed.add_field("User ID", event.entry.target_id.str(), true);

    if (mod_conf.is_error()) {
        embed.add_field("Moderator ID", event.entry.user_id.str(), false);
    } else {
        dpp::user_identified mod = std::get<dpp::user_identified>(mod_conf.value);
        embed.add_field("Kicked by", mod.global_name, false);
    }

    if (!event.entry.reason.empty()) {
        embed.add_field("Reason", event.entry.reason, false);
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["mod_log"], embed));
}

dpp::task<> members::on_ban(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.entry.target_id);
    dpp::confirmation_callback_t mod_conf = co_await event.owner->co_user_get_cached(event.entry.user_id);

    dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_title("Manual Ban");
    if (!user_conf.is_error()) {
        dpp::user_identified user = std::get<dpp::user_identified>(user_conf.value);
        embed.set_thumbnail(user.get_avatar_url()).add_field("User banned", user.username, true);
    }
    embed.add_field("User ID", event.entry.target_id.str(), true);

    if (mod_conf.is_error()) {
        embed.add_field("Moderator ID", event.entry.user_id.str(), false);
    } else {
        dpp::user_identified mod = std::get<dpp::user_identified>(mod_conf.value);
        embed.add_field("Banned by", mod.global_name, false);
    }

    if (!event.entry.reason.empty()) {
        embed.add_field("Reason", event.entry.reason, false);
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["mod_log"], embed));
}

dpp::task<> members::on_unban(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.entry.target_id);
    dpp::confirmation_callback_t mod_conf = co_await event.owner->co_user_get_cached(event.entry.user_id);

    dpp::embed embed = dpp::embed().set_color(dpp::colors::green).set_title("Ban Manually Removed");
    if (!user_conf.is_error()) {
        dpp::user_identified user = std::get<dpp::user_identified>(user_conf.value);
        embed.set_thumbnail(user.get_avatar_url()).add_field("User unbanned", user.username, true);
    }
    embed.add_field("User ID", event.entry.target_id.str(), true);

    if (mod_conf.is_error()) {
        embed.add_field("Moderator ID", event.entry.user_id.str(), false);
    } else {
        dpp::user_identified mod = std::get<dpp::user_identified>(mod_conf.value);
        embed.add_field("Ban removed by", mod.global_name, false);
    }

    if (!event.entry.reason.empty()) {
        embed.add_field("Reason", event.entry.reason, false);
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["mod_log"], embed));
}

dpp::task<> members::on_join(const dpp::guild_member_add_t& event, const nlohmann::json& config, std::vector<dpp::invite>& invites) {
    dpp::embed welcome_embed = dpp::embed().set_color(dpp::colors::green).set_title("New Member")
                                           .set_thumbnail(event.added.get_user()->get_avatar_url())
                                           .set_description(event.added.get_user()->global_name + " has joined Tech Support Central!");
    event.owner->message_create(dpp::message(config["log_channel_ids"]["welcome"], welcome_embed));

    dpp::embed log_embed = dpp::embed().set_color(dpp::colors::green).set_title("Member Joined")
                                       .set_thumbnail(event.added.get_user()->get_avatar_url())
                                       .set_description(event.added.get_user()->username)
                                       .add_field("User ID", event.added.user_id.str(), false)
                                       .add_field("Account Created:", std::format("<t:{}>",
                                                  static_cast<time_t>(event.added.get_user()->get_creation_time())), false);
    dpp::invite invite_used = co_await findinvite(event.owner, event.adding_guild.id, invites);
    if (invite_used.code != "unknown") {
        log_embed.add_field("Invite code", invite_used.code, false);
        if (invite_used.code == "2vwUBmhM8U") {
            log_embed.add_field("Invite creator", "top.gg", true);
        } else {
            log_embed.add_field("Invite creator", invite_used.inviter.username, true);
        }
        log_embed.add_field("Invite uses", std::to_string(invite_used.uses), true);
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["member_log"], log_embed));
}

void members::on_leave(const dpp::guild_member_remove_t& event, const nlohmann::json& config) {
    dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_title("Member Left")
                                   .set_thumbnail(event.removed.get_avatar_url())
                                   .set_description(event.removed.username + " has left Tech Support Central.")
                                   .set_footer(dpp::embed_footer().set_text(std::string("User ID: ") + event.removed.id.str()));
    event.owner->message_create(dpp::message(config["log_channel_ids"]["member_log"], embed));
}