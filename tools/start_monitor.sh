#!/usr/bin/env bash
# Celestial Engine Mk.V — Tmux Mission Control Launcher
# Launches a 3-pane split monitoring dashboard on Raspberry Pi 5:
#   Pane 0 (Left):   btop (Full-screen real-time CPU, RAM, NVMe I/O, process graphs)
#   Pane 1 (Top R):  pipeline_hud.py (Pi 5 thermals, Ollama model state, AI core telemetry)
#   Pane 2 (Bot R):  journalctl -u celestial-brain.service -f (Live voice transaction logs)

SESSION_NAME="celestial"

# Restart session if requested with --restart or -r
if [ "$1" == "--restart" ] || [ "$1" == "-r" ]; then
    echo "Restarting '$SESSION_NAME' tmux session..."
    tmux kill-session -t "$SESSION_NAME" 2>/dev/null
fi

# If session already exists, attach to it
if tmux has-session -t "$SESSION_NAME" 2>/dev/null; then
    echo "Attaching to existing '$SESSION_NAME' tmux session..."
    tmux attach -t "$SESSION_NAME"
    exit 0
fi

echo "Initializing Celestial Engine Mission Control in tmux..."

# Create new detached session with btop in pane 0
tmux new-session -d -s "$SESSION_NAME" -n "Dashboard" "btop"

# Enable mouse mode for easy scrolling, pane resizing, and clicking
tmux set-option -t "$SESSION_NAME" mouse on

# Split window horizontally: 55% left (btop), 45% right (run_hud.sh auto-restart loop)
tmux split-window -h -t "$SESSION_NAME:0.0" -p 45 "/home/andr/celestial_engine_project/tools/run_hud.sh"

# Split right pane vertically: 50% top (HUD), 50% bottom (logs)
tmux split-window -v -t "$SESSION_NAME:0.1" -p 50 "sudo journalctl -u celestial-brain.service -f -o cat"

# Select pane 0 as active
tmux select-pane -t "$SESSION_NAME:0.0"

# Attach to session
tmux attach -t "$SESSION_NAME"
