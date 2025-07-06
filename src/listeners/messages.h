/* messages: Handlers for events involving messages
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

namespace messages {
    /**
     * Get message content and any attachments and add this info as fields to an embed
     * @param embed Embed to add these fields to
     * @param message Message to examine
     */
    void add_message_content_fields(dpp::embed& embed, const dpp::message& message);

    // Event handlers
    void on_message(const dpp::message_create_t& event, const nlohmann::json& config);
    dpp::task<> on_message_deleted(const dpp::message_delete_t& event, const nlohmann::json& config);
    dpp::task<> on_message_edited(const dpp::message_update_t& event, const nlohmann::json& config);
    dpp::task<> on_reaction(const dpp::message_reaction_add_t& event, const nlohmann::json& config);
    dpp::task<> on_reaction_removed(const dpp::message_reaction_remove_t& event, const nlohmann::json& config);
}