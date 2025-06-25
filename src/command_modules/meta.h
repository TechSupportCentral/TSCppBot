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
#pragma once
#include <dpp/dpp.h>
#include <sqlite3.h>

namespace meta {
    void ping(const dpp::slashcommand_t &event);
    void uptime(const dpp::slashcommand_t &event);
    void get_commit(const dpp::slashcommand_t &event);
    void send_message(const dpp::slashcommand_t &event);
    dpp::task<> dm(const dpp::slashcommand_t &event, const nlohmann::json &config);
    void announce(const dpp::slashcommand_t &event, const nlohmann::json &config);
    void remindme(const dpp::slashcommand_t &event, sqlite3* db);
    void set_bump_timer(const dpp::slashcommand_t &event, const nlohmann::json &config);
}