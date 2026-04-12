#pragma once

#include "SpawnManager.h"

#include <cstdio>
#include <string>

inline std::string BuildArmySessionDebugLine(const ArmySession& session)
{
    char buffer[512] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "state=%s requested=%d spawned=%d pending=%d queue=%zu active_units=%zu active_handles=%zu wave=%d attempts=%d deferred=%d failed=%d active=%s waiting_replay=%s faction_bootstrap=%s leader=%llu platoon=%llu remaining=%.2fs",
        ToString(session.state),
        session.requestedCount,
        session.spawnedCount,
        session.pendingRequestCount,
        session.pendingRequests.size(),
        session.activeUnits.size(),
        session.activeUnitHandleIds.size(),
        session.currentWaveTarget,
        session.totalSpawnAttempts,
        session.deferredSpawnAttempts,
        session.failedSpawnAttempts,
        session.active ? "yes" : "no",
        session.waitingForReplayOpportunity ? "yes" : "no",
        session.factionBootstrappedFromLeader ? "yes" : "no",
        static_cast<unsigned long long>(session.leaderHandleId),
        static_cast<unsigned long long>(session.leaderPlatoonHandleId),
        static_cast<double>(session.remainingSeconds));
    return buffer;
}

inline std::string BuildSpawnRequestDebugLine(const SpawnRequest& request)
{
    char buffer[256] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "template=%s index=%d",
        request.templateName.c_str(),
        request.index);
    return buffer;
}

inline std::string BuildSpawnPositionDebugLine(const SpawnPosition& position)
{
    char buffer[160] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "(x=%.2f y=%.2f z=%.2f)",
        static_cast<double>(position.x),
        static_cast<double>(position.y),
        static_cast<double>(position.z));
    return buffer;
}

inline const char* ToString(SpawnAttemptOutcome outcome)
{
    switch (outcome)
    {
    case SpawnAttemptOutcome::Spawned:
        return "Spawned";
    case SpawnAttemptOutcome::DeferredAwaitingReplayHook:
        return "DeferredAwaitingReplayHook";
    case SpawnAttemptOutcome::DeferredAwaitingReplayOpportunity:
        return "DeferredAwaitingReplayOpportunity";
    case SpawnAttemptOutcome::DeferredFactoryUnavailable:
        return "DeferredFactoryUnavailable";
    case SpawnAttemptOutcome::FailedTemplateMissing:
        return "FailedTemplateMissing";
    case SpawnAttemptOutcome::FailedFactionUnavailable:
        return "FailedFactionUnavailable";
    case SpawnAttemptOutcome::FailedSpawnOriginUnavailable:
        return "FailedSpawnOriginUnavailable";
    case SpawnAttemptOutcome::FailedFactoryCall:
        return "FailedFactoryCall";
    case SpawnAttemptOutcome::FailedFactoryCallFatal:
        return "FailedFactoryCallFatal";
    default:
        return "Unknown";
    }
}
