/* message_create: Handler for new messages posted to the server or DMs
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
#include "message_create.h"
#include "../util.h"
#include <dpp/unicode_emoji.h>

void message_create::on_message(const dpp::message_create_t& event, const nlohmann::json& config) {
    if (event.msg.author == event.owner->me) {
        return;
    }
    if (event.msg.is_dm()) {
        dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_thumbnail(event.msg.author.get_avatar_url())
                                       .set_title("DM Received").add_field("From", event.msg.author.username, true)
                                       .add_field("User ID", event.msg.author.id.str(), true);
        // Display message content if it exists
        if (!event.msg.content.empty()) {
            embed.add_field("Message:", event.msg.content, false);
        }
        // Display stickers if they exist
        for (const dpp::sticker& sticker : event.msg.stickers) {
            embed.add_field(std::string("Sticker ") + sticker.name, sticker.get_url(), false);
        }
        // Special case for if the message has just one image
        if (event.msg.attachments.size() == 1 && event.msg.attachments[0].content_type.substr(0, 5) == "image") {
            embed.add_field("Image:", event.msg.attachments[0].description, false);
            embed.set_image(event.msg.attachments[0].url);
        // Otherwise display all attachments by URL and description
        } else {
            for (const dpp::attachment& attachment : event.msg.attachments) {
                std::string value;
                if (!attachment.description.empty()) {
                    value = attachment.description + '\n';
                }
                value += attachment.url;
                embed.add_field(std::string("File: ") + attachment.filename, value, false);
            }
        }
        // Send embed to log
        event.owner->message_create(dpp::message(config["log_channel_ids"]["bot_dm"], embed));
    // DISBOARD bump confirmation message
    } else if (event.msg.author.id == 760604690010079282  && event.msg.embeds.size() == 1) {
        if (event.msg.embeds[0].description.find(dpp::unicode_emoji::thumbsup) != std::string::npos) {
            event.owner->message_create(dpp::message(event.msg.channel_id, dpp::embed()
                .set_color(0x00A0A0).set_title("Thank you for bumping the server!")
                .set_description("Vote for Tech Support Central on top.gg at https://top.gg/servers/824042976371277884")));
            if (!util::BUMP_TIMER_ENABLED) {
                util::BUMP_TIMER_ENABLED = true;
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
        event.reply("We suggest you to check for viruses and suspicious processes with Malwarebytes: https://malwarebytes.com/mwb-download");
    }
}