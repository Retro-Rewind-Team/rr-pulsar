#!/usr/bin/env bash

set -euo pipefail

KAMEK_URL="https://github.com/Treeki/Kamek/releases/download/2026-02-24/linux-x64.tar.gz"
KAMEK_FILE="kamek-linux-x64.tar.gz"
KAMEK_ROOT="./Kamek"
KAMEK="$KAMEK_ROOT/Kamek"

function download_kamek() {
    echo "Downloading Kamek"
    curl -Lo "$KAMEK_FILE" "$KAMEK_URL"
    mkdir -p "$KAMEK_ROOT"

    echo "Extracting $KAMEK_FILE"
    tar xf "$KAMEK_FILE" -C "$KAMEK_ROOT"
    chmod u+x "$KAMEK"
}

WIBO_URL="https://github.com/decompals/wibo/releases/download/1.2.0/wibo-x86_64"
WIBO="./wibo"

function download_wibo() {
    echo "Downloading wibo"
    curl -Lo "$WIBO" "$WIBO_URL"
    chmod u+x "$WIBO"
}

CW_URL="https://ppeb.me/files/mwcw.tar.gz"
CW_FILE="mwcw.tar.gz"
CW_ROOT="./mwcw"
MWCCEPPC="$CW_ROOT/mwcceppc.exe"
MWASMEPPC="$CW_ROOT/mwasmeppc.exe"

function download_mwcw() {
    echo "Downloading Metrowerks CodeWarrior"
    curl -Lo "$CW_FILE" "$CW_URL"
    mkdir -p "$CW_ROOT"

    echo "Extracting $CW_FILE"
    tar xf "$CW_FILE" -C "$CW_ROOT"
}

function create_env() {
    echo "Creating environment"
    {
        echo "CC=$WIBO $MWCCEPPC"
        echo "AS=$WIBO $MWASMEPPC"
        echo "KAMEK=$KAMEK"
        echo "SECURE=../rr-secure"
    } >>.env
}

download_kamek
download_wibo
download_mwcw
create_env
