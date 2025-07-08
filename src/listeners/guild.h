/* guild: Handlers for events related to the guild
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

namespace guild {
    void on_invite_created(const dpp::invite_create_t &event, const nlohmann::json& config, std::vector<dpp::invite>& invites);
    void on_invite_deleted(const dpp::invite_delete_t &event, const nlohmann::json& config, std::vector<dpp::invite>& invites);
}