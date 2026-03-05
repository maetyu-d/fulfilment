# Skypod Swarm 3D (Minecraft-style warehouse simulation)

A C++/raylib prototype game that simulates a next-generation 3D warehouse robot system inspired by climbing-rack platforms (for example, Exotec-style flows).

## Features

- Minecraft-like block visuals (voxel racks, bins, stations, floor lanes)
- Robots that:
  - drive along floor lanes
  - transition onto rack columns
  - climb vertically to bin levels
  - retrieve bins and deliver to pick stations
  - return bins to storage
- Continuous 3D swarm movement with dozens of autonomous robots
- Distributed swarm behaviors:
  - task scheduling (order queue -> idle robot assignment)
  - local congestion avoidance (cell occupancy + reroute attempts)
  - path planning on floor grid
  - battery consumption and charging priorities
- Goods-to-person flow loop (orders generate bin retrieval tasks)
- JUCE-powered procedural euro-trance soundtrack (5 generated tracks in playlist rotation)

## Controls

- `W/A/S/D`: move camera
- `Q/E`: move camera down/up
- Mouse drag (right button): look around
- `R`: reset simulation
- `TAB`: cycle camera mode (top-down, side-on, isometric, orbital, free)
- `F1`: toggle debug overlays
- `ESC`: quit

## Build

Prerequisites:
- CMake 3.18+
- C++17 compiler
- Internet access at configure time (CMake fetches raylib)
- A local `JUCE` folder at project root (already included in this workspace)

```bash
cmake -S . -B build
cmake --build build -j
./build/skypod_swarm
```

## Notes

This is a simulation game prototype, not a digital twin. Movement, scheduling, and collision logic are intentionally simplified for clarity and performance.
