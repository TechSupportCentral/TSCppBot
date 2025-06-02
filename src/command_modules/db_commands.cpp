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
#include <sstream>

void db_commands::add_text_command_modal(const dpp::slashcommand_t &event) {
    event.dialog(dpp::interaction_modal_response("add_text_command_form", "Add a text-based DB command")
        .add_component(dpp::component()
            .set_label("Command Name")
            .set_id("add_text_command_name")
            .set_type(dpp::cot_text)
            .set_min_length(1)
            .set_max_length(32)
            .set_placeholder("Type command name here")
            .set_default_value("")
            .set_text_style(dpp::text_short)
        ).add_row()
        .add_component(dpp::component()
            .set_label("Command Description")
            .set_id("add_text_command_description")
            .set_type(dpp::cot_text)
            .set_min_length(1)
            .set_max_length(100)
            .set_placeholder("Type command description here")
            .set_default_value("")
            .set_text_style(dpp::text_short)
        ).add_row()
        .add_component(dpp::component()
            .set_label("Command Content")
            .set_id("add_text_command_content")
            .set_type(dpp::cot_text)
            .set_min_length(1)
            .set_max_length(2000)
            .set_placeholder("What does the bot say?")
            .set_default_value("")
            .set_text_style(dpp::text_paragraph)
        ).add_row()
        .add_component(dpp::component()
            .set_label("Make command global?")
            .set_id("add_text_command_global")
            .set_type(dpp::cot_text)
            .set_min_length(4)
            .set_max_length(5)
            .set_placeholder("Enter 'true' to make command accessible outside server, 'false' otherwise")
            .set_default_value("")
            .set_text_style(dpp::text_short)
        )
    );
}

