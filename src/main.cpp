#include "raylib.h"
#include "raymath.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kGridW = 56;
constexpr int kGridD = 40;
constexpr int kRackLevels = 8;
constexpr int kRobotCount = 30;
constexpr float kCellSize = 1.6f;
constexpr float kRobotFloorSpeed = 3.2f;
constexpr float kRobotClimbSpeed = 3.8f;
constexpr float kChargeRatePerSecond = 11.0f;

struct GridPos {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct BinSlot {
    GridPos pos;
    bool occupied = true;
    bool reserved = false;
};

enum class RobotState {
    Idle,
    ToRackBase,
    ClimbToBin,
    DescendWithBin,
    ToStation,
    Picking,
    ReturnToRackBase,
    ClimbReturn,
    DescendEmpty,
    ToCharge,
    Charging
};

struct Robot {
    int id = 0;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    std::vector<GridPos> path;
    int taskBin = -1;
    int stationIdx = -1;
    int chargerIdx = -1;
    RobotState state = RobotState::Idle;
    float battery = 100.0f;
    float waitTimer = 0.0f;
    float pickTimer = 0.0f;
    bool carrying = false;
    Color color = WHITE;
};

static bool gWalkable[kGridW][kGridD];
static bool gRackBase[kGridW][kGridD];

float Frand(std::mt19937& rng, float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(rng);
}

int Irand(std::mt19937& rng, int a, int b) {
    std::uniform_int_distribution<int> d(a, b);
    return d(rng);
}

Vector3 GridToWorld(float gx, float gy, float gz) {
    return {
        (gx - (kGridW - 1) * 0.5f) * kCellSize,
        gy * kCellSize,
        (gz - (kGridD - 1) * 0.5f) * kCellSize,
    };
}

long long Key2D(int x, int z) {
    return (static_cast<long long>(x) << 32) ^ (static_cast<unsigned>(z));
}

bool InBounds(int x, int z) {
    return x >= 0 && x < kGridW && z >= 0 && z < kGridD;
}

bool IsLane(int x, int z) {
    return (x % 2 == 0) || (z % 2 == 0);
}

struct GeneratedTrack {
    int sampleRate = 44100;
    int channels = 2;
    std::vector<int16_t> pcmInterleaved;
};

float WrapPhase(float phase) {
    while (phase >= 1.0f) phase -= 1.0f;
    while (phase < 0.0f) phase += 1.0f;
    return phase;
}

GeneratedTrack GenerateEuroTranceTrack(int variant) {
    GeneratedTrack out;
    constexpr float bpmOptions[5] = {138.0f, 140.0f, 136.0f, 142.0f, 139.0f};
    constexpr int barsOptions[5] = {16, 16, 16, 20, 16};
    constexpr float kickTopOptions[5] = {148.0f, 152.0f, 145.0f, 160.0f, 150.0f};
    constexpr float kickLowOptions[5] = {42.0f, 45.0f, 40.0f, 47.0f, 43.0f};
    constexpr float leadGainOptions[5] = {0.38f, 0.30f, 0.42f, 0.35f, 0.40f};
    constexpr float hatGainOptions[5] = {0.22f, 0.20f, 0.24f, 0.18f, 0.23f};
    constexpr float panRateOptions[5] = {0.12f, 0.10f, 0.14f, 0.08f, 0.16f};
    constexpr int bassSeqSet[5][8] = {
        {40, 40, 43, 43, 45, 45, 43, 47},
        {38, 38, 41, 41, 45, 45, 43, 41},
        {43, 43, 40, 40, 47, 47, 45, 43},
        {40, 43, 45, 47, 45, 43, 40, 38},
        {45, 45, 43, 43, 40, 40, 38, 43},
    };
    constexpr int leadSeqSet[5][8] = {
        {76, 79, 81, 79, 84, 81, 79, 76},
        {79, 83, 86, 83, 88, 86, 83, 79},
        {72, 76, 79, 76, 81, 79, 76, 72},
        {74, 77, 81, 84, 81, 77, 74, 72},
        {81, 79, 84, 86, 88, 86, 84, 79},
    };

    const int idx = std::clamp(variant, 0, 4);
    const float bpm = bpmOptions[idx];
    const int bars = barsOptions[idx];
    constexpr float beatsPerBar = 4.0f;
    const float secondsPerBeat = 60.0f / bpm;
    const float totalSeconds = bars * beatsPerBar * secondsPerBeat;
    const int totalFrames = static_cast<int>(totalSeconds * out.sampleRate);

    out.pcmInterleaved.resize(static_cast<size_t>(totalFrames) * 2);

    juce::Random rng(0x534B5950 + idx * 997);
    float bassPhase = 0.0f;
    float leadP1 = 0.0f;
    float leadP2 = 0.33f;
    float leadP3 = 0.66f;
    float sidechainEnv = 0.0f;

    constexpr int seqLen = 8;
    const int* bassSeq = bassSeqSet[idx];
    const int* leadSeq = leadSeqSet[idx];

    for (int n = 0; n < totalFrames; ++n) {
        const float t = static_cast<float>(n) / out.sampleRate;
        const float beatPos = t / secondsPerBeat;
        const float beatFrac = beatPos - std::floor(beatPos);
        const int beatIndex = static_cast<int>(std::floor(beatPos));
        const float barPos = beatPos / beatsPerBar;
        const float barFrac = barPos - std::floor(barPos);

        float kick = 0.0f;
        if (beatFrac < 0.23f) {
            const float x = beatFrac / 0.23f;
            const float env = std::exp(-x * 7.5f);
            const float hz = juce::jmap(x, kickTopOptions[idx], kickLowOptions[idx]);
            kick = std::sin(2.0f * juce::MathConstants<float>::pi * hz * t) * env;
            sidechainEnv = std::max(sidechainEnv, env);
        }
        sidechainEnv *= 0.9968f;

        float bass = 0.0f;
        const float offbeat = beatFrac - 0.5f;
        if (offbeat > 0.0f && offbeat < 0.42f) {
            const float env = std::exp(-offbeat * 9.0f);
            const int note = bassSeq[beatIndex % seqLen];
            const float hz = juce::MidiMessage::getMidiNoteInHertz(note);
            bassPhase = WrapPhase(bassPhase + hz / out.sampleRate);
            const float saw = bassPhase * 2.0f - 1.0f;
            bass = saw * env * 0.70f;
        }

        float lead = 0.0f;
        const float step = beatPos * 2.0f;
        const float stepFrac = step - std::floor(step);
        if (barFrac > 0.0f && barFrac < 0.95f && stepFrac < 0.38f) {
            const float env = std::exp(-stepFrac * 4.0f);
            const int note = leadSeq[(static_cast<int>(std::floor(step))) % seqLen];
            const float hz = juce::MidiMessage::getMidiNoteInHertz(note);
            leadP1 = WrapPhase(leadP1 + hz / out.sampleRate);
            leadP2 = WrapPhase(leadP2 + (hz * (1.004f + idx * 0.0007f)) / out.sampleRate);
            leadP3 = WrapPhase(leadP3 + (hz * (0.996f - idx * 0.0007f)) / out.sampleRate);
            const float s1 = leadP1 * 2.0f - 1.0f;
            const float s2 = leadP2 * 2.0f - 1.0f;
            const float s3 = leadP3 * 2.0f - 1.0f;
            lead = (s1 + s2 + s3) * (1.0f / 3.0f) * env * leadGainOptions[idx];
        }

        float hats = 0.0f;
        const float hatStep = beatPos * 2.0f;
        const float hatFrac = hatStep - std::floor(hatStep);
        if (hatFrac < 0.15f) {
            const float env = std::exp(-hatFrac * 30.0f);
            hats = (rng.nextFloat() * 2.0f - 1.0f) * env * hatGainOptions[idx];
        }

        float mono = kick * 1.1f + bass * 0.9f + lead + hats;
        const float sidechain = 1.0f - std::min(0.65f, sidechainEnv * 0.55f);
        mono *= sidechain;
        mono = std::tanh(mono * 1.6f);

        const float autoPan = std::sin(2.0f * juce::MathConstants<float>::pi * panRateOptions[idx] * t) * 0.16f;
        float left = mono * (1.0f - autoPan);
        float right = mono * (1.0f + autoPan);
        left = std::clamp(left, -1.0f, 1.0f);
        right = std::clamp(right, -1.0f, 1.0f);

        out.pcmInterleaved[static_cast<size_t>(n) * 2 + 0] = static_cast<int16_t>(left * 32767.0f);
        out.pcmInterleaved[static_cast<size_t>(n) * 2 + 1] = static_cast<int16_t>(right * 32767.0f);
    }

    return out;
}

Wave BuildWaveFromTrack(const GeneratedTrack& track) {
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(track.pcmInterleaved.size() / track.channels);
    wave.sampleRate = static_cast<unsigned int>(track.sampleRate);
    wave.sampleSize = 16;
    wave.channels = static_cast<unsigned int>(track.channels);
    const size_t bytes = track.pcmInterleaved.size() * sizeof(int16_t);
    wave.data = MemAlloc(bytes);
    std::memcpy(wave.data, track.pcmInterleaved.data(), bytes);
    return wave;
}

std::vector<GridPos> FindPath2D(GridPos start, GridPos goal) {
    if (start.x == goal.x && start.z == goal.z) return {};

    std::vector<int> parent(kGridW * kGridD, -1);
    std::deque<int> q;
    int s = start.z * kGridW + start.x;
    int g = goal.z * kGridW + goal.x;

    q.push_back(s);
    parent[s] = s;

    const int dx[4] = {1, -1, 0, 0};
    const int dz[4] = {0, 0, 1, -1};

    while (!q.empty()) {
        int cur = q.front();
        q.pop_front();
        if (cur == g) break;
        int cx = cur % kGridW;
        int cz = cur / kGridW;
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int nz = cz + dz[i];
            if (!InBounds(nx, nz) || !gWalkable[nx][nz]) continue;
            int ni = nz * kGridW + nx;
            if (parent[ni] != -1) continue;
            parent[ni] = cur;
            q.push_back(ni);
        }
    }

