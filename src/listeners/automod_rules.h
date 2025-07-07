/* automod_rules: Handlers for automod rule changes
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

namespace automod_rules {
    void add_rule_description_fields(dpp::embed& embed, const dpp::automod_rule& rule);
    dpp::task<> on_automod_rule_add(const dpp::automod_rule_create_t &event, const nlohmann::json& config);
    void on_automod_rule_remove(const dpp::automod_rule_delete_t &event, const nlohmann::json& config);
    void on_automod_rule_edit(const dpp::automod_rule_update_t &event, const nlohmann::json& config);
}