dpp::task<> db_commands::add_text_command(const dpp::form_submit_t &event, const nlohmann::json &config, std::unordered_map<std::string, text_command> &text_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Make sure command name is valid
    std::string command_name = std::get<std::string>(event.components[0].components[0].value);
    if (!util::is_valid_command_name(command_name)) {
        co_await thinking;
        event.edit_original_response(dpp::message("Command name can only include lowercase letters, numbers, dash, or underscore, and must not start with a number."));
        co_return;
    }
    // Make sure command doesn't already exist
    std::pair<util::command_search_result, dpp::snowflake> search_result = co_await util::find_command(event.owner, config, command_name);
    switch (search_result.first) {
        case util::GLOBAL_COMMAND_FOUND:
        case util::GUILD_COMMAND_FOUND:
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Command `{}` already exists.", command_name)));
            co_return;
        case util::SEARCH_ERROR:
            co_await thinking;
            event.edit_original_response(dpp::message("Bot failed to get command list."));
            co_return;
        default:
            break;
    }

    // Build text_command based on passed parameters
    text_command command;
    command.description = std::get<std::string>(event.components[1].components[0].value);
    command.value = std::get<std::string>(event.components[2].components[0].value);
    command.global = (std::get<std::string>(event.components[3].components[0].value) == "true");

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
    char* error_message;
    sqlite3_exec(db, std::format("INSERT INTO text_commands VALUES ('{}', '{}', '{}', '{}');",
    name, description, value, global).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Failed to add command `{}` to database.", command_name)));
        co_return;
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
    // Send log
    dpp::embed embed = dpp::embed().set_color(dpp::colors::green).set_title("Database Command Added")
    .set_thumbnail(event.command.member.get_avatar_url()).add_field("Added by", event.command.member.get_nickname(), false)
    .add_field("Command name", command_name, true).add_field("Command type", "Text", true);
    event.owner->message_create(dpp::message(config["log_channel_ids"]["staff_news"], embed));
    // Send confirmation
    co_await thinking;
    event.edit_original_response(dpp::message(std::format("Command `{}` added successfully.", command_name)));
}

dpp::task<> db_commands::add_embed_command(const dpp::slashcommand_t &event, const nlohmann::json &config, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    // Make sure command name is valid
    std::string command_name = std::get<std::string>(event.get_parameter("command_name"));
    if (!util::is_valid_command_name(command_name)) {
        co_await thinking;
        event.edit_original_response(dpp::message("Command name can only include lowercase letters, numbers, dash, or underscore, and must not start with a number."));
        co_return;
    }
    // Make sure command doesn't already exist
    std::pair<util::command_search_result, dpp::snowflake> search_result = co_await util::find_command(event.owner, config, command_name);
    switch (search_result.first) {
        case util::GLOBAL_COMMAND_FOUND:
        case util::GUILD_COMMAND_FOUND:
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Command `{}` already exists.", command_name)));
            co_return;
        case util::SEARCH_ERROR:
            co_await thinking;
            event.edit_original_response(dpp::message("Bot failed to get command list."));
            co_return;
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
    } catch (const std::bad_variant_access&) {
        sql_row.title = "NULL";
    }
    try {
        command.embed.set_url(std::get<std::string>(event.get_parameter("url")));
        sql_row.url = util::sql_escape_string(command.embed.url, true);
    } catch (const std::bad_variant_access&) {
        sql_row.url = "NULL";
    }
    try {
        command.embed.set_description(std::get<std::string>(event.get_parameter("description")));
        sql_row.description = util::sql_escape_string(command.embed.description, true);
    } catch (const std::bad_variant_access&) {
        sql_row.description = "NULL";
    }
    try {
        command.embed.set_thumbnail(std::get<std::string>(event.get_parameter("thumbnail")));
        sql_row.thumbnail = util::sql_escape_string(std::get<std::string>(event.get_parameter("thumbnail")), true);
    } catch (const std::bad_variant_access&) {
        sql_row.thumbnail = "NULL";
    }
    try {
        command.embed.set_image(std::get<std::string>(event.get_parameter("image")));
        sql_row.image = util::sql_escape_string(std::get<std::string>(event.get_parameter("image")), true);
    } catch (const std::bad_variant_access&) {
        sql_row.image = "NULL";
    }
    try {
        command.embed.set_video(std::get<std::string>(event.get_parameter("video")));
        sql_row.video = util::sql_escape_string(std::get<std::string>(event.get_parameter("video")), true);
    } catch (const std::bad_variant_access&) {
        sql_row.video = "NULL";
    }
    try {
        command.embed.set_color(std::get<std::int64_t>(event.get_parameter("color")));
        sql_row.color = std::to_string(command.embed.color.value());
    } catch (const std::bad_variant_access&) {
        sql_row.color = "NULL";
    }
    try {
        command.embed.set_timestamp(std::get<std::int64_t>(event.get_parameter("timestamp")));
        sql_row.timestamp = std::to_string(command.embed.timestamp);
    } catch (const std::bad_variant_access&) {
        sql_row.timestamp = "NULL";
    }

    dpp::embed_author author;
    try {
        author.name = std::get<std::string>(event.get_parameter("author_name"));
        sql_row.author_name = util::sql_escape_string(author.name, true);
    } catch (const std::bad_variant_access&) {
        sql_row.author_name = "NULL";
    }
    try {
        author.url = std::get<std::string>(event.get_parameter("author_url"));
        sql_row.author_url = util::sql_escape_string(author.url, true);
    } catch (const std::bad_variant_access&) {
        sql_row.author_url = "NULL";
    }
    try {
        author.icon_url = std::get<std::string>(event.get_parameter("author_icon_url"));
        sql_row.author_icon_url = util::sql_escape_string(author.icon_url, true);
    } catch (const std::bad_variant_access&) {
        sql_row.author_icon_url = "NULL";
    }
    command.embed.set_author(author);

    dpp::embed_footer footer;
    try {
        footer.set_text(std::get<std::string>(event.get_parameter("footer_text")));
        sql_row.footer_text = util::sql_escape_string(footer.text, true);
    } catch (const std::bad_variant_access&) {
        sql_row.footer_text = "NULL";
    }
    try {
        footer.set_icon(std::get<std::string>(event.get_parameter("footer_icon_url")));
        sql_row.footer_icon_url = util::sql_escape_string(footer.icon_url, true);
    } catch (const std::bad_variant_access&) {
        sql_row.footer_icon_url = "NULL";
    }
    command.embed.set_footer(footer);

    // Add command to database
    char* error_message;
    sqlite3_exec(db, std::format("INSERT INTO embed_commands VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, NULL);",
    sql_row.command_name, sql_row.command_description, sql_row.command_is_global, sql_row.title, sql_row.url, sql_row.description,
    sql_row.thumbnail, sql_row.image, sql_row.video, sql_row.color, sql_row.timestamp, sql_row.author_name, sql_row.author_url,
    sql_row.author_icon_url, sql_row.footer_text, sql_row.footer_icon_url).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        co_await thinking;
        event.edit_original_response(dpp::message(std::string("Failed to add command `") + command_name + "` to database."));
        co_return;
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
    // Send log
    dpp::embed embed = dpp::embed().set_color(dpp::colors::green).set_title("Database Command Added")
    .set_thumbnail(event.command.member.get_avatar_url()).add_field("Added by", event.command.member.get_nickname(), false)
    .add_field("Command name", command_name, true).add_field("Command type", "Embed", true);
    event.owner->message_create(dpp::message(config["log_channel_ids"]["staff_news"], embed));
    // Send confirmation
    co_await thinking;
    event.edit_original_response(dpp::message(std::format("Command `{}` added successfully.", command_name)));
}

void db_commands::add_embed_command_field_modal(const dpp::slashcommand_t &event, const std::unordered_map<std::string, embed_command> &embed_commands) {
    // Make sure command exists
    std::string command_name = std::get<std::string>(event.get_parameter("command_name"));
    if (!embed_commands.contains(command_name)) {
        event.reply(dpp::message(std::format("Embed-based DB command `{}` not found.", command_name)).set_flags(dpp::m_ephemeral));
        return;
    }

    event.dialog(dpp::interaction_modal_response(std::string("add_field_form,") + command_name, std::string("Add a field in ") + command_name)
        .add_component(dpp::component()
            .set_label("Field Title")
            .set_id("add_field_title")
            .set_type(dpp::cot_text)
            .set_min_length(1)
            .set_max_length(256)
            .set_placeholder("Type name of field here")
            .set_default_value("")
            .set_text_style(dpp::text_short)
        ).add_row()
        .add_component(dpp::component()
            .set_label("Field Value")
            .set_id("add_field_value")
            .set_type(dpp::cot_text)
            .set_min_length(1)
            .set_max_length(1024)
            .set_placeholder("Type text of field here")
            .set_default_value("")
            .set_text_style(dpp::text_paragraph)
        ).add_row()
        .add_component(dpp::component()
            .set_label("Make field inline?")
            .set_id("add_field_inline")
            .set_type(dpp::cot_text)
            .set_min_length(4)
            .set_max_length(5)
            .set_placeholder("Enter 'true' to place field in line with previous field, 'false' otherwise")
            .set_default_value("")
            .set_text_style(dpp::text_short)
        )
    );
}

void db_commands::add_embed_command_field(const dpp::form_submit_t &event, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking();
    // Get existing command
    std::string command_name = event.custom_id.substr(15);
    embed_command command;
    if (auto command_iterator = embed_commands.find(command_name); command_iterator != embed_commands.end()) {
        command = command_iterator->second;
    } else {
        event.edit_original_response(dpp::message(std::format("Embed-based DB command `{}` not found.", command_name)));
        return;
    }

    // Get current field IDs from command in DB
    std::string sql_command_name = util::sql_escape_string(command_name, true);
    std::string field_ids = "'";
    char* error_message;
    sqlite3_exec(db, std::format("SELECT fields FROM embed_commands WHERE command_name={};", sql_command_name).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto output_string = static_cast<std::string*>(output);
            if (column_values[0] != nullptr) {
                output_string->append(column_values[0]);
            }
            return 0;
        },
    &field_ids, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message(std::format("Failed to find command `{}` in database.", command_name)));
        return;
    }
    // Make sure we have not reached the max number of fields for a command
    if (std::ranges::count(field_ids, ',') >= 24) {
        event.edit_original_response(dpp::message(std::format("Command `{}` currently has the maximum of 25 fields.", command_name)));
        return;
    }

    // Add field to embed and build SQL row based on passed parameters
    std::string field_title = std::get<std::string>(event.components[0].components[0].value);
    std::string field_value = std::get<std::string>(event.components[1].components[0].value);
    bool field_inline = (std::get<std::string>(event.components[1].components[0].value) == "true");
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
    sqlite3_exec(db, std::format("INSERT INTO embed_command_fields VALUES (NULL, {}, {}, {});",
    title, value, is_inline).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to add field to database."));
        return;
    }
    int64_t field_id = sqlite3_last_insert_rowid(db);

    // Add field ID to command in DB
    if (field_ids != "'") {
        field_ids += ',';
    }
    field_ids += std::to_string(field_id) + '\'';
    sqlite3_exec(db, std::format("UPDATE embed_commands SET fields = {} WHERE command_name={};",
    field_ids, sql_command_name).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message(std::format("Failed to edit command `{}` in database.", command_name)));
        return;
    }

    // Add field to command in command list
    embed_commands.insert_or_assign(command_name, command);
    event.edit_original_response(dpp::message(std::format("Command `{}` edited successfully.", command_name)));
}

