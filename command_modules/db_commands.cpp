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
#include "db_commands.h"
#include "../util.h"

void db_commands::add_text_command(const dpp::slashcommand_t &event, const nlohmann::json &config, std::unordered_map<std::string, text_command> &text_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking();
    // Make sure command name is valid
    std::string command_name = std::get<std::string>(event.get_parameter("name"));
    if (!util::valid_command_name(command_name)) {
        event.edit_original_response(dpp::message("Command name can only include lowercase letters, numbers, dash, or underscore, and must be 32 characters or less."));
        return;
    }
    // Make sure command doesn't already exist
    command_search_result status = WAITING;
    event.owner->global_commands_get([&command_name, &status](const dpp::confirmation_callback_t& data) {
        if (data.is_error()) {
            status = ERROR;
        } else {
            for (const auto& command : std::get<dpp::slashcommand_map>(data.value)) {
                if (command.second.name == command_name) {
                    status = COMMAND_FOUND;
                    break;
                }
            }
            if (status == WAITING) status = COMMAND_NOT_FOUND;
        }
    });
    while (status == WAITING) {}
    event.owner->guild_commands_get(config["guild_id"], [&command_name, &status](const dpp::confirmation_callback_t& data) {
        if (data.is_error()) {
            status = ERROR;
        } else {
            for (const auto& command : std::get<dpp::slashcommand_map>(data.value)) {
                if (command.second.name == command_name) {
                    status = COMMAND_FOUND;
                    break;
                }
            }
            if (status == WAITING) status = COMMAND_NOT_FOUND;
        }
    });
    while (status == WAITING) {}
    switch (status) {
        case COMMAND_FOUND:
            event.edit_original_response(dpp::message(std::string("Command `") + command_name + "` already exists."));
            return;
        case ERROR:
            event.edit_original_response(dpp::message("Bot failed to get guild command list."));
            return;
        default:
            break;
    }

    // Build text_command based on passed parameters
    text_command command;
    command.description = std::get<std::string>(event.get_parameter("description"));
    command.value = std::get<std::string>(event.get_parameter("content"));
    command.global = std::get<bool>(event.get_parameter("is_global"));

    // Build command SQL row
    std::string name = util::sql_escape_string(command_name);
    std::string description = util::sql_escape_string(command.description);
    std::string value = util::sql_escape_string(command.value);
    std::string global;
    if (command.global) {
        global = "true";
    } else {
        global = "false";
    }
    // Add command to database
    char** error_message = {};
    sqlite3_exec(db, (std::string("INSERT INTO text_commands VALUES ('") + name + "', '"
                          + description + "', '" + value + "', '" + global + "');").c_str(),
    nullptr, nullptr, error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << *error_message << std::endl;
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message(std::string("Failed to add command `") + command_name + "` to database."));
        return;
    }

    // Add command to command list
    text_commands.emplace(command_name, command);
    // Build slash command
    dpp::slashcommand slash_command(
        command_name,
        command.description,
        event.owner->me.id
    );
    if (command.global) {
        slash_command.set_dm_permission(true);
        event.owner->global_command_create(slash_command);
    } else {
        event.owner->guild_command_create(slash_command, config["guild_id"]);
    }
    event.edit_original_response(dpp::message(std::string("Command `") + command_name + "` added successfully."));
}

