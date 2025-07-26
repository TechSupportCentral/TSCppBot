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
    for (dpp::invite& cached_invite : invites) {
        for (const dpp::invite& invite : std::get<dpp::invite_map>(invites_conf.value) | std::views::values) {
            if (invite.code == cached_invite.code && invite.uses > cached_invite.uses) {
                // Update invite in cache
                cached_invite = invite;

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

    dpp::embed embed = dpp::embed().set_color(util::color::RED).set_title("Manual Kick");
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

    dpp::embed embed = dpp::embed().set_color(util::color::RED).set_title("Manual Ban");
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

    dpp::embed embed = dpp::embed().set_color(util::color::GREEN).set_title("Ban Manually Removed");
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
    dpp::embed welcome_embed = dpp::embed().set_color(util::color::GREEN).set_title("New Member")
                                           .set_thumbnail(event.added.get_user()->get_avatar_url())
                                           .set_description(event.added.get_user()->global_name + " has joined Tech Support Central!");
    event.owner->message_create(dpp::message(config["log_channel_ids"]["welcome"], welcome_embed));

    dpp::embed log_embed = dpp::embed().set_color(util::color::GREEN).set_title("Member Joined")
                                       .set_thumbnail(event.added.get_user()->get_avatar_url())
                                       .set_description(event.added.get_user()->username)
                                       .add_field("User ID", event.added.user_id.str(), false)
                                       .add_field("Account Created:", std::format("<t:{}>",
                                                  static_cast<time_t>(event.added.get_user()->get_creation_time())), false);
    dpp::invite invite_used = co_await findinvite(event.owner, event.adding_guild.id, invites);
    if (invite_used.code != "unknown") {
        log_embed.add_field("Invite code", invite_used.code, false);
        if (invite_used.code == config["topgg_invite_code"].get<std::string>()) {
            log_embed.add_field("Invite creator", "top.gg", true);
        } else {
            log_embed.add_field("Invite creator", invite_used.inviter.username, true);
        }
        log_embed.add_field("Invite uses", std::to_string(invite_used.uses), true);
    }
    dpp::confirmation_callback_t role_conf = co_await event.owner->co_guild_member_add_role(event.adding_guild.id, event.added.user_id, config["role_ids"]["og"]);
    if (role_conf.is_error()) {
        log_embed.set_footer(dpp::embed_footer().set_text(std::format("Failed to add <@&{}> role", config["role_ids"]["id"].get<uint64_t>())));
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["member_log"], log_embed));
}

void members::on_leave(const dpp::guild_member_remove_t& event, const nlohmann::json& config) {
    dpp::embed embed = dpp::embed().set_color(util::color::RED).set_title("Member Left")
                                   .set_thumbnail(event.removed.get_avatar_url())
                                   .set_description(event.removed.username + " has left Tech Support Central.")
                                   .set_footer(dpp::embed_footer().set_text(std::string("User ID: ") + event.removed.id.str()));
    event.owner->message_create(dpp::message(config["log_channel_ids"]["member_log"], embed));
}

dpp::task<> members::on_sus_join(const dpp::guild_join_request_delete_t& event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.user_id);
    if (!user_conf.is_error()) {
        dpp::user_identified user = std::get<dpp::user_identified>(user_conf.value);
        dpp::embed embed = dpp::embed().set_color(util::color::RED).set_title("User attempted to join but failed CAPTCHA")
                                       .set_thumbnail(user.get_avatar_url())
                                       .set_description(user.username)
                                       .add_field("User ID", event.user_id.str(), false)
                                       .add_field("Account Created:", std::format("<t:{}>",
                                                  static_cast<time_t>(event.user_id.get_creation_time())), false);
        event.owner->message_create(dpp::message(config["log_channel_ids"]["filter_log"], embed));
    }
}

