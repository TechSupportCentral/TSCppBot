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
    inline std::ofstream LOG_FILE;

    enum command_search_result {
        GLOBAL_COMMAND_FOUND,
        GUILD_COMMAND_FOUND,
        COMMAND_NOT_FOUND,
        ERROR
    };

    struct reminder {
        int64_t id;
        time_t start_time;
        time_t end_time;
        dpp::snowflake user;
        std::string text;
    };

    void log(std::string_view severity, std::string_view message);
    std::string seconds_to_fancytime(unsigned long long int seconds, unsigned short int granularity);
    time_t short_time_string_to_seconds(const std::string& str);
    std::string sql_escape_string(std::string_view str, bool wrap_single_quotes = false);
    bool valid_command_name(std::string_view command_name);
    dpp::task<std::pair<command_search_result, dpp::snowflake>> find_command(dpp::cluster* bot, const nlohmann::json &config, std::string command_name);
    dpp::job remind(dpp::cluster* bot, sqlite3* db, reminder reminder);
}