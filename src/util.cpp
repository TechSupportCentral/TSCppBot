/* util: Various helper functions
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
#include "util.h"
#include <map>
#include <vector>

void util::log(const std::string_view severity, const std::string_view message) {
    std::cout << "[" << dpp::utility::current_date_time() << "] " << severity << ": " << message << std::endl;
    LOG_FILE << "[" << dpp::utility::current_date_time() << "] " << severity << ": " << message << std::endl;
}

std::string util::seconds_to_fancytime(unsigned long long int seconds, const unsigned short int granularity) {
    const std::map<const char*, unsigned int> INTERVALS = {
        {" days", 86400},
        {" hours", 3600},
        {" minutes", 60},
        {" seconds", 1}
    };
    std::vector<std::string> time_units;

    // Divide number of seconds into days, hours, minutes, and seconds and create strings
    for (const auto& [UNIT_NAME, UNIT_SIZE] : INTERVALS) {
        // Only add as many time units as specified by granularity
        if (time_units.size() >= granularity) {
            break;
        }

        // Get number of units and subtract from the seconds left
        unsigned int unit_amount = seconds / UNIT_SIZE;
        seconds -= unit_amount * UNIT_SIZE;
        // Don't include unit in time string if there are zero of them
        if (unit_amount == 0) {
            continue;
        }

        // Add unit string to list
        time_units.push_back(std::to_string(unit_amount) + UNIT_NAME);
        // Remove plural "s" if there is only one of this unit
        if (unit_amount == 1) {
            time_units.back().pop_back();
        }
    }

    switch (time_units.size()) {
        // If no units were added to the list, then there are zero seconds
        case 0:
            return "0 seconds";
        // If there is only one unit in the list, it's not a grammatical list
        case 1:
            return time_units.front();
        // If there are two units in the list, separate them by "and" with no commas
        case 2:
            return time_units.front() + " and " + time_units.back();
        // If there are at least three units in the list, separate them by commas with a final "and"
        default:
            // Start with first value
            std::string fancytime = time_units.front();
            // Add middle values with commas
            for (int i = 1; i < time_units.size() - 1; i++) {
                fancytime += ", " + time_units[i];
            }
            // Add final value with "and". Yes, we do Oxford Commas in this house!
            fancytime += ", and " + time_units.back();
            return fancytime;
    }
}

time_t util::time_string_to_seconds(const std::string& str) {
    std::istringstream ss(str);
    time_t seconds = 0;
    uint32_t time;
    while (ss >> time) {
        switch (ss.get()) {
            case 'D':
            case 'd':
                seconds += 86400LL * time;
                break;
            case 'H':
            case 'h':
                seconds += 3600LL * time;
                break;
            case 'M':
            case 'm':
                seconds += 60LL * time;
                break;
            case 'S':
            case 's':
                seconds += time;
                break;
            default:
                // If a number is ever not followed by one of these units, the format is invalid; return sentinel value.
                return -1;
        }
    }
    // If the stream ever fails to extract a number while there is still data left, the format is invalid.
    if (!ss.eof()) {
        return -1;
    }
    return seconds;
}

std::string util::sql_escape_string(const std::string_view str, const bool wrap_single_quotes) {
    std::string escaped_str;
    if (wrap_single_quotes) {
        escaped_str += '\'';
    }
    for (char c : str) {
        if (c == '\'') {
            escaped_str += "''";
        } else if (c == '\\') {
            escaped_str += "\\\\";
        } else if (c == '%' || c == '_') {
            escaped_str += "\\" + std::string(1, c);
        } else {
            escaped_str += c;
        }
    }
    if (wrap_single_quotes) {
        escaped_str += '\'';
    }
    return escaped_str;
}

void util::escape_newlines(std::string& str) {
    size_t pos = str.find("\\n");
    while (pos != std::string::npos) {
        // Don't replace if "\\n" is sent (double escape)
        if (str[pos - 1] != '\\') {
            str.replace(pos, 2, "\n");
        }
        // Find next occurence, if any
        pos = str.find("\\n", pos + 1);
    }
}

bool util::is_valid_command_name(const std::string_view command_name) {
    bool valid = true;
    for (char c : command_name) {
        if (!std::islower(c) && !std::isdigit(c) && c != '-' && c != '_') {
            valid = false;
            break;
        }
    }
    if (command_name.size() > 32 || std::isdigit(command_name[0])) valid = false;
    return valid;
}

dpp::task<dpp::confirmation_callback_t> util::get_message_cached(dpp::cluster* bot, const dpp::snowflake id, const dpp::snowflake channel) {
    // Try to get message by ID from cache, searching backwards (latest to earliest messages)
    auto it = MESSAGE_CACHE.end();
    while (it-- != MESSAGE_CACHE.begin()) {
        if (it->id == id) {
            break;
        }
    }
    // If it's not found in the cache, try to get it from Discord
    if (it == MESSAGE_CACHE.begin() - 1) {
        co_return co_await bot->co_message_get(id, channel);
    }
    // Construct a confirmation_callback_t with the message inside
    co_return dpp::confirmation_callback_t(bot, *it, dpp::http_request_completion_t());
}

dpp::task<std::pair<util::command_search_result, dpp::snowflake>> util::find_command(dpp::cluster* bot, const nlohmann::json &config, const std::string command_name) {
    dpp::snowflake command_id(0);
    command_search_result result = COMMAND_NOT_FOUND;

    dpp::confirmation_callback_t global_search = co_await bot->co_global_commands_get();
    if (global_search.is_error()) {
        co_return {SEARCH_ERROR, command_id};
    }
    for (const auto &command: std::get<dpp::slashcommand_map>(global_search.value)) {
        if (command.second.name == command_name) {
            result = GLOBAL_COMMAND_FOUND;
            command_id = command.first;
            break;
        }
    }

    if (result == GLOBAL_COMMAND_FOUND) {
        co_return {result, command_id};
    }

    dpp::confirmation_callback_t guild_search = co_await bot->co_guild_commands_get(config["guild_id"]);
    if (guild_search.is_error()) {
        co_return {SEARCH_ERROR, command_id};
    }
    for (const auto &command: std::get<dpp::slashcommand_map>(guild_search.value)) {
        if (command.second.name == command_name) {
            result = GUILD_COMMAND_FOUND;
            command_id = command.first;
            break;
        }
    }

    co_return {result, command_id};
}

dpp::task<bool> util::check_perms(dpp::cluster* bot, const nlohmann::json& config, const dpp::snowflake issuer, const dpp::snowflake subject) {
    const dpp::snowflake hierarchy[4] = {config["role_ids"]["owner"], config["role_ids"]["moderator"], config["role_ids"]["trial_mod"], config["role_ids"]["support_team"]};
    const dpp::confirmation_callback_t issuer_conf = co_await bot->co_guild_get_member(config["guild_id"], issuer);
    const dpp::confirmation_callback_t subject_conf = co_await bot->co_guild_get_member(config["guild_id"], subject);
    // If the command issuer is not a server member, they do not have permission
    if (issuer_conf.is_error()) {
        co_return false;
    }
    // If the command subject is not a server member, they cannot be in the hierarchy so the issuer has permission
    if (subject_conf.is_error()) {
        co_return true;
    }

    // Get hierarchy index of role of highest rank for issuer and subject
    int issuer_rank_index = 4;
    int subject_rank_index = 4;
    std::vector<dpp::snowflake> issuer_roles = std::get<dpp::guild_member>(issuer_conf.value).get_roles();
    std::vector<dpp::snowflake> subject_roles = std::get<dpp::guild_member>(subject_conf.value).get_roles();
    for (int i = 0; i < 4; i++) {
        if (std::ranges::find(issuer_roles, hierarchy[i]) != issuer_roles.end()) {
            issuer_rank_index = i;
            break;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (std::ranges::find(subject_roles, hierarchy[i]) != subject_roles.end()) {
            subject_rank_index = i;
            break;
        }
    }
    co_return issuer_rank_index < subject_rank_index;
}

dpp::job util::remind(dpp::cluster* bot, sqlite3* db, const reminder reminder) {
    const time_t now = time(nullptr);
    if (reminder.end_time < now) {
        // If reminder end time has already passed, send the user a belated reminder notification
        dpp::embed embed = dpp::embed().set_title("Belated Reminder").set_color(util::color::DEFAULT)
        .set_description(std::format("Sorry, the bot was offline when you were supposed to get your reminder of {} from <t:{}>.",
        seconds_to_fancytime(reminder.end_time - reminder.start_time, 4), reminder.start_time))
        .add_field("Reminder", reminder.text);
        bot->direct_message_create(reminder.user, dpp::message(embed));
        // Remove reminder from DB
        char* error_message;
        sqlite3_exec(db, std::format("DELETE FROM reminders WHERE id={};", reminder.id).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            log("SQL ERROR", error_message);
            sqlite3_free(error_message);
        }
        co_return;
    }

    co_await bot->co_sleep(reminder.end_time - now);

    // Send user reminder notification
    dpp::embed embed = dpp::embed().set_title(std::format("Reminder of {} from <t:{}>",
    seconds_to_fancytime(reminder.end_time - reminder.start_time, 4),
    reminder.start_time)).set_color(util::color::DEFAULT).set_description(reminder.text);
    bot->direct_message_create(reminder.user, dpp::message(embed));
    // Remove reminder from DB
    char* error_message;
    sqlite3_exec(db, std::format("DELETE FROM reminders WHERE id={};", reminder.id).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        log("SQL ERROR", error_message);
        sqlite3_free(error_message);
    }
}

dpp::job util::handle_mute(dpp::cluster* bot, sqlite3* db, const nlohmann::json& config, const mute mute) {
    // Wait until mute expires, unless expiry time has already passed
    const time_t now = time(nullptr);
    if (mute.end_time > now) {
        co_await bot->co_sleep(mute.end_time - now);
    }
    // Mark mute as inactive in DB
    if (mute.id != 0) {
        char* error_message;
        sqlite3_exec(db, std::format("UPDATE mod_records SET active = 'false' WHERE extra_data={};", mute.id).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            log("SQL ERROR", error_message);
            sqlite3_free(error_message);
        }
    }
    // No messages sent if user has left server or is no longer muted
    dpp::confirmation_callback_t member_conf = co_await bot->co_guild_get_member(config["guild_id"], mute.user);
    if (member_conf.is_error()) {
        co_return;
    }
    dpp::guild_member user = std::get<dpp::guild_member>(member_conf.value);
    std::vector<dpp::snowflake> roles = user.get_roles();
    if (std::ranges::find(roles, config["role_ids"]["muted"].get<dpp::snowflake>()) == roles.end()) {
        co_return;
    }
    // Send user notification and log the unmute action
    dpp::embed dm_embed = dpp::embed().set_color(util::color::GREEN).set_title("You have been automatically unmuted.");
    dpp::embed log_embed = dpp::embed().set_color(util::color::GREEN).set_title("Mute removed").set_thumbnail(user.get_user()->get_avatar_url())
                                       .add_field("User unmuted", user.get_nickname(), true)
                                       .add_field("User ID", mute.user.str(), true)
                                       .add_field("Reason", "Automatic unmute", false);
    dpp::confirmation_callback_t dm_conf = co_await bot->co_direct_message_create(mute.user, dpp::message(dm_embed));
    if (dm_conf.is_error()) {
        log_embed.set_footer(dpp::embed_footer().set_text("Failed to DM user"));
    }
    bot->message_create(dpp::message(config["log_channel_ids"]["mod_log"], log_embed));
}

dpp::job util::handle_bump(dpp::cluster* bot, const nlohmann::json& config, const dpp::snowflake channel, const time_t seconds) {
    co_await bot->co_sleep(seconds);
    bot->message_create(dpp::message(channel, std::format("Time to bump the server!\n"
    "<@&{}>, could someone please run `/bump`?", config["role_ids"]["bump_reminder"].get<uint64_t>())
    ).set_allowed_mentions(false, true));
    BUMP_TIMER_RUNNING = false;
}