    if (parent[g] == -1) return {};

    std::vector<GridPos> rev;
    int cur = g;
    while (cur != s) {
        int x = cur % kGridW;
        int z = cur / kGridW;
        rev.push_back({x, 0, z});
        cur = parent[cur];
    }
    std::reverse(rev.begin(), rev.end());
    return rev;
}

bool IsFloorCellBusy(const std::vector<Robot>& robots, int selfId, int tx, int tz) {
    for (const Robot& r : robots) {
        if (r.id == selfId) continue;
        if (r.gy > 0.2f) continue;
        float dx = r.gx - static_cast<float>(tx);
        float dz = r.gz - static_cast<float>(tz);
        if ((dx * dx + dz * dz) < 0.20f) return true;
    }
    return false;
}

const char* StateLabel(RobotState s) {
    switch (s) {
        case RobotState::Idle: return "Idle";
        case RobotState::ToRackBase: return "ToRack";
        case RobotState::ClimbToBin: return "ClimbUp";
        case RobotState::DescendWithBin: return "DownBin";
        case RobotState::ToStation: return "ToPick";
        case RobotState::Picking: return "Picking";
        case RobotState::ReturnToRackBase: return "Return";
        case RobotState::ClimbReturn: return "ClimbRet";
        case RobotState::DescendEmpty: return "Down";
        case RobotState::ToCharge: return "Charge";
        case RobotState::Charging: return "Charging";
        default: return "?";
    }
}

struct Simulation {
    std::mt19937 rng{1337};
    std::vector<GridPos> pickStations;
    std::vector<GridPos> chargers;
    std::vector<BinSlot> bins;
    std::deque<int> orderQueue;
    std::vector<Robot> robots;

    float orderSpawnTimer = 0.0f;
    int completedOrders = 0;
    int score = 0;
    double takeHomePenceExact = 0.0;
    float tiredness = 0.0f;
    float urineLevel = 0.0f;

    void BuildWarehouse() {
        for (int x = 0; x < kGridW; ++x) {
            for (int z = 0; z < kGridD; ++z) {
                gRackBase[x][z] = false;
                gWalkable[x][z] = false;
            }
        }

        for (int x = 4; x < kGridW - 4; x += 4) {
            for (int z = 3; z < kGridD - 3; z += 3) {
                gRackBase[x][z] = true;
            }
        }

        pickStations = {
            {1, 0, 2},
            {1, 0, kGridD / 2},
            {1, 0, kGridD - 3},
        };

        chargers = {
            {kGridW - 2, 0, 2},
            {kGridW - 2, 0, kGridD / 2},
            {kGridW - 2, 0, kGridD - 3},
        };

        for (int x = 0; x < kGridW; ++x) {
            for (int z = 0; z < kGridD; ++z) {
                gWalkable[x][z] = IsLane(x, z) || gRackBase[x][z];
            }
        }
        for (const GridPos& s : pickStations) gWalkable[s.x][s.z] = true;
        for (const GridPos& c : chargers) gWalkable[c.x][c.z] = true;

        bins.clear();
        for (int x = 0; x < kGridW; ++x) {
            for (int z = 0; z < kGridD; ++z) {
                if (!gRackBase[x][z]) continue;
                for (int y = 1; y <= kRackLevels; ++y) {
                    bins.push_back({{x, y, z}, Frand(rng, 0.0f, 1.0f) > 0.25f, false});
                }
            }
        }
    }

    void SpawnRobots() {
        robots.clear();
        for (int i = 0; i < kRobotCount; ++i) {
            int laneZ = 1 + (i % (kGridD - 2));
            if (!gWalkable[0][laneZ]) laneZ = 2;
            Robot r;
            r.id = i;
            r.gx = 0.0f;
            r.gz = static_cast<float>(laneZ);
            r.gy = 0.0f;
            r.battery = Frand(rng, 40.0f, 100.0f);
            r.color = ColorFromHSV(static_cast<float>((i * 31) % 360), 0.7f, 0.95f);
            robots.push_back(r);
        }
    }

    void Reset() {
        orderQueue.clear();
        completedOrders = 0;
        score = 0;
        takeHomePenceExact = 0.0;
        tiredness = 0.0f;
        urineLevel = 0.0f;
        orderSpawnTimer = 0.0f;
        BuildWarehouse();
        SpawnRobots();
    }

    long long GetTakeHomePence() const {
        return static_cast<long long>(std::floor(takeHomePenceExact));
    }

