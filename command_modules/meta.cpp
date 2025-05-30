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
    // Show "thinking" status in case git takes more than 500ms to run
    event.thinking(true);
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
    event.owner->message_create(dpp::message(event.command.channel_id, std::get<std::string>(event.get_parameter("message"))));
    event.reply(dpp::message("Message sent").set_flags(dpp::m_ephemeral));
}

void meta::announce(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    dpp::embed embed = dpp::embed().set_color(0x00A0A0).
    set_description(std::get<std::string>(event.get_parameter("message")));
    try {
        std::string title = std::get<std::string>(event.get_parameter("title"));
        embed.set_title(title);
    } catch (const std::bad_variant_access&) {
        embed.set_title("Announcement");
    }
    try {
        dpp::snowflake ping_role_id = std::get<dpp::snowflake>(event.get_parameter("ping"));
        dpp::message message = dpp::message(event.command.channel_id, embed);
        if (ping_role_id == static_cast<dpp::snowflake>(config["everyone_ping_role_id"])) {
            message.set_content("@everyone");
            message.set_allowed_mentions(true, true, true);
        } else {
            message.set_content(std::string("<@&") + std::to_string(ping_role_id) + '>');
            message.set_allowed_mentions(true, true);
        }
        event.owner->message_create(message);
    } catch (const std::bad_variant_access&) {
        event.owner->message_create(dpp::message(event.command.channel_id, embed));
    }
    event.reply(dpp::message("Announcement sent").set_flags(dpp::m_ephemeral));
}

void meta::remindme(const dpp::slashcommand_t &event, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking(true);
    const time_t now = time(nullptr);
    const time_t seconds = util::short_time_string_to_seconds(std::get<std::string>(event.get_parameter("time")));
    if (seconds == -1) {
        event.edit_original_response(dpp::message(std::string("`")
        + std::get<std::string>(event.get_parameter("time")) + "` is not in the correct format.\n"
        + "It should look something like `1d2h3m4s` (1 day, 2 hours, 3 minutes, and 4 seconds)."));
        return;
    }

    // Build reminder object from command context
    util::reminder reminder;
    reminder.start_time = now;
    reminder.end_time = now + seconds;
    reminder.user = event.command.get_issuing_user().id;
    try {
        reminder.text = std::get<std::string>(event.get_parameter("reminder"));
    } catch (const std::bad_variant_access&) {
        reminder.text = "No description provided.";
    }
    // Add reminder to DB
    char* error_message;
    sqlite3_exec(db, (std::string("INSERT INTO reminders VALUES (NULL, ") + std::to_string(reminder.start_time)
    + ", " + std::to_string(reminder.end_time) + ", '" + std::to_string(reminder.user) + "', "
    + util::sql_escape_string(reminder.text, true) + ");").c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << error_message << std::endl;
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to add reminder to database."));
        return;
    }
    reminder.id = sqlite3_last_insert_rowid(db);

    // Sleep (on another thread) for reminder duration
    util::remind(event.owner, db, reminder);
    // Let log and user know the timer started
    std::string fancytime = util::seconds_to_fancytime(seconds, 4);
    std::cout << "[" << dpp::utility::current_date_time() << "] INFO: Starting reminder of "
    << fancytime << " from " << event.command.get_issuing_user().username << std::endl;
    event.edit_original_response(dpp::message(std::string("I will remind you in ")
    + fancytime + " (<t:" + std::to_string(reminder.end_time) + ":F>)."));
}