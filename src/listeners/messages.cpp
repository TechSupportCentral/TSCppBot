/* messages: Handlers for events involving messages
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
#include "messages.h"
#include "../util.h"
#include <dpp/unicode_emoji.h>

void messages::add_message_content_fields(dpp::embed& embed, const dpp::message& message) {
    // Display message content if it exists
    if (!message.content.empty()) {
        embed.add_field("Message:", message.content, false);
    }
    // Display stickers if they exist
    for (const dpp::sticker& sticker : message.stickers) {
        embed.add_field(std::string("Sticker ") + sticker.name, sticker.get_url(), false);
    }
    // Special case for if the message has just one image
    if (message.attachments.size() == 1 && message.attachments[0].content_type.substr(0, 5) == "image") {
        embed.add_field("Image:", message.attachments[0].description, false);
        embed.set_image(message.attachments[0].url);
        // Otherwise display all attachments by URL and description
    } else {
        for (const dpp::attachment& attachment : message.attachments) {
            std::string value;
            if (!attachment.description.empty()) {
                value = attachment.description + '\n';
            }
            value += attachment.url;
            embed.add_field(std::string("File: ") + attachment.filename, value, false);
        }
    }
}

void messages::on_message(const dpp::message_create_t& event, const nlohmann::json& config) {
    util::MESSAGE_CACHE.push(event.msg);
    if (event.msg.author == event.owner->me) {
        return;
    }
    if (event.msg.is_dm()) {
        dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_thumbnail(event.msg.author.get_avatar_url())
                                       .set_title("DM Received").add_field("From", event.msg.author.username, true)
                                       .add_field("User ID", event.msg.author.id.str(), true);
        add_message_content_fields(embed, event.msg);
        // Send embed to log
        event.owner->message_create(dpp::message(config["log_channel_ids"]["bot_dm"], embed));
    // DISBOARD bump confirmation message
    } else if (event.msg.author.id == 760604690010079282  && event.msg.embeds.size() == 1) {
        if (event.msg.embeds[0].description.find(dpp::unicode_emoji::thumbsup) != std::string::npos) {
            event.owner->message_create(dpp::message(event.msg.channel_id, dpp::embed()
                .set_color(0x00A0A0).set_title("Thank you for bumping the server!")
                .set_description("Vote for Tech Support Central on top.gg at https://top.gg/servers/824042976371277884")));
            if (!util::BUMP_TIMER_RUNNING) {
                util::BUMP_TIMER_RUNNING = true;
                util::handle_bump(event.owner, config, event.msg.channel_id, 7200);
            }
        }
    } else if (event.msg.content.find("need help") != std::string::npos) {
        bool msg_in_public_nonsupport_channel = false;
        for (const auto& channel : config["public_channel_ids"]) {
            if (event.msg.channel_id == channel.get<dpp::snowflake>()) {
                msg_in_public_nonsupport_channel = true;
                break;
            }
        }
        if (msg_in_public_nonsupport_channel) {
            event.reply(dpp::message(std::format(
                "If you're looking for help, please go to a support channel like <#{}> and ping the <@&{}>.",
                config["support_channel_ids"]["general_support"].get<uint64_t>(), config["role_ids"]["support_team"].get<uint64_t>()
            )).set_allowed_mentions(false));
        }
    } else if (event.msg.content.find(" virus") != std::string::npos) {
        event.reply("We suggest that you check for viruses and suspicious processes with Malwarebytes: https://malwarebytes.com/mwb-download");
    }
}

dpp::task<> messages::on_message_deleted(const dpp::message_delete_t& event, const nlohmann::json& config) {
    dpp::confirmation_callback_t msg_conf = co_await util::get_message_cached(event.owner, event.id, event.channel_id);
    dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_title("Message Deleted")
                                   .add_field("In channel", std::format("<#{}>", event.channel_id.str()), false);
    if (msg_conf.is_error()) {
        embed.add_field("Sent on", std::format("<t:{}>", static_cast<time_t>(event.id.get_creation_time())), false);
        embed.set_footer(dpp::embed_footer().set_text("Unable to fetch further message information (message not in cache)"));
    } else {
        dpp::message message = std::get<dpp::message>(msg_conf.value);
        // Bot and owners are exempt from log
        if (message.author.id == event.owner->me.id) {
            co_return;
        }
        dpp::confirmation_callback_t member_conf = co_await event.owner->co_guild_get_member(config["guild_id"], message.author.id);
        if (!member_conf.is_error()) {
            std::vector<dpp::snowflake> roles = std::get<dpp::guild_member>(member_conf.value).get_roles();
            if (std::ranges::find(roles, config["role_ids"]["owner"].get<dpp::snowflake>()) != roles.end()) {
                co_return;
            }
        }

        embed.set_thumbnail(message.author.get_avatar_url());
        embed.add_field("Sent by", message.author.global_name, true)
             .add_field("User ID", message.author.id.str(), true);
        add_message_content_fields(embed, message);
    }
    // Send embed to log
    event.owner->message_create(dpp::message(config["log_channel_ids"]["message_deleted"], embed));
}

dpp::task<> messages::on_message_edited(const dpp::message_update_t& event, const nlohmann::json& config) {
    // Bot and owners are exempt from log
    if (event.msg.author.id == event.owner->me.id) {
        co_return;
    }
    dpp::confirmation_callback_t member_conf = co_await event.owner->co_guild_get_member(config["guild_id"], event.msg.author.id);
    if (!member_conf.is_error()) {
        std::vector<dpp::snowflake> roles = std::get<dpp::guild_member>(member_conf.value).get_roles();
        if (std::ranges::find(roles, config["role_ids"]["owner"].get<dpp::snowflake>()) != roles.end()) {
            co_return;
        }
    }

    dpp::embed embed = dpp::embed().set_color(0x00A0A0).set_title("Message Edited").set_thumbnail(event.msg.author.get_avatar_url())
                                   .add_field("In channel", std::format("<#{}>\n[Jump to Message]({})",
                                              event.msg.channel_id.str(), event.msg.get_url()), false)
                                   .add_field("Sent by", event.msg.author.global_name, true)
                                   .add_field("User ID", event.msg.author.id.str(), true);

    // Try to get old message from cache, searching backwards (latest to earliest messages)
    auto it = util::MESSAGE_CACHE.end();
    while (it-- != util::MESSAGE_CACHE.begin()) {
        if (it->id == event.msg.id) {
            break;
        }
    }
    // If message was not in cache, we can't figure out what the edits were
    if (it == util::MESSAGE_CACHE.begin() - 1) {
        add_message_content_fields(embed, event.msg);
        embed.set_footer(dpp::embed_footer().set_text("Could not determine what was edited (message not in cache)"));
    } else {
        // Get original message from cache
        const dpp::message& old_msg = *it;

        // If no content changed (sometimes happens when link previews are deleted), do not print anything
        if (old_msg.content == event.msg.content) {
            co_return;
        }

        add_message_content_fields(embed, event.msg);
        embed.fields.emplace(embed.fields.begin() + 2, "Original Message:", old_msg.content, false);
        embed.fields[3].name = "Edited Message:";

        // Update message in cache to include edits
        *it = event.msg;
    }
    // Send embed to log
    event.owner->message_create(dpp::message(config["log_channel_ids"]["message_edited"], embed));
}