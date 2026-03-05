# Fulfilment

![](https://github.com/maetyu-d/fulfilment/blob/main/Screenshot%202026-03-05%20at%2003.35.04.png)

`Fulfilment` is a C++/raylib warehouse simulation game with Minecraft-style visuals and JUCE-generated background music.

## Core Gameplay

- 3D warehouse swarm simulation with rack-climbing robots
- Goods-to-person loop:
  - order appears
  - robot retrieves bin
  - robot delivers to pick station
  - robot returns bin to rack
- Company economy:
  - `Company Income` increases per completed order
  - `Your takehome pay` accrues over time at `GBP 12.21/hour`
- Worker status mechanics:
  - `Tiredness` slowly increases
  - `Urine level` slowly and erratically increases
  - buy coffee (`GBP 3`) to reset tiredness
  - use toilet to reset urine, but controls lock for 20 seconds

## Visual + UI Features

- Voxel warehouse, ladders/rails, bins, stations, chargers
- Robot paths with color-coded route states
- Battery warning indicators above low-charge robots
- Top-right robot battery panel with per-robot `Charge` button
- Bottom-right charger table with free/busy state and charging ETA
- Toggle buttons to minimize/expand battery and charger panels
- Title screen:
  - `FULFILMENT`
  - subtitle: `Adventures in Warehouse Capitalism`
  - tagline: `Because work never ends.`

## Audio

- JUCE-powered procedural euro-trance soundtrack
- 5 generated tracks in rotating playlist
- Music speed increases proportionally (up to +10%) as average robot battery drops

## Camera + Input

- `TAB` cycles: top-down, side-on, isometric, orbital, free
- Mouse wheel zoom in non-free camera modes
- Free camera movement:
  - `W/A/S/D` move
  - `Q/E` down/up
  - right mouse drag to look

## Build and Run

Prerequisites:
- CMake 3.18+
- C++17 compiler
- Internet access at configure time (for raylib fetch)
- Local `JUCE/` folder at project root

```bash
cd /Users/md/Downloads/simulations
cmake -S . -B build
cmake --build build -j
./build/fulfilment
```

## Notes

This is a stylized simulation game prototype, not a warehouse digital twin.
