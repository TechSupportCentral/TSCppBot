/* TSCppBot: Helper bot for the Tech Support Central Discord server
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
#include "command_modules/meta.h"
#include "command_modules/server_info.h"
#include "command_modules/db_commands.h"
#include <fstream>

#define DATA_PATH "/home/ben/CLionProjects/TSCppBot"
constexpr const char* DB_FILE = DATA_PATH "/TSCppBot.db";

int main() {
    // Load config and command files
    nlohmann::json config = nlohmann::json::parse(std::ifstream(DATA_PATH "/config.json"));
    nlohmann::json commands = nlohmann::json::parse(std::ifstream(DATA_PATH "/commands.json"));

    // Initialize DB
    sqlite3 *db;
    int status = sqlite3_open(DB_FILE, &db);
    if (status != 0) {
        std::cout << "Failed to open database: " << sqlite3_errmsg(db) << std::endl;
    }

    std::unordered_map<std::string, db_commands::text_command> db_text_commands;
    std::unordered_map<std::string, db_commands::embed_command> db_embed_commands;
    // Get DB text command list
    char* error_message;
    sqlite3_exec(db, "SELECT * FROM text_commands;",
        [](void* command_list, int column_count, char** column_values, char** column_names) -> int {
            auto db_text_commands = static_cast<std::unordered_map<std::string, db_commands::text_command>*>(command_list);

            db_commands::text_command text_command;
            text_command.description = std::string(column_values[1]);
            text_command.value = std::string(column_values[2]);
            text_command.global = strcmp(column_values[3], "false");

            db_text_commands->emplace(column_values[0], text_command);
            return 0;
        }, &db_text_commands, &error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << error_message << std::endl;
        sqlite3_free(error_message);
    }
    // Get DB embed command list
    sqlite3_exec(db, "SELECT * FROM embed_commands;",
        [](void* command_list, int column_count, char** column_values, char** column_names) -> int {
            auto db_embed_commands = static_cast<std::unordered_map<std::string, db_commands::embed_command>*>(command_list);

            db_commands::embed_command embed_command;
            embed_command.description = std::string(column_values[1]);
            embed_command.global = strcmp(column_values[2], "false");
            embed_command.embed = dpp::embed();
            if (column_values[3] != nullptr) embed_command.embed.set_title(column_values[3]);
            if (column_values[4] != nullptr) embed_command.embed.set_url(column_values[4]);
            if (column_values[5] != nullptr) embed_command.embed.set_description(column_values[5]);
            if (column_values[6] != nullptr) embed_command.embed.set_thumbnail(column_values[6]);
            if (column_values[7] != nullptr) embed_command.embed.set_image(column_values[7]);
            if (column_values[8] != nullptr) embed_command.embed.set_video(column_values[8]);
            if (column_values[9] != nullptr) {
                embed_command.embed.set_color(std::strtoul(column_values[9], nullptr, 10));
            }
            if (column_values[10] != nullptr) {
                embed_command.embed.set_timestamp(std::strtoll(column_values[10], nullptr, 10));
            }
            if (column_values[11] != nullptr || column_values[12] != nullptr || column_values[13] != nullptr) {
                embed_command.embed.set_author(column_values[11], column_values[12], column_values[13]);
            }
            if (column_values[14] != nullptr || column_values[15] != nullptr) {
                embed_command.embed.set_footer(column_values[14], column_values[15]);
            }
            if (column_values[16] != nullptr) {
                // Nested DB query requires an inner function to avoid capturing sqlite's existing pointers
                auto add_fields = [](const std::string &fields, db_commands::embed_command &embed_command) {
                    // Open new DB connection to avoid messing with existing one
                    sqlite3 *db;
                    int status = sqlite3_open(DB_FILE, &db);
                    if (status != 0) {
                        return;
                    }
                    // Execute query
                    char* error_message;
                    sqlite3_exec(db, (std::string("SELECT * FROM embed_command_fields WHERE id IN (") + fields + ") ORDER BY id ASC;").c_str(),
                        [](void* command, int column_count, char** column_values, char** column_names) -> int {
                            auto embed_command = static_cast<db_commands::embed_command*>(command);
                            embed_command->embed.add_field(
                                column_values[1], // field name
                                column_values[2], // field value
                                strcmp(column_values[3], "false")); // if field is inline or not
                            return 0;
                        },
                    &embed_command, &error_message);
                    if (error_message != nullptr) {
                        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << error_message << std::endl;
                        sqlite3_free(error_message);
                    }
                    // Close new DB connection
                    sqlite3_close(db);
                };
                add_fields(column_values[16], embed_command);
            }

            db_embed_commands->emplace(column_values[0], embed_command);
            return 0;
        },
    &db_embed_commands, &error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << error_message << std::endl;
        sqlite3_free(error_message);
    }

    // Set bot token and intents, and enable logging
    uint32_t intents = dpp::i_default_intents + dpp::i_message_content + dpp::i_guild_members;
    dpp::cluster bot(config["bot_token"], intents);
    bot.on_log([](const dpp::log_t& event) {
        if (event.severity >= dpp::ll_info) {
            std::cout << "[" << dpp::utility::current_date_time() << "] " << dpp::utility::loglevel(event.severity) << ": " << event.message << std::endl;
        }
    });

    bot.on_slashcommand([&config, &db_text_commands, &db_embed_commands, &db](const dpp::slashcommand_t &event) {
        std::string command_name = event.command.get_command_name();
        if (command_name == "add-text-command") db_commands::add_text_command(event, config, db_text_commands, db);
        else if (command_name == "add-embed-command") db_commands::add_embed_command(event, config, db_embed_commands, db);
        else if (command_name == "add-embed-command-field") db_commands::add_embed_command_field(event, db_embed_commands, db);
        else if (command_name == "remove-db-command") db_commands::remove_command(event, config, db_text_commands, db_embed_commands, db);
        else if (command_name == "db-command-list") db_commands::get_commands(event, db_text_commands, db_embed_commands);
        else if (command_name == "ping") meta::ping(event);
        else if (command_name == "uptime") meta::uptime(event);
        else if (command_name == "commit") meta::getCommit(event);
        else if (command_name == "rules") server_info::rules(event, config);
        else if (command_name == "rule") server_info::rule(event, config);
        else {
            auto text_command = db_text_commands.find(command_name);
            if (text_command != db_text_commands.end()) {
                event.reply(text_command->second.value);
            } else {
                auto embed_command = db_embed_commands.find(command_name);
                if (embed_command != db_embed_commands.end()) {
                    event.reply(dpp::message(event.command.channel_id, embed_command->second.embed));
                } else {
                    std::cout << "[" << dpp::utility::current_date_time() << "] INFO: Command /" << command_name << " does not exist." << std::endl;
                }
            }
        }
    });

    bot.on_ready([&bot, &commands, &config, &db_text_commands, &db_embed_commands](const dpp::ready_t &event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::vector<dpp::slashcommand> global_commands;
            std::vector<dpp::slashcommand> tsc_commands;
            for (auto &category : commands) {
                for (auto &command : category) {
                    dpp::slashcommand slash_command(
                        command["name"].get<std::string>(),
                        command["description"].get<std::string>(),
                        bot.me.id
                    );

                    for (auto &option : command["options"]) {
                        dpp::command_option command_option(
                            option["type"],
                            option["name"],
                            option["description"],
                            option["required"]
                        );
                        if (option["min_value"] != nullptr) {
                            command_option.set_min_value(option["min_value"].get<int64_t>());
                        }
                        if (option["max_value"] != nullptr) {
                            command_option.set_max_value(option["max_value"].get<int64_t>());
                        }
                        slash_command.add_option(command_option);
                    }

                    if (command["global"]) {
                        slash_command.set_dm_permission(true);
                        global_commands.push_back(slash_command);
                    } else {
                        if (command["admin_only"]) {
                            slash_command.set_default_permissions(dpp::permissions::p_administrator);
                        } else if (command["mod_only"]) {
                            slash_command.set_default_permissions(dpp::permissions::p_manage_messages);
                        }
                        tsc_commands.push_back(slash_command);
                    }
                }
            }
            for (const auto& [name, command] : db_text_commands) {
                dpp::slashcommand slash_command(name, command.description, bot.me.id);
                if (command.global) {
                    slash_command.set_dm_permission(true);
                    global_commands.push_back(slash_command);
                } else {
                    tsc_commands.push_back(slash_command);
                }
            }
            for (const auto& [name, command] : db_embed_commands) {
                dpp::slashcommand slash_command(name, command.description, bot.me.id);
                if (command.global) {
                    slash_command.set_dm_permission(true);
                    global_commands.push_back(slash_command);
                } else {
                    tsc_commands.push_back(slash_command);
                }
            }
            bot.global_bulk_command_create(global_commands);
            bot.guild_bulk_command_create(tsc_commands, config["guild_id"]);
        }
        bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_watching, "TSC"));
    });

    bot.start(dpp::st_wait);
    sqlite3_close(db);
}