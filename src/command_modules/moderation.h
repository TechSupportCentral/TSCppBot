/* moderation: Commands related to server moderation
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
#include <sqlite3.h>
#include <dpp/dpp.h>

namespace moderation {
    dpp::task<> create_ticket(const dpp::slashcommand_t &event, const nlohmann::json &config);
    dpp::task<> purge(const dpp::slashcommand_t &event, const nlohmann::json &config);
    void userinfo(const dpp::slashcommand_t &event, const nlohmann::json &config);
    dpp::task<> inviteinfo(const dpp::slashcommand_t &event);
    dpp::task<> warn(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db);
    dpp::task<> unwarn(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db);
    dpp::task<> mute(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db);
    dpp::task<> unmute(const dpp::slashcommand_t &event, const nlohmann::json &config, sqlite3* db);
}