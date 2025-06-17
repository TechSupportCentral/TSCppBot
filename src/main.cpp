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
#include "command_modules/moderation.h"
#include "command_modules/server_info.h"
#include "command_modules/db_commands.h"
#include "util.h"
#include <fstream>

std::string DATA_PATH;
std::string DB_FILE;

/**
 * Start and run the bot with state initialized from DB and JSON files, and set up event handlers
 * @param argc Number of optional arguments passed
 * @param argv List of optional arguments passed (data path, DB filename, log filename)
 * @return 0 if successful,
 *         1 for invalid data path,
 *         2 for invalid DB filename,
 *         3 for invalid log filename,
 *         4 for missing JSON files, or
 *         any other value for a critical exception from D++ or the JSON parser.
 */
int main(int argc, char* argv[]) {
    // Set data path from first argument if it exists
    if (argc > 1) {
        // If the first char of the first arg is a dash, assume user is trying to specify an option
        if (argv[1][0] == '-') {
            std::cout << "Usage: " << argv[0] << " [data path] [DB filename] [log filename]" << std::endl
                      << "Default data path is the parent of the current directory." << std::endl
                      << "Default DB filename is TSCppBot.db." << std::endl
                      << "Default log filename is based on the current time." << std::endl
                      << "If a specified log file already exists, its contents will be appended to." << std::endl;
            return 0;
        }
        // Make sure data path is a valid dir
        DATA_PATH = argv[1];
        if (std::error_code err; !std::filesystem::is_directory(DATA_PATH, err)) {
            std::cerr << "Invalid data path \"" << DATA_PATH << "\": " << err.message() << std::endl;
            return 1;
        }
    } else {
        DATA_PATH = "..";
    }
    // Set DB file from second argument if it exists
    if (argc > 2) {
        DB_FILE = DATA_PATH + '/' + argv[2];
    } else {
        DB_FILE = DATA_PATH + "/TSCppBot.db";
    }
    // Set logfile from third argument if it exists
    if (argc > 3) {
        util::LOG_FILE.open(DATA_PATH + '/' + argv[3], std::ios::app);
        if (util::LOG_FILE.fail()) {
            std::cerr << "Failed to open log file \"" << DATA_PATH << '/' << argv[3] << "\" for writing: " << strerror(errno) << std::endl;
            return 3;
        }
    } else {
        // Create log dir
        try {
            std::filesystem::create_directory(DATA_PATH + "/log");
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << std::endl;
            return 3;
        }
        // Create log filename from current date and time
        const time_t now = time(nullptr);
        char log_file[32];
        strftime(log_file, 32, "log/TSCppBot_%Y%m%d-%H.%M.log", localtime(&now));
        // Try to create/replace logfile
        util::LOG_FILE.open(DATA_PATH + '/' + log_file);
        if (util::LOG_FILE.fail()) {
            std::cerr << "Failed to create log file \"" << DATA_PATH << '/' << log_file << "\": " << strerror(errno) << std::endl;
            return 3;
        }
    }

    // Load JSON files for config and command list
    std::ifstream config_file = std::ifstream(DATA_PATH + "/config.json");
    if (config_file.fail()) {
        std::cerr << "Failed to open config file \"" << DATA_PATH << "/config.json\": " << strerror(errno) << std::endl;
        return 4;
    }
    std::ifstream commands_file = std::ifstream(DATA_PATH + "/commands.json");
    if (commands_file.fail()) {
        std::cerr << "Failed to open commands file \"" << DATA_PATH << "/commands.json\": " << strerror(errno) << std::endl;
        return 4;
    }
    nlohmann::json config = nlohmann::json::parse(config_file);
    nlohmann::json commands = nlohmann::json::parse(commands_file);
    config_file.close();
    commands_file.close();
    // Initialize DB
    sqlite3 *db;
    int status = sqlite3_open(DB_FILE.c_str(), &db);
    if (status != 0) {
        std::cerr << "Failed to open database \"" << DB_FILE << "\": " << sqlite3_errmsg(db) << std::endl;
        return 2;
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
        },
    &db_text_commands, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
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
                    int status = sqlite3_open(DB_FILE.c_str(), &db);
                    if (status != 0) {
                        return;
                    }
                    // Execute query
                    char* error_message;
                    sqlite3_exec(db, std::format("SELECT * FROM embed_command_fields WHERE id IN ({});", fields).c_str(),
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
                        util::log("SQL ERROR", error_message);
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
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
    }

    // Set bot token and intents, and enable logging
    uint32_t intents = dpp::i_default_intents + dpp::i_message_content + dpp::i_guild_members;
    dpp::cluster bot(config["bot_token"], intents);
    bot.on_log([](const dpp::log_t& event) {
        if (event.severity >= dpp::ll_info) {
            util::log(dpp::utility::loglevel(event.severity), event.message);
        }
    });

    bot.on_slashcommand([&config, &db_text_commands, &db_embed_commands, &db](const dpp::slashcommand_t &event) -> dpp::task<> {
        std::string command_name = event.command.get_command_name();
        if (command_name == "add-text-command") db_commands::add_text_command_modal(event);
        else if (command_name == "add-embed-command") co_await db_commands::add_embed_command(event, config, db_embed_commands, db);
        else if (command_name == "add-embed-command-field") db_commands::add_embed_command_field_modal(event, db_embed_commands);
        else if (command_name == "remove-embed-command-field") db_commands::remove_embed_command_field_menu(event, db_embed_commands, db);
        else if (command_name == "edit-embed-command-field") db_commands::edit_embed_command_field_menu(event, db_embed_commands, db);
        else if (command_name == "remove-db-command") co_await db_commands::remove_command(event, config, db_text_commands, db_embed_commands, db);
        else if (command_name == "db-command-list") db_commands::get_commands(event, db_text_commands, db_embed_commands);
        else if (command_name == "ping") meta::ping(event);
        else if (command_name == "uptime") meta::uptime(event);
        else if (command_name == "commit") meta::get_commit(event);
        else if (command_name == "sendmessage") meta::send_message(event);
        else if (command_name == "announce") meta::announce(event, config);
        else if (command_name == "dm") co_await meta::dm(event, config);
        else if (command_name == "remindme") meta::remindme(event, db);
        else if (command_name == "rules") server_info::rules(event, config);
        else if (command_name == "rule") server_info::rule(event, config);
        else if (command_name == "suggest") server_info::suggest(event, config);
        else if (command_name == "suggestion-respond") co_await server_info::suggestion_response(event, config);
        else if (command_name == "create-ticket") co_await moderation::create_ticket(event, config);
        else if (command_name == "purge") co_await moderation::purge(event, config);
        else if (command_name == "userinfo") moderation::userinfo(event, config);
        else if (command_name == "inviteinfo") co_await moderation::inviteinfo(event);
        else if (command_name == "warn") co_await moderation::warn(event, config, db);
        else if (command_name == "unwarn") co_await moderation::unwarn(event, config, db);
        else if (command_name == "mute") co_await moderation::mute(event, config, db);
        else if (command_name == "unmute") co_await moderation::unmute(event, config, db);
        else if (command_name == "kick") co_await moderation::kick(event, config, db);
        else if (command_name == "ban") co_await moderation::ban(event, config, db);
        else if (command_name == "unban") co_await moderation::unban(event, config, db);
        else {
            auto text_command = db_text_commands.find(command_name);
            if (text_command != db_text_commands.end()) {
                event.reply(text_command->second.value);
            } else {
                auto embed_command = db_embed_commands.find(command_name);
                if (embed_command != db_embed_commands.end()) {
                    event.reply(dpp::message(event.command.channel_id, embed_command->second.embed));
                } else {
                    util::log("INFO", std::format("Command /{} does not exist.", command_name));
                }
            }
        }
    });
    bot.on_select_click([&db_embed_commands, &db](const dpp::select_click_t &event) {
        if (event.custom_id == "remove_field_select") db_commands::remove_embed_command_field(event, db_embed_commands, db);
        else if (event.custom_id == "edit_field_select") db_commands::edit_embed_command_field_modal(event, db);
    });
    bot.on_form_submit([&config, &db_text_commands, &db_embed_commands, &db](const dpp::form_submit_t &event) -> dpp::task<> {
        if (event.custom_id == "add_text_command_form") co_await db_commands::add_text_command(event, config, db_text_commands, db);
        else if (event.custom_id.substr(0, 14) == "add_field_form") db_commands::add_embed_command_field(event, db_embed_commands, db);
        else if (event.custom_id.substr(0, 15) == "edit_field_form") db_commands::edit_embed_command_field(event, db_embed_commands, db);
    });

    bot.on_ready([&bot, &config, &commands, &db, &db_text_commands, &db_embed_commands](const dpp::ready_t &event) {
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
                        if (option["min_length"] != nullptr) {
                            command_option.set_min_length(option["min_length"].get<int64_t>());
                        }
                        if (option["max_length"] != nullptr) {
                            command_option.set_max_length(option["max_length"].get<int64_t>());
                        }
                        slash_command.add_option(command_option);
                    }

                    switch (command["permission_level"].get<util::command_perms>()) {
                        case util::ADMIN_ONLY:
                            // Only admins can use the command
                            slash_command.set_default_permissions(dpp::permissions::p_administrator);
                            tsc_commands.push_back(slash_command);
                            break;
                        case util::MOD_ONLY:
                            // Only users with manage threads (moderators) can use the command
                            slash_command.set_default_permissions(dpp::permissions::p_manage_threads);
                            tsc_commands.push_back(slash_command);
                            break;
                        case util::TRIAL_MOD_ONLY:
                            // Only users with manage messages (trial mods and mods) can use the command
                            slash_command.set_default_permissions(dpp::permissions::p_manage_messages);
                            tsc_commands.push_back(slash_command);
                            break;
                        case util::STAFF_ONLY:
                            // Only users who can change their own nickname (staff) can use the command
                            slash_command.set_default_permissions(dpp::permissions::p_change_nickname);
                            tsc_commands.push_back(slash_command);
                            break;
                        case util::GLOBAL:
                            // This command can be run by anyone, and it can be run anywhere including DMs
                            slash_command.set_dm_permission(true);
                            global_commands.push_back(slash_command);
                            break;
                        case util::SERVER_ONLY:
                        default:
                            // Default: This command can be run by anyone, but only within the server
                            tsc_commands.push_back(slash_command);
                            break;
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

        // Resume remaining reminders
        std::vector<util::reminder> reminder_list;
        char* error_message;
        sqlite3_exec(db, "SELECT id, start_time, end_time, user, text FROM reminders;",
            [](void* reminder_list, int column_count, char** column_values, char** column_names) -> int {
                auto reminders = static_cast<std::vector<util::reminder>*>(reminder_list);
                util::reminder reminder;
                reminder.id = strtoll(column_values[0], nullptr, 10);
                reminder.start_time = strtoll(column_values[1], nullptr, 10);
                reminder.end_time = strtoll(column_values[2], nullptr, 10);
                reminder.user = strtoull(column_values[3], nullptr, 10);
                reminder.text = column_values[4];
                reminders->push_back(reminder);
                return 0;
            },
        &reminder_list, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
        }
        for (const util::reminder& reminder : reminder_list) {
            std::string log_message;
            if (reminder.end_time < time(nullptr)) {
                log_message += "Belated";
            } else {
                log_message += "Resuming";
            }
            log_message += " reminder of " + util::seconds_to_fancytime(reminder.end_time - reminder.start_time, 4);
            if (const dpp::user* user = dpp::find_user(reminder.user); user != nullptr) {
                log_message += " from " + user->username;
            }
            util::log("INFO", log_message);
            util::remind(&bot, db, reminder);
        }

        // Resume remaining mutes
        std::vector<util::mute> mute_list;
        sqlite3_exec(db, "SELECT user, extra_data_id FROM mod_records WHERE type='mute' AND active='true';",
            [](void* mute_list, int column_count, char** column_values, char** column_names) -> int {
                auto mutes = static_cast<std::vector<util::mute>*>(mute_list);
                util::mute mute;
                mute.user = strtoull(column_values[0], nullptr, 10);
                mute.id = strtoll(column_values[1], nullptr, 10);
                mutes->push_back(mute);
                return 0;
            },
        &mute_list, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
        }
        for (util::mute& mute : mute_list) {
            sqlite3_exec(db, std::format("SELECT start_time, end_time FROM mutes WHERE id={};", mute.id).c_str(),
                [](void* output, int column_count, char** column_values, char** column_names) -> int {
                    auto mute = static_cast<util::mute*>(output);
                    mute->start_time = strtoll(column_values[0], nullptr, 10);
                    mute->end_time = strtoll(column_values[1], nullptr, 10);
                    return 0;
                },
            &mute, &error_message);
            if (error_message != nullptr) {
                util::log("SQL ERROR", error_message);
                sqlite3_free(error_message);
                mute.start_time = time(nullptr);
                mute.end_time = mute.start_time;
            }

            std::string log_message;
            if (mute.end_time < time(nullptr)) {
                log_message += "Belated removal of";
            } else {
                log_message += "Resuming";
            }
            log_message += " mute of " + util::seconds_to_fancytime(mute.end_time - mute.start_time, 4);
            if (const dpp::user* user = dpp::find_user(mute.user); user != nullptr) {
                log_message += " from " + user->username;
            }
            util::log("INFO", log_message);
            util::handle_mute(&bot, db, config, mute);
        }
    });

    bot.start(dpp::st_wait);
    sqlite3_close(db);
}