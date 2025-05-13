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
#include <nlohmann/json.hpp>
#include <fstream>
using json = nlohmann::json;

int main() {
    // Load config and command files
    // TODO: Relative paths
    json config = json::parse(std::ifstream("/Users/ben/CLionProjects/TSCppBot/config.json"));
    json commands = json::parse(std::ifstream("/Users/ben/CLionProjects/TSCppBot/commands.json"));

    sqlite3 *db;
    int status = sqlite3_open("TSCppBot.db", &db);
    if (status != 0) {
        std::cout << "Failed to open database: " << sqlite3_errmsg(db) << std::endl;
    }

    // Set bot token and intents, and enable logging
    uint32_t intents = dpp::i_default_intents + dpp::i_message_content + dpp::i_guild_members;
    dpp::cluster bot(config["bot_token"], intents);
    bot.on_log([](const dpp::log_t& event) {
        if (event.severity >= dpp::ll_info) {
            std::cout << "[" << dpp::utility::current_date_time() << "] " << dpp::utility::loglevel(event.severity) << ": " << event.message << std::endl;
        }
    });

    bot.on_slashcommand([&config](const dpp::slashcommand_t &event) {
        if (event.command.get_command_name() == "ping") meta::ping(event);
        else if (event.command.get_command_name() == "uptime") meta::uptime(event);
        else if (event.command.get_command_name() == "commit") meta::getCommit(event);
        else if (event.command.get_command_name() == "rules") server_info::rules(event, config);
        else if (event.command.get_command_name() == "rule") server_info::rule(event, config);
    });

    bot.on_ready([&bot, &commands, &config](const dpp::ready_t &event) {
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
            bot.global_bulk_command_create(global_commands);
            bot.guild_bulk_command_create(tsc_commands, config["guild_id"]);
        }
        bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_watching, "TSC"));
    });

    bot.start(dpp::st_wait);
    sqlite3_close(db);
}