    bool BuyCoffee() {
        constexpr long long kCoffeeCostPence = 300;
        if (GetTakeHomePence() < kCoffeeCostPence) return false;
        takeHomePenceExact = std::max(0.0, takeHomePenceExact - static_cast<double>(kCoffeeCostPence));
        tiredness = 0.0f;
        return true;
    }

    void UseToilet() {
        urineLevel = 0.0f;
    }

    int FindNearestStation(int x, int z) const {
        int best = 0;
        int bestDist = 1e9;
        for (int i = 0; i < static_cast<int>(pickStations.size()); ++i) {
            int d = std::abs(x - pickStations[i].x) + std::abs(z - pickStations[i].z);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        return best;
    }

    int FindNearestCharger(int x, int z) const {
        int best = 0;
        int bestDist = 1e9;
        for (int i = 0; i < static_cast<int>(chargers.size()); ++i) {
            int d = std::abs(x - chargers[i].x) + std::abs(z - chargers[i].z);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        return best;
    }

    int FindBestChargerForRobot(int x, int z) const {
        int assigned[3] = {0, 0, 0};
        for (const Robot& r : robots) {
            if (r.chargerIdx < 0 || r.chargerIdx >= static_cast<int>(chargers.size())) continue;
            if (r.state == RobotState::ToCharge || r.state == RobotState::Charging) {
                ++assigned[r.chargerIdx];
            }
        }

        int best = 0;
        int bestScore = 1e9;
        for (int i = 0; i < static_cast<int>(chargers.size()); ++i) {
            int dist = std::abs(x - chargers[i].x) + std::abs(z - chargers[i].z);
            int score = dist + assigned[i] * 8;
            if (score < bestScore) {
                bestScore = score;
                best = i;
            }
        }
        return best;
    }

    bool IsChargeDispatchable(const Robot& r) const {
        if (r.state == RobotState::ToCharge || r.state == RobotState::Charging) return false;
        if (r.carrying) return false;
        if (r.gy > 0.08f) return false;
        return true;
    }

    void SendRobotToCharge(int robotId) {
        auto it = std::find_if(robots.begin(), robots.end(), [&](const Robot& r) { return r.id == robotId; });
        if (it == robots.end()) return;
        Robot& r = *it;
        if (!IsChargeDispatchable(r)) return;

        if (r.taskBin >= 0) {
            if (std::find(orderQueue.begin(), orderQueue.end(), r.taskBin) == orderQueue.end()) {
                orderQueue.push_front(r.taskBin);
            }
            bins[r.taskBin].reserved = true;
        }

        r.taskBin = -1;
        r.stationIdx = -1;
        int rx = static_cast<int>(std::round(r.gx));
        int rz = static_cast<int>(std::round(r.gz));
        r.chargerIdx = FindBestChargerForRobot(rx, rz);
        GridPos c = chargers[r.chargerIdx];
        if (rx == c.x && rz == c.z) {
            r.path.clear();
            r.state = RobotState::Charging;
        } else {
            r.path = FindPath2D({rx, 0, rz}, c);
            r.state = RobotState::ToCharge;
        }
    }

    void SpawnOrder() {
        std::vector<int> candidates;
        candidates.reserve(bins.size());
        for (int i = 0; i < static_cast<int>(bins.size()); ++i) {
            if (bins[i].occupied && !bins[i].reserved) {
                candidates.push_back(i);
            }
        }
        if (candidates.empty()) return;

        int chosen = candidates[Irand(rng, 0, static_cast<int>(candidates.size()) - 1)];
        orderQueue.push_back(chosen);
        bins[chosen].reserved = true;
    }

    void AssignTasks() {
        for (Robot& r : robots) {
            if (r.state != RobotState::Idle || r.taskBin != -1) continue;

            if (r.battery < 45.0f) {
                int cx = static_cast<int>(std::round(r.gx));
                int cz = static_cast<int>(std::round(r.gz));
                int chargerIdx = FindBestChargerForRobot(cx, cz);
                r.chargerIdx = chargerIdx;
                r.path = FindPath2D({cx, 0, cz}, chargers[r.chargerIdx]);
                r.state = RobotState::ToCharge;
                continue;
            }

            if (orderQueue.empty()) continue;

            int bestIdx = -1;
            int bestCost = 1e9;
            int rx = static_cast<int>(std::round(r.gx));
            int rz = static_cast<int>(std::round(r.gz));
            for (int i = 0; i < static_cast<int>(orderQueue.size()); ++i) {
                int bi = orderQueue[i];
                const GridPos& p = bins[bi].pos;
                int cost = std::abs(rx - p.x) + std::abs(rz - p.z);
                if (cost < bestCost) {
                    bestCost = cost;
                    bestIdx = i;
                }
            }
            if (bestIdx == -1) continue;

            int binIdx = orderQueue[bestIdx];
            orderQueue.erase(orderQueue.begin() + bestIdx);
            r.taskBin = binIdx;
            r.stationIdx = FindNearestStation(bins[binIdx].pos.x, bins[binIdx].pos.z);
            r.path = FindPath2D({rx, 0, rz}, {bins[binIdx].pos.x, 0, bins[binIdx].pos.z});
            r.state = RobotState::ToRackBase;
            r.waitTimer = 0.0f;
        }
    }

    bool StepFloor(Robot& r, float dt) {
        if (r.path.empty()) return true;
        GridPos next = r.path.front();

        if (IsFloorCellBusy(robots, r.id, next.x, next.z)) {
            r.waitTimer += dt;
            if (r.waitTimer > 1.0f) {
                int rx = static_cast<int>(std::round(r.gx));
                int rz = static_cast<int>(std::round(r.gz));
                GridPos goal = r.path.back();
                r.path = FindPath2D({rx, 0, rz}, goal);
                r.waitTimer = 0.0f;
            }
            return false;
        }

        r.waitTimer = 0.0f;
        float dx = static_cast<float>(next.x) - r.gx;
        float dz = static_cast<float>(next.z) - r.gz;
        float dist = std::sqrt(dx * dx + dz * dz);
        float step = kRobotFloorSpeed * dt;
        if (dist <= step || dist < 0.001f) {
            r.gx = static_cast<float>(next.x);
            r.gz = static_cast<float>(next.z);
            r.path.erase(r.path.begin());
        } else {
            r.gx += dx / dist * step;
            r.gz += dz / dist * step;
        }
        return r.path.empty();
    }

    void UpdateRobot(Robot& r, float dt) {
        if (r.state == RobotState::Charging) {
            r.battery = std::min(100.0f, r.battery + kChargeRatePerSecond * dt);
            if (r.battery > 94.0f) r.state = RobotState::Idle;
            return;
        }

        // Preempt task and go charge before retrieval if battery gets too low.
        if (r.state == RobotState::ToRackBase && !r.carrying && r.battery < 20.0f) {
            if (r.taskBin >= 0) {
                if (std::find(orderQueue.begin(), orderQueue.end(), r.taskBin) == orderQueue.end()) {
                    orderQueue.push_front(r.taskBin);
                }
                bins[r.taskBin].reserved = true;
            }
            r.taskBin = -1;
            r.stationIdx = -1;
            int rx = static_cast<int>(std::round(r.gx));
            int rz = static_cast<int>(std::round(r.gz));
            r.chargerIdx = FindBestChargerForRobot(rx, rz);
            r.path = FindPath2D({rx, 0, rz}, chargers[r.chargerIdx]);
            r.state = RobotState::ToCharge;
        }

        bool moving = false;
        switch (r.state) {
            case RobotState::Idle:
                break;
            case RobotState::ToRackBase:
                moving = true;
                if (StepFloor(r, dt)) r.state = RobotState::ClimbToBin;
                break;
            case RobotState::ClimbToBin: {
                moving = true;
                const BinSlot& slot = bins[r.taskBin];
                r.gy += kRobotClimbSpeed * dt;
                if (r.gy >= static_cast<float>(slot.pos.y)) {
                    r.gy = static_cast<float>(slot.pos.y);
                    r.carrying = true;
                    bins[r.taskBin].occupied = false;
                    r.state = RobotState::DescendWithBin;
                }
                break;
            }
            case RobotState::DescendWithBin:
                moving = true;
                r.gy -= kRobotClimbSpeed * dt;
                if (r.gy <= 0.0f) {
                    r.gy = 0.0f;
                    int rx = static_cast<int>(std::round(r.gx));
                    int rz = static_cast<int>(std::round(r.gz));
                    r.path = FindPath2D({rx, 0, rz}, pickStations[r.stationIdx]);
                    r.state = RobotState::ToStation;
                }
                break;
            case RobotState::ToStation:
                moving = true;
                if (StepFloor(r, dt)) {
                    r.pickTimer = 1.8f;
                    r.state = RobotState::Picking;
                }
                break;
            case RobotState::Picking:
                r.pickTimer -= dt;
                if (r.pickTimer <= 0.0f) {
                    int rx = static_cast<int>(std::round(r.gx));
                    int rz = static_cast<int>(std::round(r.gz));
                    const GridPos& home = bins[r.taskBin].pos;
                    r.path = FindPath2D({rx, 0, rz}, {home.x, 0, home.z});
                    r.state = RobotState::ReturnToRackBase;
                    ++completedOrders;
                    score += 100;
                }
                break;
            case RobotState::ReturnToRackBase:
                moving = true;
                if (StepFloor(r, dt)) r.state = RobotState::ClimbReturn;
                break;
            case RobotState::ClimbReturn: {
                moving = true;
                const BinSlot& slot = bins[r.taskBin];
                r.gy += kRobotClimbSpeed * dt;
                if (r.gy >= static_cast<float>(slot.pos.y)) {
                    r.gy = static_cast<float>(slot.pos.y);
                    r.carrying = false;
                    bins[r.taskBin].occupied = true;
                    bins[r.taskBin].reserved = false;
                    r.state = RobotState::DescendEmpty;
                }
                break;
            }
            case RobotState::DescendEmpty:
                moving = true;
                r.gy -= kRobotClimbSpeed * dt;
                if (r.gy <= 0.0f) {
                    r.gy = 0.0f;
                    r.taskBin = -1;
                    r.stationIdx = -1;
                    r.path.clear();
                    r.state = RobotState::Idle;
                }
                break;
            case RobotState::ToCharge:
                moving = true;
            if (StepFloor(r, dt)) r.state = RobotState::Charging;
                break;
            case RobotState::Charging:
                break;
        }

        if (moving) {
            r.battery = std::max(0.0f, r.battery - 0.8f * dt);
        } else {
            r.battery = std::max(0.0f, r.battery - 0.03f * dt);
        }

        if (r.battery < 20.0f && r.state == RobotState::Idle) {
            int rx = static_cast<int>(std::round(r.gx));
            int rz = static_cast<int>(std::round(r.gz));
            r.chargerIdx = FindBestChargerForRobot(rx, rz);
            r.path = FindPath2D({rx, 0, rz}, chargers[r.chargerIdx]);
            r.state = RobotState::ToCharge;
        }
    }

    void Update(float dt) {
        constexpr double kTakeHomePencePerHour = 1221.0;
        takeHomePenceExact += (kTakeHomePencePerHour / 3600.0) * static_cast<double>(dt);
        tiredness = std::min(100.0f, tiredness + dt * 0.50f);
        urineLevel += dt * (0.35f + Frand(rng, 0.0f, 0.85f));
        if (Frand(rng, 0.0f, 1.0f) < dt * 0.18f) {
            urineLevel += Frand(rng, 0.4f, 1.8f);
        }
        urineLevel = std::clamp(urineLevel, 0.0f, 100.0f);

        orderSpawnTimer += dt;
        if (orderSpawnTimer >= 1.4f && orderQueue.size() < 120) {
            int spawnCount = Irand(rng, 0, 2);
            for (int i = 0; i < spawnCount; ++i) SpawnOrder();
            orderSpawnTimer = 0.0f;
        }

        AssignTasks();

        for (Robot& r : robots) {
            UpdateRobot(r, dt);
        }
    }

    void DrawWorld(bool debugOverlay) {
        bool activeLadder[kGridW][kGridD] = {};
        for (const Robot& r : robots) {
            bool verticalMove =
                r.state == RobotState::ClimbToBin ||
                r.state == RobotState::DescendWithBin ||
                r.state == RobotState::ClimbReturn ||
                r.state == RobotState::DescendEmpty;
            if (!verticalMove) continue;
            int lx = static_cast<int>(std::round(r.gx));
            int lz = static_cast<int>(std::round(r.gz));
            if (InBounds(lx, lz) && gRackBase[lx][lz]) {
                activeLadder[lx][lz] = true;
            }
        }

        for (int x = 0; x < kGridW; ++x) {
            for (int z = 0; z < kGridD; ++z) {
                Vector3 p = GridToWorld(static_cast<float>(x), -0.2f, static_cast<float>(z));
                Color c = IsLane(x, z) ? Color{64, 77, 64, 255} : Color{48, 56, 48, 255};
                DrawCube(p, kCellSize * 0.98f, kCellSize * 0.2f, kCellSize * 0.98f, c);
            }
        }

        for (int x = 0; x < kGridW; ++x) {
            for (int z = 0; z < kGridD; ++z) {
                if (!gRackBase[x][z]) continue;

                for (int y = 0; y <= kRackLevels; ++y) {
                    Vector3 p = GridToWorld(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                    DrawCubeWires(p, kCellSize * 0.95f, kCellSize * 0.95f, kCellSize * 0.95f, Color{85, 85, 92, 255});
                    if (y == 0) {
                        DrawCube(p, kCellSize * 0.22f, kCellSize * 0.22f, kCellSize * 0.22f, Color{90, 90, 96, 255});
                    }
                }

                Color ladderColor = activeLadder[x][z] ? Color{255, 224, 120, 255} : Color{120, 126, 136, 255};
                float ladderX = static_cast<float>(x) - 0.33f;
                float leftZ = static_cast<float>(z) - 0.18f;
                float rightZ = static_cast<float>(z) + 0.18f;
                float railHeight = kRackLevels * kCellSize;
                float railCenterY = kRackLevels * 0.5f;

                Vector3 railLeft = GridToWorld(ladderX, railCenterY, leftZ);
                Vector3 railRight = GridToWorld(ladderX, railCenterY, rightZ);
                DrawCube(railLeft, kCellSize * 0.08f, railHeight, kCellSize * 0.08f, ladderColor);
                DrawCube(railRight, kCellSize * 0.08f, railHeight, kCellSize * 0.08f, ladderColor);

                for (int rung = 0; rung <= kRackLevels * 2; ++rung) {
                    float gy = 0.25f + rung * 0.5f;
                    Vector3 rungPos = GridToWorld(ladderX, gy, static_cast<float>(z));
                    DrawCube(rungPos, kCellSize * 0.08f, kCellSize * 0.06f, kCellSize * 0.44f, ladderColor);
                }
            }
        }

        for (const BinSlot& b : bins) {
            if (!b.occupied) continue;
            Vector3 p = GridToWorld(static_cast<float>(b.pos.x), static_cast<float>(b.pos.y), static_cast<float>(b.pos.z));
            Color c = b.reserved ? Color{250, 180, 40, 255} : Color{214, 128, 33, 255};
            DrawCube(p, kCellSize * 0.66f, kCellSize * 0.66f, kCellSize * 0.66f, c);
            DrawCubeWires(p, kCellSize * 0.66f, kCellSize * 0.66f, kCellSize * 0.66f, BLACK);
        }

        for (const GridPos& s : pickStations) {
            Vector3 p = GridToWorld(static_cast<float>(s.x), 0.05f, static_cast<float>(s.z));
            DrawCube(p, kCellSize * 1.1f, kCellSize * 0.35f, kCellSize * 1.1f, Color{77, 143, 220, 255});
            DrawCubeWires(p, kCellSize * 1.1f, kCellSize * 0.35f, kCellSize * 1.1f, BLACK);
        }

        for (const GridPos& c : chargers) {
            Vector3 p = GridToWorld(static_cast<float>(c.x), 0.05f, static_cast<float>(c.z));
            DrawCube(p, kCellSize * 1.1f, kCellSize * 0.3f, kCellSize * 1.1f, Color{103, 201, 89, 255});
            DrawCubeWires(p, kCellSize * 1.1f, kCellSize * 0.3f, kCellSize * 1.1f, BLACK);
        }

        for (const Robot& r : robots) {
            Color pathColor = Color{190, 190, 200, 255};
            switch (r.state) {
                case RobotState::ToRackBase:
                case RobotState::ClimbToBin:
                case RobotState::DescendWithBin:
                case RobotState::ToStation:
                case RobotState::Picking:
                    pathColor = Color{0, 245, 255, 255};    // outbound
                    break;
                case RobotState::ReturnToRackBase:
                case RobotState::ClimbReturn:
                case RobotState::DescendEmpty:
                    pathColor = Color{255, 95, 35, 255};    // return
                    break;
                case RobotState::ToCharge:
                case RobotState::Charging:
                    pathColor = Color{75, 255, 95, 255};    // charging route
                    break;
                default:
                    break;
            }

            if (!r.path.empty()) {
                Vector3 prev = GridToWorld(r.gx, 0.24f, r.gz);
                for (const GridPos& wp : r.path) {
                    Vector3 next = GridToWorld(static_cast<float>(wp.x), 0.24f, static_cast<float>(wp.z));
                    DrawLine3D(prev, next, pathColor);
                    DrawLine3D({prev.x, prev.y + 0.03f, prev.z}, {next.x, next.y + 0.03f, next.z}, pathColor);
                    DrawLine3D(prev, next, pathColor);

                    Vector3 breadcrumb = GridToWorld(static_cast<float>(wp.x), 0.08f, static_cast<float>(wp.z));
                    DrawCube(breadcrumb, kCellSize * 0.30f, kCellSize * 0.12f, kCellSize * 0.30f, pathColor);
                    DrawCubeWires(breadcrumb, kCellSize * 0.30f, kCellSize * 0.12f, kCellSize * 0.30f, BLACK);
                    prev = next;
                }
            }

            Color robotBody = r.color;
            if (r.state == RobotState::ToCharge) robotBody = Color{90, 255, 120, 255};
            if (r.state == RobotState::Charging) robotBody = Color{30, 210, 80, 255};
            if (r.battery < 20.0f && r.state != RobotState::Charging) robotBody = Color{255, 95, 70, 255};

            Vector3 p = GridToWorld(r.gx, r.gy + 0.45f, r.gz);
            DrawCube(p, kCellSize * 0.5f, kCellSize * 0.9f, kCellSize * 0.5f, robotBody);
            DrawCubeWires(p, kCellSize * 0.5f, kCellSize * 0.9f, kCellSize * 0.5f, BLACK);

            if (r.battery < 35.0f) {
                float pulse = 0.75f + 0.25f * std::sin(static_cast<float>(GetTime()) * 7.0f + static_cast<float>(r.id));
                unsigned char alpha = static_cast<unsigned char>(120 + pulse * 135);
                Color warn = Color{255, 50, 40, alpha};

                Vector3 alert = GridToWorld(r.gx, r.gy + 1.28f, r.gz);
                DrawCube(alert, kCellSize * 0.24f, kCellSize * 0.24f, kCellSize * 0.24f, warn);
                DrawCubeWires(alert, kCellSize * 0.24f, kCellSize * 0.24f, kCellSize * 0.24f, BLACK);
            }

            if (r.carrying && r.taskBin >= 0) {
                Vector3 b = GridToWorld(r.gx, r.gy + 1.08f, r.gz);
                DrawCube(b, kCellSize * 0.45f, kCellSize * 0.45f, kCellSize * 0.45f, Color{237, 160, 50, 255});
                DrawCubeWires(b, kCellSize * 0.45f, kCellSize * 0.45f, kCellSize * 0.45f, BLACK);
            }
        }

        if (debugOverlay) {
            for (const Robot& r : robots) {
                if (r.path.empty()) continue;
                GridPos n = r.path.front();
                Vector3 a = GridToWorld(r.gx, r.gy + 0.65f, r.gz);
                Vector3 b = GridToWorld(static_cast<float>(n.x), 0.65f, static_cast<float>(n.z));
                DrawLine3D(a, b, BLACK);
            }
        }

        DrawGrid(20, kCellSize * 2.0f);
    }

    Camera3D GetCamera3D() const {
        Camera3D cam{};
        cam.position = {18.0f, 22.0f, 30.0f};
        cam.target = {0.0f, 5.0f, 0.0f};
        cam.up = {0.0f, 1.0f, 0.0f};
        cam.fovy = 60.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        return cam;
    }
};

}  // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Fulfilment");
    int monitor = GetCurrentMonitor();
    SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    SetWindowPosition(0, 0);
    InitAudioDevice();
    SetTargetFPS(144);

    std::vector<Sound> trancePlaylist;
    trancePlaylist.reserve(5);
    for (int i = 0; i < 5; ++i) {
        const GeneratedTrack track = GenerateEuroTranceTrack(i);
        Wave wave = BuildWaveFromTrack(track);
        Sound sound = LoadSoundFromWave(wave);
        UnloadWave(wave);
        SetSoundVolume(sound, 0.36f);
        trancePlaylist.push_back(sound);
    }
    int currentTrack = 0;
    PlaySound(trancePlaylist[currentTrack]);

    Simulation sim;
    sim.Reset();

    Camera3D camera = sim.GetCamera3D();
    enum class CameraMode {
        TopDown = 0,
        SideOn = 1,
        Isometric = 2,
        Orbital = 3,
        Free = 4
    };

    auto CameraModeName = [](CameraMode mode) -> const char* {
        switch (mode) {
            case CameraMode::TopDown: return "TopDown";
            case CameraMode::SideOn: return "SideOn";
            case CameraMode::Isometric: return "Isometric";
            case CameraMode::Orbital: return "Orbital";
            case CameraMode::Free: return "Free";
            default: return "?";
        }
    };

    const float centerGX = (kGridW - 1) * 0.5f;
    const float centerGZ = (kGridD - 1) * 0.5f;
    const Vector3 viewCenterTop = GridToWorld(centerGX, 0.65f, centerGZ);
    const Vector3 viewCenterAngled = GridToWorld(centerGX, kRackLevels * 0.20f, centerGZ);
    const float spanX = kGridW * kCellSize;
    const float spanZ = kGridD * kCellSize;
    const float spanMax = std::max(spanX, spanZ);
    const float frameDistance = spanMax * 1.05f * 0.9f;
    const float topHeight = spanMax * 1.7f * 0.5f;
    const float sideHeight = spanMax * 0.48f;
    const float isoHeight = spanMax * 0.74f;
    CameraMode cameraMode = CameraMode::Isometric;
    float orbitalAngle = 0.45f;
    float fixedZoom = 1.0f;
    int robotTableScroll = 0;
    bool onTitleScreen = true;
    float controlsLockedTimer = 0.0f;
    bool batteryPanelMinimized = false;
    bool chargerPanelMinimized = false;

    auto ApplyCameraPreset = [&](CameraMode mode) {
        camera.target = (mode == CameraMode::TopDown) ? viewCenterTop : viewCenterAngled;
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.fovy = 60.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        switch (mode) {
            case CameraMode::TopDown:
                camera.position = {viewCenterTop.x, viewCenterTop.y + topHeight * fixedZoom, viewCenterTop.z + 0.001f};
                camera.up = {0.0f, 0.0f, -1.0f};
                break;
            case CameraMode::SideOn:
                camera.position = {viewCenterAngled.x + frameDistance * fixedZoom, viewCenterAngled.y + sideHeight * fixedZoom, viewCenterAngled.z};
                break;
            case CameraMode::Isometric:
                camera.position = {
                    viewCenterAngled.x + frameDistance * 0.66f * fixedZoom,
                    viewCenterAngled.y + isoHeight * fixedZoom,
                    viewCenterAngled.z + frameDistance * 0.66f * fixedZoom
                };
                break;
            case CameraMode::Orbital:
                camera.position = {
                    viewCenterAngled.x + std::cos(orbitalAngle) * frameDistance * fixedZoom,
                    viewCenterAngled.y + spanMax * 0.56f * fixedZoom,
                    viewCenterAngled.z + std::sin(orbitalAngle) * frameDistance * fixedZoom
                };
                break;
            case CameraMode::Free:
                camera.position = {
                    viewCenterAngled.x + frameDistance * 0.9f,
                    viewCenterAngled.y + isoHeight * 0.95f,
                    viewCenterAngled.z + frameDistance * 0.9f
                };
                break;
            default:
                break;
        }
    };

    ApplyCameraPreset(cameraMode);
    bool showDebug = false;

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.033f);
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        Rectangle startButton = {
            screenW * 0.5f - 130.0f,
            screenH * 0.5f + 52.0f,
            260.0f,
            56.0f
        };
        Vector2 mouse = GetMousePosition();
        controlsLockedTimer = std::max(0.0f, controlsLockedTimer - dt);

