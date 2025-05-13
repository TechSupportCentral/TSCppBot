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
    event.reply(std::string("Up for ") + util::secondsToFancyTime(uptime, 4));
}

void meta::getCommit(const dpp::slashcommand_t &event) {
    // Run git show in the current directory to get the current commit
    dpp::utility::exec("git", {"show", "-s", "--oneline"},
        [&event](const std::string& output) {
            // Get the first 7 characters (the commit hash) from the command output
            std::string commitHash = output.substr(0, 7);
            event.reply(std::string("I am currently running on commit [") + commitHash + "](https://github.com/TechSupportCentral/TSCppBot/commit/" + commitHash + ").");
        }
    );
}