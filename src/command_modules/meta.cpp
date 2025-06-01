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
    event.reply(std::format("Pong! {}ms", event.from()->websocket_ping / 1000));
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
    event.edit_original_response(dpp::message(std::format(
    "I am currently running on commit [{}](https://github.com/TechSupportCentral/TSCppBot/commit/{}).", commit_hash, commit_hash)));
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
    dpp::message message = dpp::message(event.command.channel_id, embed);
    try {
        dpp::snowflake ping_role_id = std::get<dpp::snowflake>(event.get_parameter("ping"));
        if (ping_role_id == config["role_ids"]["everyone"].get<dpp::snowflake>()) {
            message.set_content("@everyone");
            message.set_allowed_mentions(true, true, true);
        } else {
            message.set_content(event.command.get_resolved_role(ping_role_id).get_mention());
            message.set_allowed_mentions(true, true);
        }
    } catch (const std::bad_variant_access&) {}
    event.owner->message_create(message);
    event.reply(dpp::message("Announcement sent").set_flags(dpp::m_ephemeral));
}

dpp::task<> meta::dm(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking(true);
    // Get user and message, construct embed, and send DM
    dpp::user user = event.command.get_resolved_user(std::get<dpp::snowflake>(event.get_parameter("user")));
    std::string message = std::get<std::string>(event.get_parameter("message"));
    dpp::embed dm_embed = dpp::embed().set_color(0x00A0A0).set_title("Message from the owners of TSC").set_description(message);
    dpp::confirmation_callback_t confirmation = co_await event.owner->co_direct_message_create(user.id, dpp::message(dm_embed));
    if (confirmation.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::string("Failed to send direct message: ") + confirmation.get_error().human_readable));
        co_return;
    }
    // Let sender know and send a log message
    dpp::embed log_embed = dpp::embed().set_color(dpp::colors::green).set_title("DM Sent")
    .set_thumbnail(user.get_avatar_url()).add_field("Sent to", user.global_name, true)
    .add_field("User ID", std::to_string(user.id), true)
    .add_field("Sent by", event.command.get_issuing_user().global_name, false)
    .add_field("Message:", message, false);
    event.owner->message_create(dpp::message(config["log_channel_ids"]["bot_dm"], log_embed));
    co_await thinking;
    event.edit_original_response(dpp::message("Direct message sent successfully."));
}

void meta::remindme(const dpp::slashcommand_t &event, sqlite3* db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking(true);
    const time_t now = time(nullptr);
    const std::string seconds_str = std::get<std::string>(event.get_parameter("time"));
    const time_t seconds = util::time_string_to_seconds(seconds_str);
    if (seconds == -1) {
        event.edit_original_response(dpp::message(std::format(
        "Time string `{}` is not in the correct format.\n", seconds_str)
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
    sqlite3_exec(db, std::format("INSERT INTO reminders VALUES (NULL, {}, {}, '{}', {});", reminder.start_time, reminder.end_time,
    reminder.user.str(), util::sql_escape_string(reminder.text, true)).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to add reminder to database."));
        return;
    }
    reminder.id = sqlite3_last_insert_rowid(db);

    // Sleep (on another thread) for reminder duration
    util::remind(event.owner, db, reminder);
    // Let log and user know the timer started
    std::string fancytime = util::seconds_to_fancytime(seconds, 4);
    util::log("INFO", std::format("Starting reminder of {} from {}",
    fancytime, event.command.get_issuing_user().username));
    event.edit_original_response(dpp::message(std::format(
    "I will remind you in {} (<t:{}:F>).", fancytime, reminder.end_time)));
}