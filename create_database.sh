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
"