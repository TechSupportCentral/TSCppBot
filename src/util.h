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
#pragma once
#include <string>
#include <dpp/dpp.h>
#include <sqlite3.h>

namespace util {
    /**
     * File stream for the log
     */
    inline std::ofstream LOG_FILE;

    /**
     * Possible result types for a slash command search
     */
    enum command_search_result {
        GLOBAL_COMMAND_FOUND,
        GUILD_COMMAND_FOUND,
        COMMAND_NOT_FOUND,
        SEARCH_ERROR
    };

    /**
     * Details for a user reminder
     */
    struct reminder {
        int64_t id; /**< ID for reminder in database */
        time_t start_time; /**< Time when user requested reminder */
        time_t end_time; /**< Time when notification should happen */
        dpp::snowflake user; /**< ID of user who requested the reminder */
        std::string text; /**< Text of the reminder */
    };

    /**
     * Print a message to the console and logfile, including the current date and time.
     * @param severity Severity of the event to log
     * @param message Message to log
     */
    void log(std::string_view severity, std::string_view message);

    /**
     * Convert a number of seconds into a fancy time string
     * @param seconds Amount of time in seconds
     * @param granularity Number of time units to include in string (precision)
     * @return A time string like "12 days, 3 hours, 2 minutes, and 5 seconds"
     */
    std::string seconds_to_fancytime(unsigned long long int seconds, unsigned short int granularity);

    /**
     * Convert a string representing a length of time to a number of seconds
     * @param str Time string in a format like "12d3h2m5s"
     * @return Number of seconds in the time string
     */
    time_t time_string_to_seconds(const std::string& str);

    /**
     * Add SQL escape sequences to a string for use in a query
     * @param str String to escape
     * @param wrap_single_quotes Whether to put the ' character at the beginning and end of the string
     * @return Escaped string
     */
    std::string sql_escape_string(std::string_view str, bool wrap_single_quotes = false);

    /**
     * Check if a string would be a valid Discord slash command name
     * @param command_name Proposed command name
     * @return true if the proposed command name is valid
     */
    bool is_valid_command_name(std::string_view command_name);

    /**
     * Find a slash command by name
     * @param bot Cluster to use for command search
     * @param config JSON bot config data
     * @param command_name Name of command to search for
     * @return Pair of a command_search_result enum and a snowflake with the command's ID if found, 0 otherwise.
     */
    dpp::task<std::pair<command_search_result, dpp::snowflake>> find_command(dpp::cluster* bot, const nlohmann::json &config, std::string command_name);

    /**
     * Wait for the duration of a reminder, then send the user a notification DM
     * @param bot Cluster to send the reminder DM with
     * @param db SQLite DB pointer with the database to delete reminder from
     * @param reminder reminder to send
     */
    dpp::job remind(dpp::cluster* bot, sqlite3* db, reminder reminder);
}