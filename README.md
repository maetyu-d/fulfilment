# Fulfilment

A next-generation warehouse robot game inspired by climbing-rack platforms (for example, [Exotec](https://www.exotec.com/system/automated-warehouse-robots/?creative=732460942001&keyword=exotec%20robotics&matchtype=b&network=g&device=c&utm_source=google&utm_medium=cpc&utm_campaign=bu~weu_c~gbr_l~en_e~p-search_b~goo_o~conversion_t~brand&gad_source=1&gad_campaignid=21645955334&gbraid=0AAAAApDt7A2zq56FtPnhzlHEAFuW79FcY&gclid=CjwKCAiAzZ_NBhAEEiwAMtqKy4QhVfeQdZKTriL0FOO4eUXeZ5vu65nyd1sp0Pf0S6zyD_NDFrC9XBoCo64QAvD_BwE)-style flows), the relentless march of consumer capitalism, and the fact that a huge swathe of fields in Minworth, West Midlands, has been replaced by the more than one-million-square-foot Amazon EMA4 fulfilment centre. Remember, the job never ends...

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