        if (onTitleScreen) {
            bool startNow = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
            if (!startNow && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, startButton)) {
                startNow = true;
            }
            if (startNow) {
                onTitleScreen = false;
                sim.Reset();
                ApplyCameraPreset(cameraMode);
            }

            BeginDrawing();
            ClearBackground(Color{16, 22, 34, 255});
            DrawRectangleGradientV(0, 0, screenW, screenH, Color{28, 39, 60, 255}, Color{9, 13, 22, 255});
            DrawText("FULFILMENT", screenW / 2 - 250, screenH / 2 - 170, 74, RAYWHITE);
            DrawText("Adventures in Warehouse Capitalism", screenW / 2 - 305, screenH / 2 - 84, 34, Color{130, 220, 255, 255});
            DrawText("Climb racks. Retrieve bins. Feed pick stations.", screenW / 2 - 235, screenH / 2 - 34, 26, Color{235, 240, 248, 255});
            DrawText("The job never ends", screenW / 2 - 102, screenH / 2 + 2, 22, Color{170, 210, 255, 255});
            DrawRectangleRec(startButton, Color{96, 206, 132, 255});
            DrawRectangleLinesEx(startButton, 2.0f, BLACK);
            DrawText("START", static_cast<int>(startButton.x) + 84, static_cast<int>(startButton.y) + 14, 28, BLACK);
            DrawText("Press ENTER or click START", screenW / 2 - 160, screenH / 2 + 126, 22, LIGHTGRAY);
            EndDrawing();
            continue;
        }

        const int tableW = 540;
        const int tableX = screenW - tableW - 10;
        const int tableY = 10;
        const int headerH = 54;
        const int rowH = 22;
        const int visibleRows = 10;
        const int footerH = 28;
        const int batteryMinH = 42;
        const int tableHFull = headerH + visibleRows * rowH + footerH;
        const int tableH = batteryPanelMinimized ? batteryMinH : tableHFull;
        const int rowAreaTop = tableY + headerH;
        const int rowAreaBottom = tableY + tableH - 10;
        const int maxScroll = std::max(0, static_cast<int>(sim.robots.size()) - visibleRows);
        bool mouseOverTable = (mouse.x >= tableX && mouse.x <= tableX + tableW && mouse.y >= tableY && mouse.y <= tableY + tableH);
        Rectangle batteryToggleButton = {
            static_cast<float>(tableX + tableW - 34),
            static_cast<float>(tableY + 10),
            24.0f,
            24.0f
        };
        float wheel = GetMouseWheelMove();
        bool controlsLocked = controlsLockedTimer > 0.0f;

        const int chargerTableW = 470;
        const int chargerHeaderH = 44;
        const int chargerRowH = 22;
        const int chargerRows = static_cast<int>(sim.chargers.size());
        const int chargerMinH = 42;
        const int chargerTableHFull = chargerHeaderH + chargerRows * chargerRowH + 12;
        const int chargerTableH = chargerPanelMinimized ? chargerMinH : chargerTableHFull;
        const int chargerTableX = screenW - chargerTableW - 10;
        const int chargerTableY = screenH - chargerTableH - 10;
        Rectangle chargerToggleButton = {
            static_cast<float>(chargerTableX + chargerTableW - 34),
            static_cast<float>(chargerTableY + 10),
            24.0f,
            24.0f
        };

        const int workerPanelX = 10;
        const int workerPanelY = screenH - 172;
        const int workerPanelW = 470;
        const int workerPanelH = 162;
        Rectangle coffeeButton = {
            static_cast<float>(workerPanelX + 16),
            static_cast<float>(workerPanelY + 112),
            170.0f,
            36.0f
        };
        Rectangle toiletButton = {
            static_cast<float>(workerPanelX + 202),
            static_cast<float>(workerPanelY + 112),
            170.0f,
            36.0f
        };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mouse, batteryToggleButton)) {
                batteryPanelMinimized = !batteryPanelMinimized;
            } else if (CheckCollisionPointRec(mouse, chargerToggleButton)) {
                chargerPanelMinimized = !chargerPanelMinimized;
            }
        }

        if (!controlsLocked && IsKeyPressed(KEY_TAB)) {
            int next = (static_cast<int>(cameraMode) + 1) % 5;
            cameraMode = static_cast<CameraMode>(next);
            ApplyCameraPreset(cameraMode);
        }
        if (!controlsLocked && IsKeyPressed(KEY_F1)) showDebug = !showDebug;
        if (!controlsLocked && IsKeyPressed(KEY_R)) sim.Reset();

        if (!controlsLocked && !batteryPanelMinimized && mouseOverTable && std::abs(wheel) > 0.001f) {
            robotTableScroll -= static_cast<int>(wheel * 3.0f);
            robotTableScroll = std::clamp(robotTableScroll, 0, maxScroll);
        }

        if (!controlsLocked && cameraMode != CameraMode::Free && (!mouseOverTable || batteryPanelMinimized)) {
            if (std::abs(wheel) > 0.001f) {
                fixedZoom *= std::pow(0.9f, wheel);
                fixedZoom = std::clamp(fixedZoom, 0.35f, 2.5f);
            }
        }

        if (!controlsLocked && cameraMode == CameraMode::Free) {
            UpdateCamera(&camera, CAMERA_FREE);
        } else if (!controlsLocked && cameraMode == CameraMode::Orbital) {
            orbitalAngle += dt * 0.42f;
            camera.target = viewCenterAngled;
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.fovy = 60.0f;
            camera.position = {
                viewCenterAngled.x + std::cos(orbitalAngle) * frameDistance * fixedZoom,
                viewCenterAngled.y + spanMax * 0.56f * fixedZoom,
                viewCenterAngled.z + std::sin(orbitalAngle) * frameDistance * fixedZoom
            };
        } else {
            camera.target = (cameraMode == CameraMode::TopDown) ? viewCenterTop : viewCenterAngled;
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.fovy = 60.0f;
            camera.projection = CAMERA_PERSPECTIVE;
            switch (cameraMode) {
                case CameraMode::TopDown:
                    camera.position = {viewCenterTop.x, viewCenterTop.y + topHeight * fixedZoom, viewCenterTop.z + 0.001f};
                    camera.up = {0.0f, 0.0f, -1.0f};
                    break;
                case CameraMode::SideOn:
                    camera.position = {viewCenterAngled.x + frameDistance * fixedZoom, viewCenterAngled.y + sideHeight * fixedZoom, viewCenterAngled.z};
                    break;
                case CameraMode::Isometric:
                    camera.position = {
                        viewCenterAngled.x + frameDistance * 0.66f * fixedZoom,
                        viewCenterAngled.y + isoHeight * fixedZoom,
                        viewCenterAngled.z + frameDistance * 0.66f * fixedZoom
                    };
                    break;
                default:
                    break;
            }
        }

        if (!trancePlaylist.empty() && !IsSoundPlaying(trancePlaylist[currentTrack])) {
            currentTrack = (currentTrack + 1) % static_cast<int>(trancePlaylist.size());
            PlaySound(trancePlaylist[currentTrack]);
        }

        sim.Update(dt);

        if (!controlsLocked && !batteryPanelMinimized && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mouseOverTable) {
            int start = robotTableScroll;
            int end = std::min(start + visibleRows, static_cast<int>(sim.robots.size()));
            for (int i = start; i < end; ++i) {
                int row = i - start;
                int y = rowAreaTop + row * rowH;
                Rectangle button = {
                    static_cast<float>(tableX + tableW - 98),
                    static_cast<float>(y + 2),
                    84.0f,
                    static_cast<float>(rowH - 4)
                };
                if (CheckCollisionPointRec(mouse, button)) {
                    sim.SendRobotToCharge(sim.robots[i].id);
                    break;
                }
            }
        }

        if (!controlsLocked && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mouse, coffeeButton)) {
                sim.BuyCoffee();
            } else if (CheckCollisionPointRec(mouse, toiletButton)) {
                sim.UseToilet();
                controlsLockedTimer = 20.0f;
            }
        }

        BeginDrawing();
        ClearBackground(Color{170, 210, 255, 255});

        BeginMode3D(camera);
        sim.DrawWorld(showDebug);
        EndMode3D();

        DrawRectangle(8, 8, 470, 188, Color{15, 18, 26, 185});
        DrawText("Fulfilment", 18, 14, 30, RAYWHITE);
        DrawText("Next-Gen 3D Warehouse Swarm Simulation", 18, 48, 20, Color{190, 220, 255, 255});
        DrawText(TextFormat("Robots: %d", static_cast<int>(sim.robots.size())), 18, 74, 18, RAYWHITE);
        DrawText(TextFormat("Open orders: %d", static_cast<int>(sim.orderQueue.size())), 18, 96, 18, ORANGE);
        DrawText(TextFormat("Completed orders: %d", sim.completedOrders), 18, 118, 18, GREEN);
        DrawText(TextFormat("Company Income: %d", sim.score), 18, 140, 18, YELLOW);
        DrawText(TextFormat("Camera: %s (TAB cycles)", CameraModeName(cameraMode)), 18, 162, 18, LIGHTGRAY);

        int lowBattery = 0;
        int charging = 0;
        float avgBattery = 0.0f;
        for (const Robot& r : sim.robots) {
            if (r.battery < 20.0f) ++lowBattery;
            if (r.state == RobotState::Charging || r.state == RobotState::ToCharge) ++charging;
            avgBattery += r.battery;
        }
        if (!sim.robots.empty()) {
            avgBattery /= static_cast<float>(sim.robots.size());
        }
        const float musicPitch = 1.0f + (1.0f - std::clamp(avgBattery / 100.0f, 0.0f, 1.0f)) * 0.10f;
        if (!trancePlaylist.empty()) {
            SetSoundPitch(trancePlaylist[currentTrack], musicPitch);
        }

        DrawRectangle(workerPanelX, workerPanelY, workerPanelW, workerPanelH, Color{15, 18, 26, 185});
        DrawRectangleLines(workerPanelX, workerPanelY, workerPanelW, workerPanelH, Color{120, 160, 200, 220});
        long long takeHomePence = sim.GetTakeHomePence();
        long long takeHomePounds = takeHomePence / 100;
        long long takeHomePenceRemainder = takeHomePence % 100;
        DrawText(TextFormat("Your takehome pay: GBP %lld.%02lld", takeHomePounds, takeHomePenceRemainder), workerPanelX + 12, workerPanelY + 12, 20, Color{255, 214, 170, 255});
        DrawText(TextFormat("Tiredness: %.1f", sim.tiredness), workerPanelX + 12, workerPanelY + 44, 18, Color{235, 230, 170, 255});
        DrawText(TextFormat("Urine level: %.1f", sim.urineLevel), workerPanelX + 12, workerPanelY + 68, 18, Color{190, 225, 255, 255});
        DrawText(TextFormat("Low battery robots: %d  Charging: %d", lowBattery, charging), workerPanelX + 12, workerPanelY + 90, 16, LIGHTGRAY);

        bool canBuyCoffee = sim.GetTakeHomePence() >= 300;
        Color coffeeColor = canBuyCoffee && !controlsLocked ? Color{218, 184, 98, 255} : Color{100, 100, 104, 220};
        Color toiletColor = !controlsLocked ? Color{126, 190, 255, 255} : Color{100, 100, 104, 220};
        DrawRectangleRec(coffeeButton, coffeeColor);
        DrawRectangleLinesEx(coffeeButton, 1.0f, BLACK);
        DrawText("Coffee - GBP 3", static_cast<int>(coffeeButton.x) + 16, static_cast<int>(coffeeButton.y) + 10, 16, BLACK);
        DrawRectangleRec(toiletButton, toiletColor);
        DrawRectangleLinesEx(toiletButton, 1.0f, BLACK);
        DrawText("Toilet (20s lock)", static_cast<int>(toiletButton.x) + 10, static_cast<int>(toiletButton.y) + 10, 16, BLACK);
        if (controlsLocked) {
            DrawText(TextFormat("Controls locked: %.1fs", controlsLockedTimer), workerPanelX + 286, workerPanelY + 90, 16, RED);
        }

        DrawRectangle(tableX, tableY, tableW, tableH, Color{10, 16, 24, 210});
        DrawRectangleLines(tableX, tableY, tableW, tableH, Color{120, 160, 200, 220});
        DrawText("Robot Battery + Charge Dispatch", tableX + 12, tableY + 10, 20, RAYWHITE);
        DrawRectangleRec(batteryToggleButton, Color{95, 110, 132, 230});
        DrawRectangleLinesEx(batteryToggleButton, 1.0f, BLACK);
        DrawText(batteryPanelMinimized ? "+" : "-", static_cast<int>(batteryToggleButton.x) + 8, static_cast<int>(batteryToggleButton.y) + 2, 20, WHITE);
        if (!batteryPanelMinimized) {
        DrawText("ID", tableX + 14, tableY + 34, 16, LIGHTGRAY);
        DrawText("Battery", tableX + 72, tableY + 34, 16, LIGHTGRAY);
        DrawText("State", tableX + 178, tableY + 34, 16, LIGHTGRAY);
        DrawText("Action", tableX + tableW - 96, tableY + 34, 16, LIGHTGRAY);

        int start = robotTableScroll;
        int end = std::min(start + visibleRows, static_cast<int>(sim.robots.size()));
        for (int i = start; i < end; ++i) {
            const Robot& r = sim.robots[i];
            int row = i - start;
            int y = rowAreaTop + row * rowH;
            Color rowBg = (row % 2 == 0) ? Color{20, 30, 44, 205} : Color{16, 24, 36, 205};
            DrawRectangle(tableX + 8, y, tableW - 16, rowH - 1, rowBg);

            Color batteryColor = (r.battery < 20.0f) ? RED : (r.battery < 45.0f ? YELLOW : Color{130, 255, 130, 255});
            DrawText(TextFormat("%02d", r.id), tableX + 14, y + 4, 15, RAYWHITE);
            DrawText(TextFormat("%5.1f%%", r.battery), tableX + 72, y + 4, 15, batteryColor);
            DrawText(StateLabel(r.state), tableX + 178, y + 4, 15, Color{210, 220, 230, 255});

            Rectangle button = {
                static_cast<float>(tableX + tableW - 98),
                static_cast<float>(y + 2),
                84.0f,
                static_cast<float>(rowH - 4)
            };
            bool canCharge = sim.IsChargeDispatchable(r);
            Color btnColor = canCharge ? Color{85, 200, 115, 255} : Color{90, 98, 110, 220};
            DrawRectangleRec(button, btnColor);
            DrawRectangleLinesEx(button, 1.0f, BLACK);
            DrawText(canCharge ? "Charge" : "-", static_cast<int>(button.x) + 18, static_cast<int>(button.y) + 3, 14, BLACK);
        }

        if (maxScroll > 0) {
            DrawText(TextFormat("Scroll %d/%d", robotTableScroll, maxScroll), tableX + tableW - 150, tableY + tableH - 22, 14, LIGHTGRAY);
        } else {
            DrawText("All robots visible", tableX + tableW - 150, tableY + tableH - 22, 14, LIGHTGRAY);
        }
        }

        DrawRectangle(chargerTableX, chargerTableY, chargerTableW, chargerTableH, Color{10, 16, 24, 210});
        DrawRectangleLines(chargerTableX, chargerTableY, chargerTableW, chargerTableH, Color{120, 160, 200, 220});
        DrawText("Chargers", chargerTableX + 12, chargerTableY + 10, 20, RAYWHITE);
        DrawRectangleRec(chargerToggleButton, Color{95, 110, 132, 230});
        DrawRectangleLinesEx(chargerToggleButton, 1.0f, BLACK);
        DrawText(chargerPanelMinimized ? "+" : "-", static_cast<int>(chargerToggleButton.x) + 8, static_cast<int>(chargerToggleButton.y) + 2, 20, WHITE);
        if (!chargerPanelMinimized) {
        DrawText("ID", chargerTableX + 14, chargerTableY + 28, 16, LIGHTGRAY);
        DrawText("Location", chargerTableX + 64, chargerTableY + 28, 16, LIGHTGRAY);
        DrawText("Free", chargerTableX + 258, chargerTableY + 28, 16, LIGHTGRAY);
        DrawText("ETA >94%", chargerTableX + 322, chargerTableY + 28, 16, LIGHTGRAY);

        for (int ci = 0; ci < chargerRows; ++ci) {
            int y = chargerTableY + chargerHeaderH + ci * chargerRowH;
            Color rowBg = (ci % 2 == 0) ? Color{20, 30, 44, 205} : Color{16, 24, 36, 205};
            DrawRectangle(chargerTableX + 8, y, chargerTableW - 16, chargerRowH - 1, rowBg);

            bool inUse = false;
            float etaSeconds = -1.0f;
            for (const Robot& r : sim.robots) {
                if (r.chargerIdx != ci) continue;
                if (r.state == RobotState::Charging || r.state == RobotState::ToCharge) {
                    inUse = true;
                }
                if (r.state == RobotState::Charging) {
                    etaSeconds = std::max(0.0f, (94.0f - r.battery) / kChargeRatePerSecond);
                    break;
                }
            }

            const GridPos& c = sim.chargers[ci];
            DrawText(TextFormat("%d", ci), chargerTableX + 14, y + 4, 15, RAYWHITE);
            DrawText(TextFormat("(%d, %d)", c.x, c.z), chargerTableX + 64, y + 4, 15, Color{210, 220, 230, 255});
            DrawText(inUse ? "No" : "Yes", chargerTableX + 264, y + 4, 15, inUse ? RED : Color{130, 255, 130, 255});
            DrawText(
                etaSeconds >= 0.0f ? TextFormat("%.1fs", etaSeconds) : "-",
                chargerTableX + 338,
                y + 4,
                15,
                etaSeconds >= 0.0f ? YELLOW : LIGHTGRAY
            );
        }
        }

        EndDrawing();
    }

    for (Sound& s : trancePlaylist) {
        StopSound(s);
        UnloadSound(s);
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
