/* members: Handlers for events involving server members
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

namespace members {
    /**
     * Find the most recent invite used.
     * @param bot Bot cluster to use to query guild invites
     * @param guild Guild to search invites of
     * @param invites List of invites to this guild (before the most recent invite use)
     * @return The most recently used invite, or an empty invite with code "unknown"
     */
    dpp::task<dpp::invite> findinvite(dpp::cluster* bot, dpp::snowflake guild, std::vector<dpp::invite>& invites);

    dpp::task<> on_kick(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config);
    dpp::task<> on_ban(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config);
    dpp::task<> on_unban(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config);
    dpp::task<> on_join(const dpp::guild_member_add_t& event, const nlohmann::json& config, std::vector<dpp::invite>& invites);
    void on_leave(const dpp::guild_member_remove_t& event, const nlohmann::json& config);
    dpp::task<> on_sus_join(const dpp::guild_join_request_delete_t& event, const nlohmann::json& config);
    dpp::task<> on_member_edit(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config);
    dpp::task<> on_roles_change(const dpp::guild_audit_log_entry_create_t& event, const nlohmann::json& config);
}