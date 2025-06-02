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
    extra_data_id INTEGER
) WITHOUT ROWID;
"