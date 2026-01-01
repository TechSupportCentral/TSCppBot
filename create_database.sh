#!/bin/sh

sqlite3 TSCppBot.db "
CREATE TABLE text_commands(
    name TEXT PRIMARY KEY,
    description TEXT,
    value TEXT,
    is_global TEXT
) WITHOUT ROWID;

CREATE TABLE embed_command_fields(
    id INTEGER PRIMARY KEY ASC,
    title TEXT,
    value TEXT,
    is_inline TEXT
);

CREATE TABLE embed_commands(
    command_name TEXT PRIMARY KEY,
    command_description TEXT,
    command_is_global TEXT,
    title TEXT,
    url TEXT,
    description TEXT,
    thumbnail TEXT,
    image TEXT,
    video TEXT,
    color INTEGER,
    timestamp INTEGER,
    author_name TEXT,
    author_url TEXT,
    author_icon_url TEXT,
    footer_text TEXT,
    footer_icon_url TEXT,
    fields TEXT
) WITHOUT ROWID;

CREATE TABLE reminders(
    id INTEGER PRIMARY KEY ASC,
    start_time INTEGER,
    end_time INTEGER,
    user TEXT,
    text TEXT
) STRICT;

CREATE TABLE mutes(
    id INTEGER PRIMARY KEY ASC,
    start_time INTEGER,
    end_time INTEGER
);

CREATE TABLE mod_records(
    id TEXT PRIMARY KEY,
    type TEXT,
    moderator TEXT,
    user TEXT,
    reason TEXT,
    active TEXT,
    extra_data INTEGER
) WITHOUT ROWID;
CREATE TABLE staff_applications(
    id TEXT,
    time INTEGER,
    type TEXT,
    status TEXT,
    q1 TEXT,
    q2 TEXT,
    q3 TEXT,
    q4 TEXT,
    q5 TEXT,
    q6 TEXT,
    q7 TEXT,
    q8 TEXT,
    q9 TEXT,
    q10 TEXT
);
CREATE TABLE ban_appeals(
    id TEXT,
    email TEXT,
    time INTEGER,
    status TEXT,
    reason TEXT,
    appeal TEXT
);
"
# mod_records extra_data is currently either the number of seconds to delete messages for in a ban, or a mutes table row ID.