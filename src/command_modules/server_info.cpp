/* server_info: Commands to get info about the server
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
#include "server_info.h"
#include "../util.h"

void server_info::rules(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    dpp::embed embed = dpp::embed().set_color(0x00A0A0).set_title("Server Rules");

    for (size_t i = 0; i < config["rules"].size(); i++) {
        embed.add_field(std::format("Rule {}:", i + 1), config["rules"][i].get<std::string>());
    }

    event.reply(dpp::message(event.command.channel_id, embed));
}

void server_info::rule(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    int64_t rule = std::get<int64_t>(event.get_parameter("rule"));
    if (rule < 1 || rule > config["rules"].size()) {
        event.reply(dpp::message(std::format("There is no rule {}. There are only {} rules.", rule, config["rules"].size())).set_flags(dpp::m_ephemeral));
        return;
    }

    dpp::embed embed = dpp::embed().set_color(0x00A0A0)
                                   .set_title(std::string("Rule ") + std::to_string(rule))
                                   .set_description(config["rules"][rule - 1].get<std::string>());
    event.reply(dpp::message(event.command.channel_id, embed));
}

void server_info::suggest(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    std::string suggestion = std::get<std::string>(event.get_parameter("suggestion"));
    dpp::embed_author author(event.command.member.get_nickname(), "", event.command.member.get_avatar_url());
    dpp::embed embed = dpp::embed().set_color(dpp::colors::light_gray).set_author(author)
    .set_description(std::string("**Suggestion:** ") + suggestion).add_field("Status: Pending", "");
    event.owner->message_create(dpp::message(config["log_channel_ids"]["suggestion_list"], embed));
    event.reply("Thanks for your suggestion! You will be notified when the owners respond.");
}

dpp::task<> server_info::suggestion_response(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking();
    dpp::snowflake message_id = std::stoull(std::get<std::string>(event.get_parameter("suggestion_id")));
    dpp::confirmation_callback_t suggestion = co_await event.owner->co_message_get(message_id, config["log_channel_ids"]["suggestion_list"]);
    if (suggestion.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message("Suggestion message not found."));
        co_return;
    }
    dpp::message message = std::get<dpp::message>(suggestion.value);
    if (message.embeds.empty()) {
        co_await thinking;
        event.edit_original_response(dpp::message("Suggestion message not in correct format."));
        co_return;
    }
    if (message.embeds[0].fields.empty()) {
        co_await thinking;
        event.edit_original_response(dpp::message("Suggestion message not in correct format."));
        co_return;
    }

    if (std::get<bool>(event.get_parameter("accept"))) {
        message.embeds[0].set_color(dpp::colors::green);
        message.embeds[0].fields[0].name = "Status: Accepted";
    } else {
        message.embeds[0].set_color(dpp::colors::red);
        message.embeds[0].fields[0].name = "Status: Declined";
    }
    message.embeds[0].fields[0].value = std::get<std::string>(event.get_parameter("response"));

    event.owner->message_edit(message);
    co_await thinking;
    event.edit_original_response(dpp::message("Successfully responded to suggestion."));
}