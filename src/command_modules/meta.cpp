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

dpp::task<> meta::send_message(const dpp::slashcommand_t &event) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking(true);
    // Replace escaped newline "\n" with actual newline character
    std::string message = std::get<std::string>(event.get_parameter("message"));
    util::escape_newlines(message);
    // Send message
    dpp::confirmation_callback_t msg_conf = co_await event.owner->co_message_create(dpp::message(event.command.channel_id, message));
    co_await thinking;
    if (msg_conf.is_error()) {
        event.edit_original_response(dpp::message("Message failed to send."));
    } else {
        event.edit_original_response(dpp::message("Message sent"));
    }
}

dpp::task<> meta::announce(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking(true);
    // Replace escaped newline "\\n" with actual newline character
    std::string message = std::get<std::string>(event.get_parameter("message"));
    util::escape_newlines(message);

    dpp::embed embed = dpp::embed().set_color(util::color::DEFAULT).
    set_description(message);
    try {
        std::string title = std::get<std::string>(event.get_parameter("title"));
        embed.set_title(title);
    } catch (const std::bad_variant_access&) {
        embed.set_title("Announcement");
    }
    dpp::message msg = dpp::message(event.command.channel_id, embed);
    try {
        dpp::snowflake ping_role_id = std::get<dpp::snowflake>(event.get_parameter("ping"));
        if (ping_role_id == config["role_ids"]["everyone"].get<dpp::snowflake>()) {
            msg.set_content("@everyone");
            msg.set_allowed_mentions(true, true, true);
        } else {
            msg.set_content(event.command.get_resolved_role(ping_role_id).get_mention());
            msg.set_allowed_mentions(true, true);
        }
    } catch (const std::bad_variant_access&) {}
    dpp::confirmation_callback_t msg_conf = co_await event.owner->co_message_create(msg);
    co_await thinking;
    if (msg_conf.is_error()) {
        event.edit_original_response(dpp::message("Announcement failed to send."));
    } else {
        event.edit_original_response(dpp::message("Announcement sent"));
    }
}

dpp::task<> meta::dm(const dpp::slashcommand_t &event, const nlohmann::json &config) {
    // Send "thinking" response to allow time for Discord API
    dpp::async thinking = event.co_thinking(true);
    // Get user and message, construct embed, and send DM
    dpp::user user = event.command.get_resolved_user(std::get<dpp::snowflake>(event.get_parameter("user")));
    std::string message = std::get<std::string>(event.get_parameter("message"));
    // Replace escaped newline "\\n" with actual newline character
    util::escape_newlines(message);

    dpp::embed dm_embed = dpp::embed().set_color(util::color::DEFAULT).set_title("Message from the owners of TSC").set_description(message);
    dpp::confirmation_callback_t confirmation = co_await event.owner->co_direct_message_create(user.id, dpp::message(dm_embed));
    if (confirmation.is_error()) {
        co_await thinking;
        event.edit_original_response(dpp::message(std::string("Failed to send direct message: ") + confirmation.get_error().human_readable));
        co_return;
    }
    // Let sender know and send a log message
    dpp::embed log_embed = dpp::embed().set_color(util::color::GREEN).set_title("DM Sent")
    .set_thumbnail(user.get_avatar_url()).add_field("Sent to", user.username, true)
    .add_field("User ID", std::to_string(user.id), true)
    .add_field("Sent by", event.command.member.get_nickname(), false)
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
        // Replace escaped newline "\\n" with actual newline character
        util::escape_newlines(reminder.text);
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

void meta::set_bump_timer(const dpp::slashcommand_t &event, const nlohmann::json &config, bool& bump_timer_running) {
    if (bump_timer_running) {
        event.reply(dpp::message("The bump timer is already running.").set_flags(dpp::m_ephemeral));
        return;
    }
    // Get timer length
    time_t minutes;
    try {
        minutes = std::get<int64_t>(event.get_parameter("minutes"));
    } catch (const std::bad_variant_access&) {
        minutes = 120;
    }
    // Start timer and send confirmation
    bump_timer_running = true;
    util::handle_bump(event.owner, config, event.command.channel_id, minutes * 60LL, bump_timer_running);
    event.reply(dpp::message(std::format("Timer set for {} minutes.", minutes)).set_flags(dpp::m_ephemeral));
}