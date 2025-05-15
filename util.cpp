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

std::string util::secondsToFancyTime(unsigned int seconds, unsigned short int granularity) {
    const std::map<const char*, int> INTERVALS = {
        {"days", 86400},
        {"hours", 3600},
        {"minutes", 60},
        {"seconds", 1}
    };
    std::vector<std::string> timeUnits;

    // Divide number of seconds into days, hours, minutes, and seconds and create strings
    for (const auto& [UNIT_NAME, UNIT_SIZE] : INTERVALS) {
        // Only add as many time units as specified by granularity
        if (timeUnits.size() >= granularity) {
            break;
        }

        // Get number of units and subtract from the seconds left
        unsigned int unitAmount = seconds / UNIT_SIZE;
        seconds -= unitAmount * UNIT_SIZE;
        // Don't include unit in time string if there are zero of them
        if (unitAmount == 0) {
            continue;
        }

        // Add unit string to list
        timeUnits.push_back(std::to_string(unitAmount) + ' ' + UNIT_NAME);
        // Remove plural "s" if there is only one of this unit
        if (unitAmount == 1) {
            timeUnits.back().pop_back();
        }
    }

    switch (timeUnits.size()) {
        // If no units were added to the list, then there are zero seconds
        case 0:
            return "0 seconds";
        // If there is only one unit in the list, it's not a grammatical list
        case 1:
            return timeUnits.front();
        // If there are two units in the list, separate them by "and" with no commas
        case 2:
            return timeUnits.front() + " and " + timeUnits.back();
        // If there are at least three units in the list, separate them by commas with a final "and"
        default:
            // Start with first value
            std::string fancyTime = timeUnits.front();
            // Add middle values with commas
            for (int i = 1; i < timeUnits.size() - 1; i++) {
                fancyTime += ", " + timeUnits[i];
            }
            // Add final value with "and". Yes, we do Oxford Commas in this house!
            fancyTime += ", and " + timeUnits.back();

            return fancyTime;
    }
}

std::string util::sql_escape_string(const std::string_view str, bool wrap_single_quotes) {
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

bool util::valid_command_name(std::string_view command_name) {
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