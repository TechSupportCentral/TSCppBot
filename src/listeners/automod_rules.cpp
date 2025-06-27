/* automod_rules: Handlers for automod rule changes
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
#include "automod_rules.h"
#include "../util.h"

dpp::task<> automod_rules::on_automod_rule_add(const dpp::automod_rule_create_t &event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.created.creator_id);
    dpp::user_identified user;
    if (user_conf.is_error()) {
        user = event.owner->me;
    } else {
        user = std::get<dpp::user_identified>(user_conf.value);
    }

    dpp::embed embed = dpp::embed().set_color(dpp::colors::green).set_thumbnail(user.get_avatar_url())
                                   .set_title(std::string("New Automod Rule: ") + event.created.name)
                                   .add_field("Created by", user.username, false);
    switch (event.created.trigger_type) {
        case dpp::amod_type_keyword_preset:
            embed.add_field("Type", "Message Filter (preset list)", false);

            if (!event.created.trigger_metadata.presets.empty()) {
                std::string presets;
                for (dpp::automod_preset_type preset : event.created.trigger_metadata.presets) {
                    switch (preset) {
                        case dpp::amod_preset_profanity:
                            presets += "Profanity";
                            break;
                        case dpp::amod_preset_sexual_content:
                            presets += "Sexual Content";
                            break;
                        case dpp::amod_preset_slurs:
                            presets += "Hate Speech";
                    }
                    presets += ", ";
                }
                embed.add_field("Lists selected", presets, true);
            }
            if (!event.created.trigger_metadata.allow_list.empty()) {
                std::string exceptions;
                for (const std::string& word : event.created.trigger_metadata.allow_list) {
                    exceptions += word;
                    exceptions += '\n';
                }
                exceptions.pop_back();
                embed.add_field("Exceptions", exceptions, true);
            }

            break;
        case dpp::amod_type_mention_spam:
            embed.add_field("Type", "Spam Ping Detection", false);

            embed.add_field("Ping limit", std::to_string(event.created.trigger_metadata.mention_total_limit), true);
            if (event.created.trigger_metadata.mention_raid_protection_enabled) {
                embed.add_field("Raid Protection Enabled?", "True", true);
            } else {
                embed.add_field("Raid Protection Enabled?", "False", true);
            }

            break;
        case dpp::amod_type_keyword:
            embed.add_field("Type", "Message Filter", false);

            if (!event.created.trigger_metadata.keywords.empty()) {
                std::string triggers;
                for (const std::string& trigger : event.created.trigger_metadata.keywords) {
                    triggers += trigger;
                    triggers += '\n';
                }
                triggers.pop_back();
                embed.add_field("Keywords", triggers, true);
            }

            if (!event.created.trigger_metadata.allow_list.empty()) {
                std::string exceptions;
                for (const std::string& word : event.created.trigger_metadata.allow_list) {
                    exceptions += word;
                    exceptions += '\n';
                }
                exceptions.pop_back();
                embed.add_field("Exceptions", exceptions, true);
            }

            break;
        case dpp::amod_type_spam:
            embed.add_field("Type", "Spam Detection", false);
        default:
            break;
    }

    std::string exempt_channels;
    for (dpp::snowflake channel : event.created.exempt_channels) {
        exempt_channels += std::format("<#{}>\n", channel.str());
    }
    std::string exempt_roles;
    for (dpp::snowflake role : event.created.exempt_roles) {
        exempt_roles += std::format("<@&{}>\n", role.str());
    }
    if (!exempt_channels.empty()) {
        exempt_channels.pop_back();
        embed.add_field("Exempt Channels", exempt_channels, false);
    }
    if (!exempt_roles.empty()) {
        exempt_roles.pop_back();
        embed.add_field("Exempt Roles", exempt_roles, false);
    }

    if (!event.created.actions.empty()) {
        std::string actions;
        for (const dpp::automod_action& action : event.created.actions) {
            switch (action.type) {
                case dpp::amod_action_block_message:
                    if (action.custom_message.empty()) {
                        actions += "Prevent message from being sent";
                    } else {
                        actions += "Block message with warning \"" + action.custom_message + '"';
                    }
                    break;
                case dpp::amod_action_send_alert:
                    actions += "Send log to <#" + action.channel_id.str() + '>';
                    break;
                case dpp::amod_action_timeout:
                    actions += "Mute user for " + util::seconds_to_fancytime(action.duration_seconds, 4);
            }
            actions += '\n';
        }
        actions.pop_back();
        embed.add_field("Action to take", actions, false);
    }

    event.owner->message_create(dpp::message(config["log_channel_ids"]["discord_updates"], embed));
}

dpp::task<> automod_rules::on_automod_rule_remove(const dpp::automod_rule_delete_t &event, const nlohmann::json& config) {
    dpp::confirmation_callback_t user_conf = co_await event.owner->co_user_get_cached(event.deleted.creator_id);
    dpp::user_identified user;
    if (user_conf.is_error()) {
        user = event.owner->me;
    } else {
        user = std::get<dpp::user_identified>(user_conf.value);
    }

    dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_thumbnail(user.get_avatar_url())
                                   .set_title("Automod Rule Removed")
                                   .add_field("Rule Name", event.deleted.name, true)
                                   .add_field("Created by", user.username, true);
    event.owner->message_create(dpp::message(config["log_channel_ids"]["discord_updates"], embed));
}