void db_commands::remove_embed_command_field_menu(const dpp::slashcommand_t &event, std::unordered_map<std::string, embed_command> const &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking();
    // Make sure command exists
    std::string command_name = std::get<std::string>(event.get_parameter("command_name"));
    if (!embed_commands.contains(command_name)) {
        event.edit_original_response(dpp::message(std::format("Embed-based DB command `{}` not found.", command_name)));
        return;
    }

    // Get current field IDs from command in DB
    std::string field_ids;
    char* error_message;
    sqlite3_exec(db, std::format("SELECT fields FROM embed_commands WHERE command_name={};", util::sql_escape_string(command_name, true)).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto field_ids = static_cast<std::string*>(output);
            if (column_values[0] != nullptr) {
                field_ids->append(column_values[0]);
            }
            return 0;
        },
    &field_ids, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to get field from database."));
        return;
    }

    // Construct Discord select menu component with field titles
    std::map<std::string, std::string> fields;
    sqlite3_exec(db, std::format("SELECT id, title FROM embed_command_fields WHERE id IN ({});", field_ids).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto output_map = static_cast<std::map<std::string, std::string>*>(output);
            output_map->emplace(column_values[0], column_values[1]);
            return 0;
        },
    &fields, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to get field from database."));
        return;
    }
    if (fields.empty()) {
        event.edit_original_response(dpp::message(std::format("Embed for command `{}` has no embeds.", command_name)));
        return;
    }
    dpp::component select_menu = dpp::component().set_type(dpp::cot_selectmenu)
    .set_placeholder("Select a field to remove").set_id("remove_field_select");
    for (const auto& [id, title] : fields) {
        select_menu.add_select_option(dpp::select_option(title, id + command_name));
    }
    event.edit_original_response(dpp::message().add_component(dpp::component().add_component(select_menu)));
}

