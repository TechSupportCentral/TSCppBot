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
#include <dpp/dpp.h>
#include <sqlite3.h>

namespace util {
    /**
     * File stream for the log
     */
    inline std::ofstream LOG_FILE;

    /**
     * Whether the bump timer is running
     */
    inline bool BUMP_TIMER_RUNNING = false;

    /**
     * Ring buffer used to store the newest N objects of type T
     * @tparam T Type of element to store
     * @tparam N Size of buffer
     */
    template<std::default_initializable T, size_t N>
    class cache {
        T data[N + 1]; /**< Underlying array everything is stored in */
        T* head = data; /**< Pointer to the oldest element placed in the buffer */
        T* tail = data - 1; /**< Pointer to the newest element placed in the buffer */
        size_t size = 0; /**< The number of elements currently stored */
        public:
            /**
             * Push a new element into the cache, making the oldest element inaccessible if the cache is full.
             * @param element The element to push to the cache
             */
            void push(T element) {
                /* If buffer is not yet full:
                 * Head is not moved
                 * Tail is moved forward one position, then element is placed at tail
                 * Size is incremented by one
                 */
                if (size < N) {
                    *(++tail) = element;
                    size++;
                /* If buffer is full and head is at the end of the array:
                 * Head is moved to the beginning of the array
                 * Tail is moved forward one position, then element is placed at tail
                 * Size is not incremented (buffer stays full)
                 */
                } else if (head == data + N) {
                    *(++tail) = element;
                    head = data;
                /* If buffer is full and tail is at the end of the array:
                 * Head is moved forward one position
                 * Tail is moved to the beginning of the array, then element is placed at tail
                 * Size is not incremented (buffer stays full)
                 */
                } else if (tail == data + N) {
                    tail = data;
                    *tail = element;
                    ++head;
                /* If buffer is full and neither head nor tail are at the end of the array:
                 * Head is moved forward one position
                 * Tail is moved forward one position, then element is placed at tail
                 * Size is not incremented (buffer stays full)
                 */
                } else {
                    *(++tail) = element;
                    ++head;
                }
            }
            /**
             * Iterator to traverse elements of the cache, in either direction
             */
            struct Iterator {
                using iterator_category = std::random_access_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using value_type = T;
                using pointer = T*;
                using reference = T&;
                Iterator(pointer data, pointer ptr) {
                    m_data = data;
                    m_ptr = ptr;
                }

                reference operator*() const { return *m_ptr; }
                pointer operator->() { return m_ptr; }
                // Prefix increment
                Iterator& operator++() {
                    // Wrap around when the iterator is at the end of the array
                    if (m_ptr == m_data + N) {
                        m_ptr = m_data;
                    } else {
                        ++m_ptr;
                    }
                    return *this;
                }
                // Postfix increment
                Iterator operator++(int) {
                    Iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                // Prefix decrement
                Iterator& operator--() {
                    // Wrap around when the iterator is at the beginning of the array
                    if (m_ptr == m_data) {
                        m_ptr = m_data + N;
                    } else {
                        --m_ptr;
                    }
                    return *this;
                }
                // Postfix decrement
                Iterator operator--(int) {
                    Iterator tmp = *this;
                    --(*this);
                    return tmp;
                }
                Iterator operator+(const difference_type n) {
                    Iterator tmp = *this;
                    difference_type cur_index = m_ptr - m_data;
                    difference_type new_index;
                    if (n >= 0) {
                        // Use modulo to wrap around the ring buffer
                        new_index = (cur_index + n) % (N + 1);
                    } else {
                        // True modulo instead of negative remainder
                        new_index = (cur_index + (N + 1 + n)) % (N + 1);
                    }
                    tmp.m_ptr = m_data + new_index;
                    return tmp;
                }
                Iterator operator-(const difference_type n) {
                    return *this + -n;
                }
                friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; }
                friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; }

