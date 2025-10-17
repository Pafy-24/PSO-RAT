#!/usr/bin/env bash
# Helper script to open two terminals for the RAT server:
# - Terminal 1: runs the server (shows logs and command controller output if attached)
# - Terminal 2: connects to the admin command channel (server_port + 1) via nc
#
# Usage: ./scripts/run_server_with_terminals.sh [server_port]
# Default server_port: 5555

set -euo pipefail

PORT=${1:-5555}
ADMIN_PORT=$((PORT + 1))
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SERVER_BIN="$ROOT_DIR/build_make/rat_server"

if [ ! -x "$SERVER_BIN" ]; then
  echo "Error: server binary not found or not executable: $SERVER_BIN"
  echo "Run 'make' at project root first."
  exit 1
fi

# Candidate terminal emulators and how to launch a command in each
declare -A EMULATORS
EMULATORS[gnome-terminal]="-- bash -lc '%s; exec bash'"
EMULATORS[konsole]="-e bash -lc '%s; exec bash'"
EMULATORS[xfce4-terminal]="-e bash -lc '%s; exec bash'"
EMULATORS[xterm]="-e bash -lc '%s; exec bash'"
EMULATORS[urxvt]="-e bash -lc '%s; exec bash'"
EMULATORS[alacritty]="-e bash -lc '%s; exec bash'"
EMULATORS[kitty]="-- bash -lc '%s; exec bash'"

detect_terminal() {
  for t in "${!EMULATORS[@]}"; do
    if command -v "$t" >/dev/null 2>&1; then
      echo "$t"
      return 0
    fi
  done
  return 1
}

TERMINAL_CMD="$(detect_terminal || true)"

if [ -z "$TERMINAL_CMD" ]; then
  echo "No known terminal emulator found (gnome-terminal, konsole, xfce4-terminal, xterm, alacritty, kitty, urxvt)."
  echo "You can still run the server manually:" 
  echo "  $SERVER_BIN"
  echo "and connect to the admin channel:" 
  echo "  nc localhost $ADMIN_PORT"
  exit 1
fi

# Launch server in terminal 1
SERVER_CMD="$SERVER_BIN"
TER_CMD_TEMPLATE="${EMULATORS[$TERMINAL_CMD]}"
TER_CMD1=$(printf "$TER_CMD_TEMPLATE" "$SERVER_CMD")

# Launch nc in terminal 2 to connect to admin port
NC_CMD="bash -lc 'sleep 0.5; echo "Connecting to admin port $ADMIN_PORT..."; nc localhost $ADMIN_PORT; exec bash'"
TER_CMD2=$(printf "$TER_CMD_TEMPLATE" "$NC_CMD")

# Start both terminals
echo "Opening terminal 1: running server ($SERVER_BIN)"
# shellcheck disable=SC2086
$TERMINAL_CMD $TER_CMD1 &

sleep 0.3

echo "Opening terminal 2: connecting to admin port $ADMIN_PORT"
# shellcheck disable=SC2086
$TERMINAL_CMD $TER_CMD2 &

exit 0