void db_commands::remove_embed_command_field(const dpp::select_click_t &event, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.reply(dpp::ir_deferred_update_message, "");
    // Get context info from selection value
    int64_t field_id;
    std::string command_name;
    std::istringstream context(event.values[0]);
    context >> field_id;
    context >> command_name;
    embed_command command = embed_commands.find(command_name)->second;
    std::vector<int64_t> field_ids;
    // Get field IDs
    char* error_message;
    sqlite3_exec(db, std::format("SELECT fields FROM embed_commands WHERE command_name={};", util::sql_escape_string(command_name, true)).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto output_vec = static_cast<std::vector<int64_t>*>(output);
            if (column_values[0] != nullptr) {
                std::istringstream field_ids(column_values[0]);
                std::string id;
                while (std::getline(field_ids, id, ',')) {
                    output_vec->push_back(std::stoll(id));
                }
            }
            return 0;
        },
    &field_ids, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_response("Failed to get field from database.");
        return;
    }

    // Make sure there are still fields left
    if (field_ids.empty()) {
        event.edit_response(std::format("Embed for command `{}` has no fields.", command_name));
        return;
    }
    // Make sure this field hasn't already been removed
    int field_index = -1;
    for (int i = 0; i < field_ids.size(); i++) {
        if (field_ids[i] == field_id) {
            field_index = i;
        }
    }
    if (field_index == -1) {
        event.edit_response("This field was already removed.");
        return;
    }
    // Remove this field from the list
    field_ids.erase(field_ids.begin() + field_index);

    // Remove field from database
    sqlite3_exec(db, std::format("DELETE FROM embed_command_fields WHERE id={};", field_id).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_response("Failed to remove field from database.");
        return;
    }

    // Update fields list for command in DB
    std::string new_field_ids;
    if (field_ids.empty()) {
        new_field_ids = "NULL";
    } else {
        new_field_ids += '\'';
        for (int i = 0; i < field_ids.size() - 1; i++) {
            new_field_ids += std::to_string(field_ids[i]) + ',';
        }
        new_field_ids += std::to_string(field_ids.back()) + '\'';
    }
    sqlite3_exec(db, std::format("UPDATE embed_commands SET fields = {} WHERE command_name={};",
    new_field_ids, util::sql_escape_string(command_name, true)).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_response(std::format("Failed to edit command `{}` in database.", command_name));
        return;
    }

    // Remove field from command in command list
    command.embed.fields.erase(command.embed.fields.begin() + field_index);
    embed_commands.insert_or_assign(command_name, command);
    event.edit_response(std::format("Command `{}` edited successfully.", command_name));
}

