#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class Character;

using ArmyHandleId = std::uint64_t;

enum class ArmyState
{
    Idle,
    Preparing,
    Spawning,
    Active,
    Dismissing
};

struct SpawnRequest
{
    std::string templateName;
    int index = 0;
};

struct ArmySession
{
    ArmyState state = ArmyState::Idle;
    int requestedCount = 30;
    int spawnedCount = 0;
    int pendingRequestCount = 0;
    int totalSpawnAttempts = 0;
    int failedSpawnAttempts = 0;
    int deferredSpawnAttempts = 0;
    int currentWaveTarget = 0;
    float durationSeconds = 180.0f;
    float remainingSeconds = 0.0f;
    float escortRefreshAccumulator = 0.0f;
    bool active = false;
    bool lockOneArmyAtATime = true;
    bool waitingForReplayOpportunity = false;
    bool factionBootstrappedFromLeader = false;
    ArmyHandleId leaderHandleId = 0;
    ArmyHandleId leaderPlatoonHandleId = 0;
    std::deque<SpawnRequest> pendingRequests;
    std::vector<ArmyHandleId> activeUnitHandleIds;
    std::vector<Character*> activeUnits;
};

inline const char* ToString(ArmyState state)
{
    switch (state)
    {
    case ArmyState::Idle:
        return "Idle";
    case ArmyState::Preparing:
        return "Preparing";
    case ArmyState::Spawning:
        return "Spawning";
    case ArmyState::Active:
        return "Active";
    case ArmyState::Dismissing:
        return "Dismissing";
    default:
        return "Unknown";
    }
}

inline void ResetArmySession(ArmySession& session)
{
    const bool lockOneArmyAtATime = session.lockOneArmyAtATime;

    session = ArmySession();
    session.lockOneArmyAtATime = lockOneArmyAtATime;
}