void db_commands::add_embed_command(const dpp::slashcommand_t &event, const nlohmann::json &config, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking();
    // Make sure command name is valid
    std::string command_name = std::get<std::string>(event.get_parameter("command_name"));
    if (!util::valid_command_name(command_name)) {
        event.edit_original_response(dpp::message("Command name can only include lowercase letters, numbers, dash, or underscore, and must be 32 characters or less."));
        return;
    }
    // Make sure command doesn't already exist
    command_search_result status = WAITING;
    event.owner->global_commands_get([&command_name, &status](const dpp::confirmation_callback_t& data) {
        if (data.is_error()) {
            status = ERROR;
        } else {
            for (const auto& command : std::get<dpp::slashcommand_map>(data.value)) {
                if (command.second.name == command_name) {
                    status = COMMAND_FOUND;
                    break;
                }
            }
            if (status == WAITING) status = COMMAND_NOT_FOUND;
        }
    });
    while (status == WAITING) {}
    event.owner->guild_commands_get(config["guild_id"], [&command_name, &status](const dpp::confirmation_callback_t& data) {
        if (data.is_error()) {
            status = ERROR;
        } else {
            for (const auto& command : std::get<dpp::slashcommand_map>(data.value)) {
                if (command.second.name == command_name) {
                    status = COMMAND_FOUND;
                    break;
                }
            }
            if (status == WAITING) status = COMMAND_NOT_FOUND;
        }
    });
    while (status == WAITING) {}
    switch (status) {
        case COMMAND_FOUND:
            event.edit_original_response(dpp::message(std::string("Command `") + command_name + "` already exists."));
            return;
        case ERROR:
            event.edit_original_response(dpp::message("Bot failed to get guild command list."));
            return;
        default:
            break;
    }

    // Build embed_command and SQL row based on passed parameters
    embed_command command;
    struct {
        std::string command_name;
        std::string command_description;
        std::string command_is_global;
        std::string title;
        std::string url;
        std::string description;
        std::string thumbnail;
        std::string image;
        std::string video;
        std::string color;
        std::string timestamp;
        std::string author_name;
        std::string author_url;
        std::string author_icon_url;
        std::string footer_text;
        std::string footer_icon_url;
    } sql_row;
    sql_row.command_name = util::sql_escape_string(command_name, true);
    command.description = std::get<std::string>(event.get_parameter("command_description"));
    sql_row.command_description = util::sql_escape_string(command.description, true);
    command.global = std::get<bool>(event.get_parameter("command_is_global"));
    if (command.global) {
        sql_row.command_is_global = "'true'";
    } else {
        sql_row.command_is_global = "'false'";
    }
    command.embed = dpp::embed();
    try {
        command.embed.set_title(std::get<std::string>(event.get_parameter("title")));
        sql_row.title = util::sql_escape_string(command.embed.title, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.title = "NULL";
    }
    try {
        command.embed.set_url(std::get<std::string>(event.get_parameter("url")));
        sql_row.url = util::sql_escape_string(command.embed.url, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.url = "NULL";
    }
    try {
        command.embed.set_description(std::get<std::string>(event.get_parameter("description")));
        sql_row.description = util::sql_escape_string(command.embed.description, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.description = "NULL";
    }
    try {
        command.embed.set_thumbnail(std::get<std::string>(event.get_parameter("thumbnail")));
        sql_row.thumbnail = util::sql_escape_string(std::get<std::string>(event.get_parameter("thumbnail")), true);
    } catch (const std::bad_variant_access& e) {
        sql_row.thumbnail = "NULL";
    }
    try {
        command.embed.set_image(std::get<std::string>(event.get_parameter("image")));
        sql_row.image = util::sql_escape_string(std::get<std::string>(event.get_parameter("image")), true);
    } catch (const std::bad_variant_access& e) {
        sql_row.image = "NULL";
    }
    try {
        command.embed.set_video(std::get<std::string>(event.get_parameter("video")));
        sql_row.video = util::sql_escape_string(std::get<std::string>(event.get_parameter("video")), true);
    } catch (const std::bad_variant_access& e) {
        sql_row.video = "NULL";
    }
    try {
        command.embed.set_color(std::get<std::int64_t>(event.get_parameter("color")));
        sql_row.color = std::to_string(command.embed.color.value());
    } catch (const std::bad_variant_access& e) {
        sql_row.color = "NULL";
    }
    try {
        command.embed.set_timestamp(std::get<std::int64_t>(event.get_parameter("timestamp")));
        sql_row.timestamp = std::to_string(command.embed.timestamp);
    } catch (const std::bad_variant_access& e) {
        sql_row.timestamp = "NULL";
    }

    dpp::embed_author author;
    try {
        author.name = std::get<std::string>(event.get_parameter("author_name"));
        sql_row.author_name = util::sql_escape_string(author.name, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.author_name = "NULL";
    }
    try {
        author.url = std::get<std::string>(event.get_parameter("author_url"));
        sql_row.author_url = util::sql_escape_string(author.url, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.author_url = "NULL";
    }
    try {
        author.icon_url = std::get<std::string>(event.get_parameter("author_icon_url"));
        sql_row.author_icon_url = util::sql_escape_string(author.icon_url, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.author_icon_url = "NULL";
    }
    command.embed.set_author(author);

    dpp::embed_footer footer;
    try {
        footer.set_text(std::get<std::string>(event.get_parameter("footer_text")));
        sql_row.footer_text = util::sql_escape_string(footer.text, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.footer_text = "NULL";
    }
    try {
        footer.set_icon(std::get<std::string>(event.get_parameter("footer_icon_url")));
        sql_row.footer_icon_url = util::sql_escape_string(footer.icon_url, true);
    } catch (const std::bad_variant_access& e) {
        sql_row.footer_icon_url = "NULL";
    }
    command.embed.set_footer(footer);

    // Add command to database
    std::string sql_query = std::string("INSERT INTO embed_commands VALUES (") + sql_row.command_name + ", "
    + sql_row.command_description + ", " + sql_row.command_is_global + ", " + sql_row.title + ", " + sql_row.url
    + ", " + sql_row.description + ", " + sql_row.thumbnail + ", " + sql_row.image + ", " + sql_row.video
    + ", " + sql_row.color + ", " + sql_row.timestamp + ", " + sql_row.author_name + ", " + sql_row.author_url
    + ", " + sql_row.author_icon_url + ", " + sql_row.footer_text + ", " + sql_row.footer_icon_url + ", NULL);";
    char** error_message = {};
    std::cout << sql_query;
    sqlite3_exec(db, sql_query.c_str(), nullptr, nullptr, error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << *error_message << std::endl;
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message(std::string("Failed to add command `") + command_name + "` to database."));
        return;
    }

    // Add command to command list
    embed_commands.emplace(command_name, command);
    // Build slash command
    dpp::slashcommand slash_command(
        command_name,
        command.description,
        event.owner->me.id
    );
    if (command.global) {
        slash_command.set_dm_permission(true);
        event.owner->global_command_create(slash_command);
    } else {
        event.owner->guild_command_create(slash_command, config["guild_id"]);
    }
    event.edit_original_response(dpp::message(std::string("Command `") + command_name + "` added successfully."));
}

void db_commands::add_embed_command_field(const dpp::slashcommand_t &event, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking();
    // Get existing command
    std::string command_name = std::get<std::string>(event.get_parameter("command_name"));
    embed_command command;
    if (auto command_iterator = embed_commands.find(command_name); command_iterator != embed_commands.end()) {
        command = command_iterator->second;
    } else {
        event.edit_original_response(dpp::message(std::string("Embed-based DB command `") + command_name + "` not found."));
        return;
    }

    // Add field to embed and build SQL row based on passed parameters
    std::string field_title = std::get<std::string>(event.get_parameter("title"));
    std::string field_value = std::get<std::string>(event.get_parameter("value"));
    bool field_inline = std::get<bool>(event.get_parameter("inline"));
    command.embed.add_field(field_title, field_value, field_inline);
    std::string title = util::sql_escape_string(field_title, true);
    std::string value = util::sql_escape_string(field_value, true);
    std::string is_inline;
    if (field_inline) {
        is_inline = "'true'";
    } else {
        is_inline = "'false'";
    }

    // Add field to database
    char** error_message = {};
    sqlite3_exec(db, (std::string("INSERT INTO embed_command_fields VALUES (NULL, ") + title + ", " + value + ", " + is_inline + ");").c_str(), nullptr, nullptr, error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << *error_message << std::endl;
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message(std::string("Failed to edit command `") + command_name + "` in database."));
        return;
    }
    int64_t field_id = sqlite3_last_insert_rowid(db);

    // Get current field IDs from command in DB
    std::string sql_command_name = util::sql_escape_string(command_name, true);
    std::string field_ids = "'";
    error_message = {};
    std::cout << std::string("SELECT fields FROM embed_commands WHERE command_name=") + sql_command_name + ';' << std::endl;
    sqlite3_exec(db, (std::string("SELECT fields FROM embed_commands WHERE command_name=") + sql_command_name + ';').c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto output_string = static_cast<std::string*>(output);
            if (column_values[0] != nullptr) {
                output_string->append(column_values[0]);
            }
            return 0;
        },
    &field_ids, error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << *error_message << std::endl;
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message(std::string("Failed to edit command `") + command_name + "` in database."));
        return;
    }
    // Add field ID to command in DB
    if (field_ids != "'") {
        field_ids += ',';
    }
    field_ids += std::to_string(field_id) + '\'';
    error_message = {};
    std::cout << std::string("UPDATE embed_commands SET fields = ") + field_ids + " WHERE command_name=" + sql_command_name + ';' << std::endl;
    sqlite3_exec(db, (std::string("UPDATE embed_commands SET fields = ") + field_ids + "WHERE command_name=" + sql_command_name + ';').c_str(), nullptr, nullptr, error_message);
    if (error_message != nullptr) {
        std::cout << "[" << dpp::utility::current_date_time() << "] SQL ERROR: " << *error_message << std::endl;
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message(std::string("Failed to edit command `") + command_name + "` in database."));
        return;
    }

    // Add field to command in command list
    embed_commands.insert_or_assign(command_name, command);
    event.edit_original_response(dpp::message(std::string("Command `") + command_name + "` edited successfully."));
}

void db_commands::get_commands(const dpp::slashcommand_t &event, std::unordered_map<std::string, text_command> &text_commands, std::unordered_map<std::string, embed_command> &embed_commands) {
    event.thinking();
    dpp::embed embed = dpp::embed().set_color(0x00A0A0).set_title("Commands in Database:")
    .set_footer("Commands not accessible in DM are on the second column", "");
    for (const auto& [name, command] : text_commands) {
        embed.add_field(name, command.description, command.global);
    }
    for (const auto& [name, command] : embed_commands) {
        embed.add_field(name, command.description, command.global);
    }
    event.edit_original_response(dpp::message(event.command.channel_id, embed));
}