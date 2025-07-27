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
#include "guild.h"
#include "../util.h"

void guild::on_invite_created(const dpp::invite_create_t &event, const nlohmann::json& config, std::vector<dpp::invite>& invites) {
    invites.push_back(event.created_invite);
    // Don't log invites without a known creator
    if (event.created_invite.inviter_id == 0) {
        return;
    }

    dpp::embed embed = dpp::embed().set_color(util::color::GREEN).set_thumbnail(event.created_invite.inviter.get_avatar_url())
                                   .set_title("Invite Created")
                                   .add_field("Invite Creator", event.created_invite.inviter.username, true)
                                   .add_field("User ID", event.created_invite.inviter.id.str(), true)
                                   .add_field("Invite Code", event.created_invite.code, false);
    if (event.created_invite.channel_id != 0) {
        embed.add_field("Invite Channel", event.created_invite.destination_channel.get_mention(), true);
    }
    if (event.created_invite.expires_at == 0) {
        embed.add_field("Expires", "Never", true);
    } else {
        embed.add_field("Expires", std::format("<t:{}:R>", event.created_invite.expires_at), true);
    }
    event.owner->message_create(dpp::message(config["log_channel_ids"]["invites"], embed));
}

void guild::on_invite_deleted(const dpp::invite_delete_t &event, const nlohmann::json& config, std::vector<dpp::invite>& invites) {
    dpp::embed embed = dpp::embed().set_color(util::color::RED).set_title("Invite Deleted");
    // Find invite in cache
    auto invite = invites.begin();
    while (invite != invites.end() && invite->code != event.deleted_invite.code) ++invite;
    if (invite == invites.end()) {
        embed.add_field("Invite Code", event.deleted_invite.code, false);
        embed.set_footer(dpp::embed_footer().set_text("Invite not found in cache; limited info available."));
    } else {
        embed.add_field("Invite Creator", invite->inviter.username, true)
             .add_field("User ID", invite->inviter.id.str(), true)
             .add_field("Invite Code", invite->code, false)
             .add_field("Uses", std::to_string(invite->uses), true);
        if (invite->channel_id != 0) {
            embed.add_field("Invite Channel", invite->destination_channel.get_mention(), true);
        }
        // Delete invite from cache
        invites.erase(invite);
    }
    // Don't log invites without a known creator
    if (!embed.fields[0].value.empty()) {
        event.owner->message_create(dpp::message(config["log_channel_ids"]["invites"], embed));
    }
}