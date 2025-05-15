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

namespace util {
    enum command_search_result {
        COMMAND_FOUND,
        COMMAND_NOT_FOUND,
        WAITING,
        ERROR
    };

    std::string secondsToFancyTime(unsigned int seconds, unsigned short int granularity);
    std::string sql_escape_string(std::string_view str, bool wrap_single_quotes = false);
    bool valid_command_name(std::string_view command_name);
    std::tuple<command_search_result, command_search_result, dpp::snowflake> find_command(const dpp::slashcommand_t &event, const nlohmann::json &config, const std::string &command_name);
}