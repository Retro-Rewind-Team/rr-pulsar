#!/usr/bin/env bash

set -euo pipefail

function download() {
    local url="$1" file="$2" sha256="$3"
    curl -fL --retry 3 -o "$file" "$url"
    echo "$sha256  $file" | sha256sum -c -
}

KAMEK_URL="https://github.com/Treeki/Kamek/releases/download/2026-02-24/linux-x64.tar.gz"
KAMEK_SHA256="48e0b095c9774abae95caa965584f1cd231a52bb23d3c425742649aa7f768b5b"
KAMEK_FILE="kamek-linux-x64.tar.gz"
KAMEK_ROOT="./Kamek"
KAMEK="$KAMEK_ROOT/Kamek"

function download_kamek() {
    echo "Downloading Kamek"
    download "$KAMEK_URL" "$KAMEK_FILE" "$KAMEK_SHA256"
    mkdir -p "$KAMEK_ROOT"

    echo "Extracting $KAMEK_FILE"
    tar xf "$KAMEK_FILE" -C "$KAMEK_ROOT"
    chmod u+x "$KAMEK"
}

WIBO_URL="https://github.com/decompals/wibo/releases/download/1.2.0/wibo-x86_64"
WIBO_SHA256="13f86a2d618f0dbe67179d349625345eabf9b46450295cb4c904e49f6aff85af"
WIBO="./wibo"

function download_wibo() {
    echo "Downloading wibo"
    download "$WIBO_URL" "$WIBO" "$WIBO_SHA256"
    chmod u+x "$WIBO"
}

CW_URL="https://files.decomp.dev/compilers_20251118.zip"
CW_SHA256="69b0d4980a2eccea4ae6e70b486714b5190cb6c75e3bc7b8fb5878a8854d0d1c"
CW_FILE="compilers.zip"
CW_VERSION="Wii/1.7"
CW_ROOT="./mwcw"
MWCCEPPC="$CW_ROOT/mwcceppc.exe"
MWASMEPPC="$CW_ROOT/mwasmeppc.exe"

function download_mwcw() {
    echo "Downloading Metrowerks CodeWarrior"
    download "$CW_URL" "$CW_FILE" "$CW_SHA256"
    mkdir -p "$CW_ROOT"

    echo "Extracting $CW_FILE"
    unzip -joq "$CW_FILE" "$CW_VERSION/*" -d "$CW_ROOT"
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