void db_commands::edit_embed_command_field_menu(const dpp::slashcommand_t &event, const std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.thinking();
    // Make sure command exists
    std::string command_name = std::get<std::string>(event.get_parameter("command_name"));
    if (!embed_commands.contains(command_name)) {
        event.edit_original_response(dpp::message(std::format("Embed-based DB command `{}` not found.", command_name)));
        return;
    }

    // Get current field IDs from command in DB
    std::string field_ids;
    char* error_message;
    sqlite3_exec(db, std::format("SELECT fields FROM embed_commands WHERE command_name={};", util::sql_escape_string(command_name, true)).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto field_ids = static_cast<std::string*>(output);
            if (column_values[0] != nullptr) {
                field_ids->append(column_values[0]);
            }
            return 0;
        },
    &field_ids, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to get field from database."));
        return;
    }

    // Construct Discord select menu component with field titles
    std::map<std::string, std::string> fields;
    sqlite3_exec(db, std::format("SELECT id, title FROM embed_command_fields WHERE id IN ({});", field_ids).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto output_map = static_cast<std::map<std::string, std::string>*>(output);
            output_map->emplace(column_values[0], column_values[1]);
            return 0;
        },
    &fields, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_original_response(dpp::message("Failed to get field from database."));
        return;
    }
    if (fields.empty()) {
        event.edit_original_response(dpp::message(std::format("Embed for command `{}` has no fields.", command_name)));
        return;
    }
    dpp::component select_menu = dpp::component().set_type(dpp::cot_selectmenu)
    .set_placeholder("Select field to edit").set_id("edit_field_select");
    for (const auto& [id, title] : fields) {
        select_menu.add_select_option(dpp::select_option(title, id + command_name));
    }
    event.edit_original_response(dpp::message().add_component(dpp::component().add_component(select_menu)));
}

