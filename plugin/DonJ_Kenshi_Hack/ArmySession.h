#pragma once

#include "Compatibility.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class Character;

typedef std::uint64_t ArmyHandleId;

struct ArmyState
{
    enum Type
    {
        Idle,
        Preparing,
        Spawning,
        Active,
        Dismissing
    };
};

struct SpawnRequest
{
    std::string templateName;
    int index;

    SpawnRequest()
        : index(0)
    {
    }

    SpawnRequest(const std::string& templateNameValue, int indexValue)
        : templateName(templateNameValue)
        , index(indexValue)
    {
    }
};

struct PendingSpawnFinalize
{
    SpawnRequest request;
    ArmyHandleId handleId;
    int retryCount;

    PendingSpawnFinalize()
        : handleId(0)
        , retryCount(0)
    {
    }

    PendingSpawnFinalize(const SpawnRequest& requestValue, ArmyHandleId handleIdValue, int retryCountValue)
        : request(requestValue)
        , handleId(handleIdValue)
        , retryCount(retryCountValue)
    {
    }
};

struct ArmySession
{
    ArmyState::Type state;
    int requestedCount;
    int spawnedCount;
    int pendingRequestCount;
    int totalSpawnAttempts;
    int failedSpawnAttempts;
    int deferredSpawnAttempts;
    int currentWaveTarget;
    float durationSeconds;
    float remainingSeconds;
    float escortRefreshAccumulator;
    bool active;
    bool lockOneArmyAtATime;
    bool waitingForReplayOpportunity;
    bool factionBootstrappedFromLeader;
    ArmyHandleId leaderHandleId;
    ArmyHandleId leaderPlatoonHandleId;
    std::deque<SpawnRequest> pendingRequests;
    std::deque<PendingSpawnFinalize> pendingFinalizeUnits;
    std::vector<ArmyHandleId> activeUnitHandleIds;
    std::vector<Character*> activeUnits;

    ArmySession()
        : state(ArmyState::Idle)
        , requestedCount(30)
        , spawnedCount(0)
        , pendingRequestCount(0)
        , totalSpawnAttempts(0)
        , failedSpawnAttempts(0)
        , deferredSpawnAttempts(0)
        , currentWaveTarget(0)
        , durationSeconds(180.0f)
        , remainingSeconds(0.0f)
        , escortRefreshAccumulator(0.0f)
        , active(false)
        , lockOneArmyAtATime(true)
        , waitingForReplayOpportunity(false)
        , factionBootstrappedFromLeader(false)
        , leaderHandleId(0)
        , leaderPlatoonHandleId(0)
    {
    }
};

inline const char* ToString(ArmyState::Type state)
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
