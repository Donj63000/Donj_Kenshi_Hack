#pragma once

#include "ArmySession.h"

#include <array>
#include <deque>
#include <functional>
#include <string>

class Character;
class Faction;

struct SpawnPosition
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class SpawnAttemptOutcome
{
    Spawned,
    DeferredAwaitingReplayHook,
    DeferredAwaitingReplayOpportunity,
    DeferredFactoryUnavailable,
    FailedTemplateMissing,
    FailedFactionUnavailable,
    FailedSpawnOriginUnavailable,
    FailedFactoryCall,
    FailedFactoryCallFatal
};

struct SpawnAttemptResult
{
    SpawnAttemptOutcome outcome = SpawnAttemptOutcome::DeferredFactoryUnavailable;
    Character* character = nullptr;
    bool shouldRequeue = false;
    std::string detail;
};

struct SpawnManagerConfig
{
    // Par defaut, on reste volontairement conservateur : une seule tentative par tick.
    int maxAttemptsPerTick = 1;
    int maxTotalAttempts = 200;
    float reminderIntervalSeconds = 5.0f;
    std::array<int, 4> validationWaveTargets = { 1, 3, 10, 30 };
};

struct SpawnManagerEnvironment
{
    std::function<bool()> isGameLoaded = []() { return false; };
    std::function<bool()> isFactoryAvailable = []() { return false; };
    std::function<bool()> isReplayHookInstalled = []() { return false; };
    std::function<bool()> hasNaturalSpawnOpportunity = []() { return false; };
    std::function<void*(const std::string&)> resolveTemplate = [](const std::string&) { return static_cast<void*>(nullptr); };
    std::function<Faction*()> resolvePlayerFaction = []() { return static_cast<Faction*>(nullptr); };
    std::function<bool(SpawnPosition&)> resolveSpawnOrigin = [](SpawnPosition&) { return false; };
    std::function<SpawnAttemptResult(const SpawnRequest&, void*, Faction*, const SpawnPosition&)> trySpawnThroughFactory;
    std::function<bool(ArmySession&, const SpawnRequest&, Character*)> onUnitSpawned;
    std::function<void(const std::string&)> logInfo = [](const std::string&) {};
    std::function<void(const std::string&)> logError = [](const std::string&) {};
    std::function<void(const std::string&)> traceDebug = [](const std::string&) {};
};

class SpawnManager
{
public:
    void SetConfig(const SpawnManagerConfig& config);
    void SetEnvironment(const SpawnManagerEnvironment& environment);

    void Reset();
    bool IsConfigured() const;
    bool HasPendingRequests() const;
    std::size_t GetPendingRequestCount() const;
    const char* GetModeLabel() const;

    bool AdoptSessionRequests(ArmySession& session);
    std::size_t Tick(ArmySession& session, float deltaSeconds);

private:
    static int DetermineWaveTarget(const SpawnManagerConfig& config, int requestedCount, int alreadySpawned);
    void SyncPendingCount(ArmySession& session) const;
    void AdvanceWaveIfNeeded(ArmySession& session);
    void LogWaveProgress(const ArmySession& session) const;
    void LogDeferredReplayHint();

    SpawnManagerConfig config_;
    SpawnManagerEnvironment environment_;
    std::deque<SpawnRequest> spawnQueue_;
    float reminderAccumulator_ = 0.0f;
    bool populatedAreaHintIssued_ = false;
};