                private:
                    pointer m_data;
                    pointer m_ptr;
            };
            /**
             * The cache begins at the oldest element (head)
             */
            Iterator begin() { return Iterator(data, head); }
            /**
             * The cache ends at the position between the newest and oldest elements (one position after tail)
             */
            Iterator end() { return Iterator(data, tail + 1); }
    };

    /**
     * Global cache of the 1000 latest messages (~1.25 MB)
     */
    inline cache<dpp::message, 1000> MESSAGE_CACHE;

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
     * Possible permission levels for a command
     */
    enum command_perms {
        ADMIN_ONLY, /**< Restrict to admins */
        MOD_ONLY, /**< Restrict to moderators */
        TRIAL_MOD_ONLY, /**< Restrict to trial mods and moderators */
        STAFF_ONLY, /**< Restrict to staff (support team and mod team) */
        SERVER_ONLY, /**< Command can be run by anyone, but only in the server */
        GLOBAL /**< Command can be run anyone anywhere, including DMs */
    };
    // Define JSON serialization for each value
    NLOHMANN_JSON_SERIALIZE_ENUM(command_perms, {
        {ADMIN_ONLY, "admin"},
        {MOD_ONLY, "moderator"},
        {TRIAL_MOD_ONLY, "trial_mod"},
        {STAFF_ONLY, "support_team"},
        {SERVER_ONLY, nullptr},
        {GLOBAL, "global"},
    })

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
     * Details for a mute action on a user
     */
    struct mute {
        int64_t id; /**< ID for mute in database */
        dpp::snowflake user; /**< ID of user who is muted */
        time_t start_time; /**< Time when user was muted */
        time_t end_time; /**< Time when mute expires */
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
     * Try to find a message in the cache, and get it via API if it's not cached.
     * @param bot Bot cluster to use for API call if message cannot be found in cache
     * @param channel ID of the channel the message is in
     * @param id ID of the message
     * @return confirmation_callback_t containing the message if it exists.
     */
    dpp::task<dpp::confirmation_callback_t> get_message_cached(dpp::cluster* bot, dpp::snowflake id, dpp::snowflake channel);

    /**
     * Find a slash command by name
     * @param bot Cluster to use for command search
     * @param config JSON bot config data
     * @param command_name Name of command to search for
     * @return Pair of a command_search_result enum and a snowflake with the command's ID if found, 0 otherwise.
     */
    dpp::task<std::pair<command_search_result, dpp::snowflake>> find_command(dpp::cluster* bot, const nlohmann::json &config, std::string command_name);

    /**
     * Make sure a user is allowed to run a command against another user
     * @param bot Pointer to bot cluster to find guild members with
     * @param config JSON bot configuration
     * @param issuer ID of user who issued the command
     * @param subject ID of user the command is acting on
     * @return true if the issuer is higher in the role hierarchy than the subject
     */
    dpp::task<bool> check_perms(dpp::cluster* bot, const nlohmann::json& config, dpp::snowflake issuer, dpp::snowflake subject);

    /**
     * Wait for the duration of a reminder, then send the user a notification DM
     * @param bot Cluster to send the reminder DM with
     * @param db SQLite DB pointer with the database to delete reminder from
     * @param reminder reminder to send
     */
    dpp::job remind(dpp::cluster* bot, sqlite3* db, reminder reminder);

    /**
     * Wait for the duration of a mute, then unmute the user and send a notification and log
     * @param bot Cluster to do these actions with
     * @param db SQLite DB pointer with the database to deactivate mute in
     * @param config JSON bot config data
     * @param mute mute info to use
     * @return
     */
    dpp::job handle_mute(dpp::cluster* bot, sqlite3* db, const nlohmann::json& config, mute mute);

    /**
     * Wait for some time then send a DISBOARD bump reminder.
     * @param bot Bot cluster to send reminder message with
     * @param config JSON bot config data
     * @param channel ID of channel to send reminder to
     * @param seconds Number of seconds to wait
     */
    dpp::job handle_bump(dpp::cluster* bot, const nlohmann::json& config, dpp::snowflake channel, time_t seconds);
}