dpp::task<> members::on_member_edit(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.entry.target_id);
    if (!user_conf.is_error()) {
        dpp::user_identified user = std::get<dpp::user_identified>(user_conf.value);
        for (const dpp::audit_change& change : event.entry.changes) {
            dpp::embed embed;
            dpp::snowflake channel = 0;
            if (change.key == "nick") {
                embed.set_author(dpp::embed_author(user.username, "", user.get_avatar_url()));
                dpp::confirmation_callback_t mod_conf = co_await event.owner->co_user_get_cached(event.entry.user_id);
                if (!mod_conf.is_error()) {
                    dpp::user_identified mod = std::get<dpp::user_identified>(mod_conf.value);
                    embed.set_thumbnail(mod.get_avatar_url());
                    embed.add_field("Named by", mod.global_name, false);
                }
                if (change.old_value.empty()) {
                    embed.set_color(util::color::GREEN).set_title("Nickname Added");
                } else {
                    embed.set_color(util::color::DEFAULT).set_title("Nickname Changed");
                    // Remove quotation marks at beginning and end
                    embed.add_field("Old Nickname", change.old_value.substr(1, change.old_value.size() - 2), true);
                }
                if (change.new_value.empty()) {
                    embed.set_color(util::color::RED).set_title("Nickname Removed");
                } else {
                    // Remove quotation marks at beginning and end
                    embed.add_field("New Nickname", change.new_value.substr(1, change.new_value.size() - 2), true);
                }
                embed.set_footer(dpp::embed_footer().set_text(std::string("User ID: ") + user.id.str()));
                channel = config["log_channel_ids"]["name_changed"].get<dpp::snowflake>();
            } // other attributes can be added in the future
            if (channel != 0) {
                event.owner->message_create(dpp::message(channel, embed));
            }
        }
    }
}

dpp::task<> members::on_roles_change(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.entry.target_id);
    if (!user_conf.is_error()) {
        dpp::user_identified user = std::get<dpp::user_identified>(user_conf.value);
        for (const dpp::audit_change& change : event.entry.changes) {
            std::vector<dpp::snowflake> roles;
            if (!change.new_value.empty()) {
                for (const auto& role : nlohmann::json::parse(change.new_value)) {
                    roles.emplace_back(role["id"].get<std::string>());
                    // Don't log new members getting their OG role on join
                    if (roles.back() == config["role_ids"]["og"].get<dpp::snowflake>()) {
                        co_return;
                    }
                    // New staff member log
                    if (change.key == "$add") {
                        if (roles.back() == config["role_ids"]["moderator"].get<dpp::snowflake>()) {
                            dpp::embed welcome_embed = dpp::embed().set_color(util::color::MODERATOR_ROLE_COLOR).set_title("New Moderator")
                                .set_thumbnail(user.get_avatar_url())
                                .set_description(std::format("Congratulate {} on the promotion from trial mod!", user.get_mention()));
                            event.owner->message_create(dpp::message(config["log_channel_ids"]["new_staff_members"], welcome_embed));
                        }
                        else if (roles.back() == config["role_ids"]["trial_mod"].get<dpp::snowflake>()) {
                            dpp::embed welcome_embed = dpp::embed().set_color(util::color::TRIAL_MOD_ROLE_COLOR).set_title("New Trial Moderator")
                                .set_thumbnail(user.get_avatar_url())
                                .set_description(std::format("Welcome {} to the moderation team!", user.get_mention()));
                            event.owner->message_create(dpp::message(config["log_channel_ids"]["new_staff_members"], welcome_embed));
                        }
                        else if (roles.back() == config["role_ids"]["support_team"].get<dpp::snowflake>()) {
                            dpp::embed welcome_embed = dpp::embed().set_color(util::color::SUPPORT_TEAM_ROLE_COLOR).set_title("New Support Team Member")
                                .set_thumbnail(user.get_avatar_url())
                                .set_description(std::format("Welcome {} to the support team!", user.get_mention()));
                            event.owner->message_create(dpp::message(config["log_channel_ids"]["new_staff_members"], welcome_embed));
                        }
                    }
                }
            }

            dpp::embed embed = dpp::embed().set_author(dpp::embed_author(user.global_name, "", user.get_avatar_url()));
            if (change.key == "$add") {
                embed.set_color(util::color::GREEN).set_title("Role Added");
            } else if (change.key == "$remove") {
                embed.set_color(util::color::RED).set_title("Role Removed");
            }

            std::string description;
            for (dpp::snowflake role : roles) {
                description += "<@&";
                description += role.str();
                description += ">\n";
            }
            embed.set_description(description);

            dpp::confirmation_callback_t mod_conf = co_await event.owner->co_user_get_cached(event.entry.user_id);
            if (!mod_conf.is_error()) {
                dpp::user_identified mod = std::get<dpp::user_identified>(mod_conf.value);
                embed.set_thumbnail(mod.get_avatar_url()).add_field("By", mod.global_name, false);
            }
            embed.set_footer(dpp::embed_footer().set_text(std::string("User ID: ") + user.id.str()));

            event.owner->message_create(dpp::message(config["log_channel_ids"]["role_changed"], embed));
        }
    }
}