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

std::string util::seconds_to_fancytime(unsigned int seconds, const unsigned short int granularity) {
    const std::map<const char*, int> INTERVALS = {
        {"days", 86400},
        {"hours", 3600},
        {"minutes", 60},
        {"seconds", 1}
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
        time_units.push_back(std::to_string(unit_amount) + ' ' + UNIT_NAME);
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
            std::string fancyTime = time_units.front();
            // Add middle values with commas
            for (int i = 1; i < time_units.size() - 1; i++) {
                fancyTime += ", " + time_units[i];
            }
            // Add final value with "and". Yes, we do Oxford Commas in this house!
            fancyTime += ", and " + time_units.back();

            return fancyTime;
    }
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

bool util::valid_command_name(const std::string_view command_name) {
    bool valid = true;
    for (char c : command_name) {
        bool is_lowercase = c >= 'a' && c <= 'z';
        bool is_num = c >= '0' && c <= '9';
        bool is_allowed_special_char = c == '-' || c == '_';

        if (!is_lowercase && !is_num && !is_allowed_special_char) {
            valid = false;
            break;
        }
    }
    if (command_name.size() > 32) valid = false;
    return valid;
}

std::tuple<util::command_search_result, util::command_search_result, dpp::snowflake> util::find_command(const dpp::slashcommand_t &event, const nlohmann::json &config, const std::string &command_name) {
    dpp::snowflake command_id(0);

    command_search_result global_status = WAITING;
    event.owner->global_commands_get([&command_name, &command_id, &global_status](const dpp::confirmation_callback_t& data) {
        if (data.is_error()) {
            global_status = ERROR;
        } else {
            for (const auto &command: std::get<dpp::slashcommand_map>(data.value) | std::views::values) {
                if (command.name == command_name) {
                    global_status = COMMAND_FOUND;
                    command_id = command.id;
                    break;
                }
            }
            if (global_status == WAITING) global_status = COMMAND_NOT_FOUND;
        }
    });
    while (global_status == WAITING) {}

    if (global_status == COMMAND_FOUND) {
        return {COMMAND_FOUND, COMMAND_NOT_FOUND, command_id};
    }

    command_search_result guild_status = WAITING;
    event.owner->guild_commands_get(config["guild_id"], [&command_name, &command_id, &guild_status](const dpp::confirmation_callback_t& data) {
        if (data.is_error()) {
            guild_status = ERROR;
        } else {
            for (const auto &command: std::get<dpp::slashcommand_map>(data.value) | std::views::values) {
                if (command.name == command_name) {
                    guild_status = COMMAND_FOUND;
                    command_id = command.id;
                    break;
                }
            }
            if (guild_status == WAITING) guild_status = COMMAND_NOT_FOUND;
        }
    });
    while (guild_status == WAITING) {}

    return {global_status, guild_status, command_id};
}