void db_commands::edit_embed_command_field_modal(const dpp::select_click_t &event, sqlite3 *db) {
    // Get context vars
    int64_t field_id;
    std::string command_name;
    std::istringstream context(event.values[0]);
    context >> field_id;
    context >> command_name;

    // Get selected field info
    char* error_message;
    std::string field[3];
    sqlite3_exec(db, std::format("SELECT title, value, is_inline FROM embed_command_fields WHERE id = {};", field_id).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto field = static_cast<std::string*>(output);
            field[0] = column_values[0];
            field[1] = column_values[1];
            field[2] = column_values[2];
            return 0;
        },
    field, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.reply("Failed to get field from database.");
        return;
    }

    event.dialog(dpp::interaction_modal_response(std::format("edit_field_form{}{}", field_id, command_name), std::string("Edit a field in ") + command_name)
        .add_component(dpp::component()
            .set_label("New Field Title")
            .set_id("edit_field_title")
            .set_type(dpp::cot_text)
            .set_min_length(1)
            .set_max_length(256)
            .set_default_value(field[0])
            .set_text_style(dpp::text_short)
        ).add_row()
        .add_component(dpp::component()
            .set_label("New Field Value")
            .set_id("edit_field_value")
            .set_type(dpp::cot_text)
            .set_min_length(1)
            .set_max_length(1024)
            .set_default_value(field[1])
            .set_text_style(dpp::text_paragraph)
        ).add_row()
        .add_component(dpp::component()
            .set_label("Make field inline?")
            .set_id("edit_field_inline")
            .set_type(dpp::cot_text)
            .set_min_length(4)
            .set_max_length(5)
            .set_placeholder(std::string("Enter 'true' or 'false' (field is currently ") + field[2] + ')')
            .set_default_value("")
            .set_text_style(dpp::text_short)
        )
    );
}

void db_commands::edit_embed_command_field(const dpp::form_submit_t &event, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    event.reply(dpp::ir_deferred_update_message, "");
    // Get context info
    std::istringstream context(event.custom_id.substr(15));
    int64_t field_id;
    std::string command_name;
    context >> field_id;
    context >> command_name;
    embed_command command = embed_commands.find(command_name)->second;
    // Get values from modal
    std::string title = std::get<std::string>(event.components[0].components[0].value);
    std::string value = std::get<std::string>(event.components[1].components[0].value);
    std::string is_inline;
    if (std::get<std::string>(event.components[2].components[0].value) == "true") {
        is_inline = "'true'";
    } else {
        is_inline = "'false'";
    }

    // Edit field info in DB
    char* error_message;
    sqlite3_exec(db, std::format("UPDATE embed_command_fields SET title = {}, value = {}, is_inline = {} WHERE id={};",
    util::sql_escape_string(title, true), util::sql_escape_string(value, true),
    is_inline, field_id).c_str(), nullptr, nullptr, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_response(std::format("Failed to edit command `{}` in database.", command_name));
        return;
    }

    std::vector<int64_t> field_ids;
    // Get field IDs for command
    sqlite3_exec(db, std::format("SELECT fields FROM embed_commands WHERE command_name={};", util::sql_escape_string(command_name, true)).c_str(),
        [](void* output, int column_count, char** column_values, char** column_names) -> int {
            auto output_vec = static_cast<std::vector<int64_t>*>(output);
            if (column_values[0] != nullptr) {
                std::istringstream field_ids(column_values[0]);
                std::string id;
                while (std::getline(field_ids, id, ',')) {
                    output_vec->push_back(std::stoll(id));
                }
            }
            return 0;
        },
    &field_ids, &error_message);
    if (error_message != nullptr) {
        util::log("SQL ERROR", error_message);
        sqlite3_free(error_message);
        event.edit_response("Failed to get field from database.");
        return;
    }
    // Get index of field to update existing command
    int field_index = -1;
    for (int i = 0; i < field_ids.size(); i++) {
        if (field_ids[i] == field_id) {
            field_index = i;
        }
    }
    if (field_index == -1) {
        event.edit_response(std::format("Could not find field in command `{}`.", command_name));
        return;
    }

    // Update field inside command in command list
    command.embed.fields[field_index].name = title;
    command.embed.fields[field_index].value = value;
    command.embed.fields[field_index].is_inline = (is_inline == "'true'");
    embed_commands.insert_or_assign(command_name, command);
    event.edit_response(std::format("Command `{}` edited successfully.", command_name));
}

