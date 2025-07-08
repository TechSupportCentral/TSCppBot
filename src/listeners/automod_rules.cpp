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

void automod_rules::add_rule_description_fields(dpp::embed& embed, const dpp::automod_rule& rule) {
    switch (rule.trigger_type) {
        case dpp::amod_type_keyword_preset:
            embed.add_field("Type", "Message Filter (preset list)", false);

            if (!rule.trigger_metadata.presets.empty()) {
                std::string presets;
                for (dpp::automod_preset_type preset : rule.trigger_metadata.presets) {
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
            if (!rule.trigger_metadata.allow_list.empty()) {
                std::string exceptions;
                for (const std::string& word : rule.trigger_metadata.allow_list) {
                    exceptions += word;
                    exceptions += '\n';
                }
                exceptions.pop_back();
                embed.add_field("Exceptions", exceptions, true);
            }

            break;
        case dpp::amod_type_mention_spam:
            embed.add_field("Type", "Spam Ping Detection", false);

            embed.add_field("Ping limit", std::to_string(rule.trigger_metadata.mention_total_limit), true);
            if (rule.trigger_metadata.mention_raid_protection_enabled) {
                embed.add_field("Raid Protection Enabled?", "True", true);
            } else {
                embed.add_field("Raid Protection Enabled?", "False", true);
            }

            break;
        case dpp::amod_type_keyword:
            embed.add_field("Type", "Message Filter", false);

            if (!rule.trigger_metadata.keywords.empty()) {
                std::string triggers;
                for (const std::string& trigger : rule.trigger_metadata.keywords) {
                    triggers += trigger;
                    triggers += '\n';
                }
                triggers.pop_back();
                embed.add_field("Keywords", triggers, true);
            }

            if (!rule.trigger_metadata.allow_list.empty()) {
                std::string exceptions;
                for (const std::string& word : rule.trigger_metadata.allow_list) {
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
    for (dpp::snowflake channel : rule.exempt_channels) {
        exempt_channels += std::format("<#{}>\n", channel.str());
    }
    std::string exempt_roles;
    for (dpp::snowflake role : rule.exempt_roles) {
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

    if (!rule.actions.empty()) {
        std::string actions;
        for (const dpp::automod_action& action : rule.actions) {
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
}

dpp::task<> automod_rules::on_automod_rule_add(const dpp::automod_rule_create_t &event, const nlohmann::json& config, std::vector<dpp::automod_rule>& automod_rules) {
    automod_rules.push_back(event.created);
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
    add_rule_description_fields(embed, event.created);
    event.owner->message_create(dpp::message(config["log_channel_ids"]["discord_updates"], embed));
}

void automod_rules::on_automod_rule_remove(const dpp::automod_rule_delete_t &event, const nlohmann::json& config, std::vector<dpp::automod_rule>& automod_rules) {
    dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_title("Automod Rule Deleted")
                                   .add_field("Rule Name", event.deleted.name, true);
    add_rule_description_fields(embed, event.deleted);
    event.owner->message_create(dpp::message(config["log_channel_ids"]["discord_updates"], embed));
    // Delete rule from cache if possible
    if (const auto it = std::ranges::find(automod_rules, event.deleted); it != automod_rules.end()) {
        automod_rules.erase(it);
    }
}

void automod_rules::on_automod_rule_edit(const dpp::automod_rule_update_t &event, const nlohmann::json& config, std::vector<dpp::automod_rule>& automod_rules) {
    dpp::embed embed = dpp::embed().set_color(0x00A0A0).set_title("Automod Rule Edited")
                                   .add_field("Rule Name", event.updated.name, false);
    // Find rule in cache
    auto old_rule = automod_rules.begin();
    while (old_rule != automod_rules.end() && old_rule->id != event.updated.id) ++old_rule;
    // If rule cannot be found, add edited version to cache and exit
    if (old_rule == automod_rules.end()) {
        automod_rules.push_back(event.updated);
        embed.set_footer(dpp::embed_footer().set_text("Limited info available (old rule not in cache)"));
        event.owner->message_create(dpp::message(config["log_channel_ids"]["discord_updates"], embed));
        return;
    }

    if (event.updated.trigger_type != old_rule->trigger_type) {
        const std::unordered_map<dpp::automod_trigger_type, const char*> TRIGGER_TYPE_DESCRIPTIONS = {
            {dpp::amod_type_keyword_preset, "Message Filter (preset list)"},
            {dpp::amod_type_mention_spam, "Spam Ping Detection"},
            {dpp::amod_type_keyword, "Message Filter"},
            {dpp::amod_type_spam, "Spam Detection"}
        };
        embed.add_field("Old Type", TRIGGER_TYPE_DESCRIPTIONS.at(old_rule->trigger_type), true);
        embed.add_field("New Type", TRIGGER_TYPE_DESCRIPTIONS.at(event.updated.trigger_type), true);
    }
    switch (event.updated.trigger_type) {
        case dpp::amod_type_keyword_preset:
            if (event.updated.trigger_metadata.presets != old_rule->trigger_metadata.presets) {
                std::string presets_added;
                for (dpp::automod_preset_type preset : event.updated.trigger_metadata.presets) {
                    if (std::ranges::find(old_rule->trigger_metadata.presets, preset) == old_rule->trigger_metadata.presets.end()) {
                        switch (preset) {
                            case dpp::amod_preset_profanity:
                                presets_added += "Profanity";
                                break;
                            case dpp::amod_preset_sexual_content:
                                presets_added += "Sexual Content";
                                break;
                            case dpp::amod_preset_slurs:
                                presets_added += "Hate Speech";
                        }
                        presets_added += ", ";
                    }
                }
                if (!presets_added.empty()) {
                    embed.add_field("New selected lists", presets_added, true);
                }

                std::string presets_removed;
                for (dpp::automod_preset_type preset : old_rule->trigger_metadata.presets) {
                    if (std::ranges::find(event.updated.trigger_metadata.presets, preset) == event.updated.trigger_metadata.presets.end()) {
                        switch (preset) {
                            case dpp::amod_preset_profanity:
                                presets_removed += "Profanity";
                                break;
                            case dpp::amod_preset_sexual_content:
                                presets_removed += "Sexual Content";
                                break;
                            case dpp::amod_preset_slurs:
                                presets_removed += "Hate Speech";
                        }
                        presets_removed += ", ";
                    }
                }
                if (!presets_removed.empty()) {
                    embed.add_field("Lists de-selected", presets_removed, true);
                }
            }
            if (event.updated.trigger_metadata.allow_list != old_rule->trigger_metadata.allow_list) {
                std::string exceptions_added;
                for (const std::string& word : event.updated.trigger_metadata.allow_list) {
                    if (std::ranges::find(old_rule->trigger_metadata.allow_list, word) == old_rule->trigger_metadata.allow_list.end()) {
                        exceptions_added += word;
                        exceptions_added += '\n';
                    }
                }
                if (!exceptions_added.empty()) {
                    exceptions_added.pop_back();
                    embed.add_field("Exceptions Added", exceptions_added, true);
                }

                std::string exceptions_removed;
                for (const std::string& word : old_rule->trigger_metadata.allow_list) {
                    if (std::ranges::find(event.updated.trigger_metadata.allow_list, word) == event.updated.trigger_metadata.allow_list.end()) {
                        exceptions_removed += word;
                        exceptions_removed += '\n';
                    }
                }
                if (!exceptions_removed.empty()) {
                    exceptions_removed.pop_back();
                    embed.add_field("Exceptions Removed", exceptions_removed, true);
                }
            }
            break;
        case dpp::amod_type_mention_spam:
            if (event.updated.trigger_metadata.mention_total_limit != old_rule->trigger_metadata.mention_total_limit) {
                embed.add_field("Old ping limit", std::to_string(old_rule->trigger_metadata.mention_total_limit), true);
                embed.add_field("New ping limit", std::to_string(event.updated.trigger_metadata.mention_total_limit), true);
            }
            if (event.updated.trigger_metadata.mention_raid_protection_enabled != old_rule->trigger_metadata.mention_raid_protection_enabled) {
                if (event.updated.trigger_metadata.mention_raid_protection_enabled) {
                    embed.add_field("Raid Protection", "Enabled", true);
                } else {
                    embed.add_field("Raid Protection", "Disabled", true);
                }
            }
            break;
        case dpp::amod_type_keyword:
            if (event.updated.trigger_metadata.keywords != old_rule->trigger_metadata.keywords) {
                std::string triggers_added;
                for (const std::string& trigger : event.updated.trigger_metadata.keywords) {
                    if (std::ranges::find(old_rule->trigger_metadata.keywords, trigger) == old_rule->trigger_metadata.keywords.end()) {
                        triggers_added += trigger;
                        triggers_added += '\n';
                    }
                }
                if (!triggers_added.empty()) {
                    triggers_added.pop_back();
                    embed.add_field("Keywords Added", triggers_added, true);
                }

                std::string triggers_removed;
                for (const std::string& trigger : old_rule->trigger_metadata.keywords) {
                    if (std::ranges::find(event.updated.trigger_metadata.keywords, trigger) == event.updated.trigger_metadata.keywords.end()) {
                        triggers_removed += trigger;
                        triggers_removed += '\n';
                    }
                }
                if (!triggers_removed.empty()) {
                    triggers_removed.pop_back();
                    embed.add_field("Keywords Removed", triggers_removed, true);
                }
            }

            if (event.updated.trigger_metadata.allow_list != old_rule->trigger_metadata.allow_list) {
                std::string exceptions_added;
                for (const std::string& word : event.updated.trigger_metadata.allow_list) {
                    if (std::ranges::find(old_rule->trigger_metadata.allow_list, word) == old_rule->trigger_metadata.allow_list.end()) {
                        exceptions_added += word;
                        exceptions_added += '\n';
                    }
                }
                if (!exceptions_added.empty()) {
                    exceptions_added.pop_back();
                    embed.add_field("Exceptions Added", exceptions_added, true);
                }

                std::string exceptions_removed;
                for (const std::string& word : old_rule->trigger_metadata.allow_list) {
                    if (std::ranges::find(event.updated.trigger_metadata.allow_list, word) == event.updated.trigger_metadata.allow_list.end()) {
                        exceptions_removed += word;
                        exceptions_removed += '\n';
                    }
                }
                if (!exceptions_removed.empty()) {
                    exceptions_removed.pop_back();
                    embed.add_field("Exceptions Removed", exceptions_removed, true);
                }
            }
        default:
            break;
    }

    if (event.updated.exempt_channels != old_rule->exempt_channels) {
        std::string exempt_channels_added;
        for (dpp::snowflake channel : event.updated.exempt_channels) {
            if (std::ranges::find(old_rule->exempt_channels, channel) == old_rule->exempt_channels.end()) {
                exempt_channels_added += std::format("<#{}>\n", channel.str());
            }
        }
        if (!exempt_channels_added.empty()) {
            exempt_channels_added.pop_back();
            embed.add_field("Exempt Channels Added", exempt_channels_added, false);
        }

        std::string exempt_channels_removed;
        for (dpp::snowflake channel : old_rule->exempt_channels) {
            if (std::ranges::find(event.updated.exempt_channels, channel) == event.updated.exempt_channels.end()) {
                exempt_channels_removed += std::format("<#{}>\n", channel.str());
            }
        }
        if (!exempt_channels_removed.empty()) {
            exempt_channels_removed.pop_back();
            embed.add_field("Exempt Channels Removed", exempt_channels_removed, false);
        }
    }
    if (event.updated.exempt_roles != old_rule->exempt_roles) {
        std::string exempt_roles_added;
        for (dpp::snowflake role : event.updated.exempt_roles) {
            if (std::ranges::find(old_rule->exempt_roles, role) == old_rule->exempt_roles.end()) {
                exempt_roles_added += std::format("<@&{}>\n", role.str());
            }
        }
        if (!exempt_roles_added.empty()) {
            exempt_roles_added.pop_back();
            embed.add_field("Exempt Roles Added", exempt_roles_added, false);
        }

        std::string exempt_roles_removed;
        for (dpp::snowflake role : old_rule->exempt_roles) {
            if (std::ranges::find(event.updated.exempt_roles, role) == event.updated.exempt_roles.end()) {
                exempt_roles_removed += std::format("<@&{}>\n", role.str());
            }
        }
        if (!exempt_roles_removed.empty()) {
            exempt_roles_removed.pop_back();
            embed.add_field("Exempt Roles Removed", exempt_roles_removed, false);
        }
    }

    std::string old_actions;
    for (const dpp::automod_action& action : old_rule->actions) {
        switch (action.type) {
            case dpp::amod_action_block_message:
                if (action.custom_message.empty()) {
                    old_actions += "Prevent message from being sent";
                } else {
                    old_actions += "Block message with warning \"" + action.custom_message + '"';
                }
                break;
            case dpp::amod_action_send_alert:
                old_actions += "Send log to <#" + action.channel_id.str() + '>';
                break;
            case dpp::amod_action_timeout:
                old_actions += "Mute user for " + util::seconds_to_fancytime(action.duration_seconds, 4);
        }
        old_actions += '\n';
    }

    std::string new_actions;
    for (const dpp::automod_action& action : event.updated.actions) {
        switch (action.type) {
            case dpp::amod_action_block_message:
                if (action.custom_message.empty()) {
                    new_actions += "Prevent message from being sent";
                } else {
                    new_actions += "Block message with warning \"" + action.custom_message + '"';
                }
                break;
            case dpp::amod_action_send_alert:
                new_actions += "Send log to <#" + action.channel_id.str() + '>';
                break;
            case dpp::amod_action_timeout:
                new_actions += "Mute user for " + util::seconds_to_fancytime(action.duration_seconds, 4);
        }
        new_actions += '\n';
    }
    if (new_actions != old_actions) {
        old_actions.pop_back();
        new_actions.pop_back();
        embed.add_field("Old list of actions to take", old_actions, true);
        embed.add_field("New list of actions to take", new_actions, true);
    }

    if (event.updated.enabled != old_rule->enabled) {
        if (event.updated.enabled) {
            embed.set_color(dpp::colors::green).set_title("Automod Rule Enabled");
        } else {
            embed.set_color(dpp::colors::red).set_title("Automod Rule Disabled");
        }
    }

    // Send log and update rule in cache
    event.owner->message_create(dpp::message(config["log_channel_ids"]["discord_updates"], embed));
    *old_rule = event.updated;
}