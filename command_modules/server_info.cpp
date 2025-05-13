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
    dpp::embed embed = dpp::embed().set_color(dpp::colors::green).set_title("Server Rules");

    for (int i = 0; i < config["rules"].size(); i++) {
        embed.add_field(std::string("Rule ") + std::to_string(i + 1) + ':', config["rules"][i].get<std::string>());
    }

    event.reply(dpp::message(event.command.channel_id, embed));
}

void server_info::rule(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    std::cout << std::get<long long>(event.get_parameter("rule")) << std::endl;
    long long rule = std::get<long long>(event.get_parameter("rule"));
    if (rule < 1 || rule > config["rules"].size()) {
        event.reply(dpp::message(std::string("There is no rule ")
                                 + std::to_string(rule) + ". There are only "
                                 + std::to_string(config["rules"].size())
                                 + " rules.").set_flags(dpp::m_ephemeral));
        return;
    }

    dpp::embed embed = dpp::embed().set_color(0x00A0A0)
                                   .set_title(std::string("Rule ") + std::to_string(rule) + ':')
                                   .set_description(config["rules"][rule - 1].get<std::string>());
    event.reply(dpp::message(event.command.channel_id, embed));
}