dpp::task<> db_commands::remove_command(const dpp::slashcommand_t &event, const nlohmann::json &config, std::unordered_map<std::string, text_command> &text_commands, std::unordered_map<std::string, embed_command> &embed_commands, sqlite3 *db) {
    // Send "thinking" response to allow time for DB operation
    dpp::async thinking = event.co_thinking(true);
    std::string command_name = std::get<std::string>(event.get_parameter("command_name"));

    // Get existing command
    auto text_command_it = text_commands.find(command_name);
    if (text_command_it != text_commands.end()) {
        // Remove command from database
        char* error_message;
        sqlite3_exec(db, std::format("DELETE FROM text_commands WHERE name={};",
        util::sql_escape_string(command_name, true)).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Failed to remove command `{}` from database.", command_name)));
            co_return;
        }

        // Remove slash command
        std::pair<util::command_search_result, dpp::snowflake> search_result = co_await util::find_command(event.owner, config, command_name);
        if (search_result.first == util::GLOBAL_COMMAND_FOUND) {
            event.owner->global_command_delete(search_result.second);
        } else if (search_result.first == util::GUILD_COMMAND_FOUND) {
            event.owner->guild_command_delete(search_result.second, config["guild_id"]);
        } else {
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Failed to remove command `{}` from Discord.", command_name)));
            co_return;
        }
        text_commands.erase(text_command_it);

        // Send log
        dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_title("Database Command Removed")
        .set_thumbnail(event.command.member.get_avatar_url()).add_field("Removed by", event.command.member.get_nickname(), false)
        .add_field("Command name", command_name, true).add_field("Command type", "Text", true);
        event.owner->message_create(dpp::message(config["log_channel_ids"]["staff_news"], embed));
        // Send confirmation
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Command `{}` removed successfully.", command_name)));
        co_return;
    }
    auto embed_command_it = embed_commands.find(command_name);
    if (embed_command_it != embed_commands.end()) {
        // Get fields that are part of this embed command
        std::string fields;
        char* error_message;
        sqlite3_exec(db, std::format("SELECT fields FROM embed_commands WHERE command_name={};",
        util::sql_escape_string(command_name, true)).c_str(),
            [](void* output, int column_count, char** column_values, char** column_names) -> int {
                auto fields = static_cast<std::string*>(output);
                if (column_values[0] != nullptr) {
                    fields->append(column_values[0]);
                }
                return 0;
            },
        &fields, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Failed to remove command `{}` from database.", command_name)));
            co_return;
        }
        // Remove fields from database
        sqlite3_exec(db, std::format("DELETE FROM embed_command_fields WHERE id IN ({});", fields).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Failed to remove command `{}` from database.", command_name)));
            co_return;
        }

        // Remove command from database
        sqlite3_exec(db, std::format("DELETE FROM embed_commands WHERE command_name={};",
        util::sql_escape_string(command_name, true)).c_str(), nullptr, nullptr, &error_message);
        if (error_message != nullptr) {
            util::log("SQL ERROR", error_message);
            sqlite3_free(error_message);
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Failed to remove command `{}` from database.", command_name)));
            co_return;
        }

        // Remove slash command
        std::pair<util::command_search_result, dpp::snowflake> search_result = co_await util::find_command(event.owner, config, command_name);
        if (search_result.first == util::GLOBAL_COMMAND_FOUND) {
            event.owner->global_command_delete(search_result.second);
        } else if (search_result.first == util::GUILD_COMMAND_FOUND) {
            event.owner->guild_command_delete(search_result.second, config["guild_id"]);
        } else {
            co_await thinking;
            event.edit_original_response(dpp::message(std::format("Failed to remove command `{}` from Discord.", command_name)));
            co_return;
        }
        embed_commands.erase(embed_command_it);

        // Send log
        dpp::embed embed = dpp::embed().set_color(dpp::colors::red).set_title("Database Command Removed")
        .set_thumbnail(event.command.member.get_avatar_url()).add_field("Removed by", event.command.member.get_nickname(), false)
        .add_field("Command name", command_name, true).add_field("Command type", "Embed", true);
        event.owner->message_create(dpp::message(config["log_channel_ids"]["staff_news"], embed));
        // Send confirmation
        co_await thinking;
        event.edit_original_response(dpp::message(std::format("Command `{}` removed successfully.", command_name)));
        co_return;
    }
    // Let user know if command doesn't exist
    co_await thinking;
    event.edit_original_response(dpp::message(std::format("Database command `{}` not found.", command_name)));
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