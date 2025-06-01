/* db_commands: Bot commands stored in a database
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

namespace db_commands {
    struct text_command {
        std::string description;
        std::string value;
        bool global;
    };
    struct embed_command {
        std::string description;
        dpp::embed embed;
        bool global;
    };

    void add_text_command_modal(const dpp::slashcommand_t &event);
    dpp::task<> add_text_command(const dpp::form_submit_t &event, const nlohmann::json &config, std::unordered_map<std::string, text_command> &text_commands, sqlite3 *db);
    dpp::task<> add_embed_command(const dpp::slashcommand_t &event, const nlohmann::json &config, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db);
    void add_embed_command_field_modal(const dpp::slashcommand_t &event, const std::unordered_map<std::string, embed_command> &embed_commands);
    void add_embed_command_field(const dpp::form_submit_t &event, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db);
    void remove_embed_command_field_menu(const dpp::slashcommand_t &event, const std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db);
    void remove_embed_command_field(const dpp::select_click_t &event, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db);
    void edit_embed_command_field_menu(const dpp::slashcommand_t &event, const std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db);
    void edit_embed_command_field_modal(const dpp::select_click_t &event, sqlite3 *db);
    void edit_embed_command_field(const dpp::form_submit_t &event, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db);
    dpp::task<> remove_command(const dpp::slashcommand_t &event, const nlohmann::json &config, std::unordered_map<std::string, text_command> &text_commands, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db);
    void get_commands(const dpp::slashcommand_t &event, std::unordered_map<std::string, text_command> &text_commands, std::unordered_map<std::string, embed_command> &embed_commands);
}