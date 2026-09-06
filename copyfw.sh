#!/bin/bash

set -euo pipefail

PIO_BUILD="${PIO_BUILD:-.pio/build/STM32F103RE_creality}"
TARGET="${TARGET:-/media/kaedenn/1E21-CFAA}"

if [[ ! -d "$TARGET" ]]; then
  echo "ERROR: Printer's SD Card not present" >&2
  exit 1
fi

YYMMDD="$(date +%y%m%d)"
fwname="fw${YYMMDD}.bin"

# Determine the source firmware file.
if [[ $# -gt 0 ]]; then
    src="$1"

    # Allow either an explicit path or a filename within $PIO_BUILD.
    if [[ ! -f "$src" && -f "$PIO_BUILD/$src" ]]; then
        src="$PIO_BUILD/$src"
    fi
else
    # Find the most recently modified .bin file.
    shopt -s nullglob
    bins=("$PIO_BUILD"/*.bin)
    shopt -u nullglob

    if (( ${#bins[@]} == 0 )); then
        echo "ERROR: No .bin files found in $PIO_BUILD" >&2
        exit 1
    fi

    src="$(ls -1t -- "${bins[@]}" | head -n 1)"
fi

if [[ ! -f "$src" ]]; then
    echo "ERROR: Firmware file does not exist: $src" >&2
    exit 1
fi

if [[ ! -d "$TARGET" ]]; then
    echo "ERROR: Target directory does not exist: $TARGET" >&2
    exit 1
fi

#
# Determine today's sequence letter from any existing firmware on the card.
#
# Valid names:
#   fwYYMMDD.bin
#   fwYYMMDDa.bin
#   fwYYMMDDb.bin
#   ...
#
shopt -s nullglob
today_fws=("$TARGET"/fw"$YYMMDD"*.bin)
shopt -u nullglob

if (( ${#today_fws[@]} > 0 )); then
    l=""

    for fw in "${today_fws[@]}"; do
        base="$(basename "$fw")"

        if [[ "$base" =~ ^fw${YYMMDD}([a-z]?)\.bin$ ]]; then
            candidate="${BASH_REMATCH[1]}"

            if [[ -z "$candidate" ]]; then
                # An unsuffixed file means the next one is 'a'.
                [[ -z "$l" ]] && l="a"
            elif [[ -z "$l" || "$candidate" > "$l" ]]; then
                # Existing suffixed file means use the following letter.
                printf -v next '%b' "\\$(printf '%03o' "$(( $(printf '%d' "'$candidate") + 1 ))")"
                l="$next"
            fi
        fi
    done

    fwname="fw${YYMMDD}${l}.bin"
fi

# Don't copy if the firmware is already present
shopt -s nullglob
existing_fws=("$TARGET"/fw*.bin)
shopt -u nullglob

if (( ${#existing_fws[@]} > 0 )); then
    latest_fw="$(ls -1t -- "${existing_fws[@]}" | head -n 1)"

    if cmp -s -- "$src" "$latest_fw"; then
        echo "Firmware byte-matches $(basename "$latest_fw"); nothing to do."
        exit 0
    fi
fi

# Remove old firmware binaries from the target.
shopt -s nullglob
oldfws=("$TARGET"/*.bin)
shopt -u nullglob

if (( ${#oldfws[@]} > 0 )); then
    rm -v -- "${oldfws[@]}"
fi

# Copy the new firmware.
cp -v -- "$src" "$TARGET/$fwname"

# vim: set ts=4 sts=4 sw=4:
