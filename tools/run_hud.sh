#!/usr/bin/env bash
# Runs pipeline_hud.py in a restart loop so the tmux pane never terminates
while true; do
    python3 /home/andr/celestial_engine_project/tools/pipeline_hud.py
    sleep 1
done
