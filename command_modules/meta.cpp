/* meta: Commands related to the bot itself
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
#include "meta.h"
#include "../util.h"

void meta::ping(const dpp::slashcommand_t &event) {
    // TODO: Figure out why this is returning 0 ms
    event.reply(std::string("Pong! ") + std::to_string(event.from()->websocket_ping / 1000) + "ms");
}

void meta::uptime(const dpp::slashcommand_t &event) {
    uint64_t uptime = event.owner->uptime().to_secs();
    event.reply(std::string("Up for ") + util::seconds_to_fancytime(uptime, 4));
}

void meta::get_commit(const dpp::slashcommand_t &event) {
    event.thinking(true); // Show "thinking" status in case git takes more than 500ms to run
    std::string commit_hash;
    // Run git show in the current directory to get the current commit
    dpp::utility::exec("git", {"show", "-s", "--oneline"},
        [&commit_hash](const std::string& output) {
            // Get the first 7 characters (the commit hash) from the command output
            commit_hash = output.substr(0, 7);
        }
    );
    /* git is run on a separate thread that the event cannot be passed to.
       Wait for the command to finish before replying to the event. */
    while (commit_hash.empty()) {}
    event.edit_original_response(dpp::message(std::string("I am currently running on commit [")
                                 + commit_hash + "](https://github.com/TechSupportCentral/TSCppBot/commit/"
                                 + commit_hash + ")."));
}

void meta::send_message(const dpp::slashcommand_t &event) {
    event.reply(std::get<std::string>(event.get_parameter("message")));
}

void meta::announce(const dpp::slashcommand_t &event) {
    dpp::embed embed = dpp::embed().set_color(0x00A0A0).
    set_description(std::get<std::string>(event.get_parameter("message")));
    try {
        std::string title = std::get<std::string>(event.get_parameter("title"));
        embed.set_title(title);
    } catch (const std::bad_variant_access& e) {
        embed.set_title("Announcement");
    }
    try {
        dpp::snowflake ping_role_id = std::get<dpp::snowflake>(event.get_parameter("ping"));
        dpp::message message = dpp::message(event.command.channel_id, embed);
        message.set_content(std::string("<@&") + std::to_string(ping_role_id) + '>');
        event.reply(message);
    } catch (const std::bad_variant_access& e) {
        event.reply(dpp::message(event.command.channel_id, embed));
    }
}