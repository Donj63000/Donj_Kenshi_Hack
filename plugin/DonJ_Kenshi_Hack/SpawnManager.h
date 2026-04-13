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
    float x;
    float y;
    float z;

    SpawnPosition()
        : x(0.0f)
        , y(0.0f)
        , z(0.0f)
    {
    }

    SpawnPosition(float xValue, float yValue, float zValue)
        : x(xValue)
        , y(yValue)
        , z(zValue)
    {
    }
};

struct SpawnAttemptOutcome
{
    enum Type
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
};

struct SpawnAttemptResult
{
    SpawnAttemptOutcome::Type outcome;
    Character* character;
    bool shouldRequeue;
    std::string detail;

    SpawnAttemptResult()
        : outcome(SpawnAttemptOutcome::DeferredFactoryUnavailable)
        , character(NULL)
        , shouldRequeue(false)
    {
    }
};

struct SpawnManagerConfig
{
    int maxAttemptsPerTick;
    int maxTotalAttempts;
    float reminderIntervalSeconds;
    std::array<int, 4> validationWaveTargets;

    SpawnManagerConfig()
        : maxAttemptsPerTick(1)
        , maxTotalAttempts(200)
        , reminderIntervalSeconds(5.0f)
    {
        // La je reste volontairement conservateur : une seule tentative par tick.
        validationWaveTargets[0] = 1;
        validationWaveTargets[1] = 3;
        validationWaveTargets[2] = 10;
        validationWaveTargets[3] = 30;
    }
};

struct SpawnManagerEnvironment
{
    std::function<bool()> isGameLoaded;
    std::function<bool()> isFactoryAvailable;
    std::function<bool()> isReplayHookInstalled;
    std::function<bool()> hasNaturalSpawnOpportunity;
    std::function<void*(const std::string&)> resolveTemplate;
    std::function<Faction*()> resolvePlayerFaction;
    std::function<bool(SpawnPosition&)> resolveSpawnOrigin;
    std::function<SpawnAttemptResult(const SpawnRequest&, void*, Faction*, const SpawnPosition&)> trySpawnThroughFactory;
    std::function<bool(ArmySession&, const SpawnRequest&, Character*)> onUnitSpawned;
    std::function<void(const std::string&)> logInfo;
    std::function<void(const std::string&)> logError;
    std::function<void(const std::string&)> traceDebug;

    SpawnManagerEnvironment()
        : isGameLoaded([]() { return false; })
        , isFactoryAvailable([]() { return false; })
        , isReplayHookInstalled([]() { return false; })
        , hasNaturalSpawnOpportunity([]() { return false; })
        , resolveTemplate([](const std::string&) { return static_cast<void*>(NULL); })
        , resolvePlayerFaction([]() { return static_cast<Faction*>(NULL); })
        , resolveSpawnOrigin([](SpawnPosition&) { return false; })
        , logInfo([](const std::string&) {})
        , logError([](const std::string&) {})
        , traceDebug([](const std::string&) {})
    {
    }

    SpawnManagerEnvironment(
        const std::function<bool()>& isGameLoadedValue,
        const std::function<bool()>& isFactoryAvailableValue,
        const std::function<bool()>& isReplayHookInstalledValue,
        const std::function<bool()>& hasNaturalSpawnOpportunityValue,
        const std::function<void*(const std::string&)>& resolveTemplateValue,
        const std::function<Faction*()>& resolvePlayerFactionValue,
        const std::function<bool(SpawnPosition&)>& resolveSpawnOriginValue,
        const std::function<SpawnAttemptResult(const SpawnRequest&, void*, Faction*, const SpawnPosition&)>& trySpawnThroughFactoryValue = std::function<SpawnAttemptResult(const SpawnRequest&, void*, Faction*, const SpawnPosition&)>(),
        const std::function<bool(ArmySession&, const SpawnRequest&, Character*)>& onUnitSpawnedValue = std::function<bool(ArmySession&, const SpawnRequest&, Character*)>(),
        const std::function<void(const std::string&)>& logInfoValue = std::function<void(const std::string&)>(),
        const std::function<void(const std::string&)>& logErrorValue = std::function<void(const std::string&)>(),
        const std::function<void(const std::string&)>& traceDebugValue = std::function<void(const std::string&)>())
        : isGameLoaded(isGameLoadedValue ? isGameLoadedValue : std::function<bool()>([]() { return false; }))
        , isFactoryAvailable(isFactoryAvailableValue ? isFactoryAvailableValue : std::function<bool()>([]() { return false; }))
        , isReplayHookInstalled(isReplayHookInstalledValue ? isReplayHookInstalledValue : std::function<bool()>([]() { return false; }))
        , hasNaturalSpawnOpportunity(hasNaturalSpawnOpportunityValue ? hasNaturalSpawnOpportunityValue : std::function<bool()>([]() { return false; }))
        , resolveTemplate(resolveTemplateValue ? resolveTemplateValue : std::function<void*(const std::string&)>([](const std::string&) { return static_cast<void*>(NULL); }))
        , resolvePlayerFaction(resolvePlayerFactionValue ? resolvePlayerFactionValue : std::function<Faction*()>([]() { return static_cast<Faction*>(NULL); }))
        , resolveSpawnOrigin(resolveSpawnOriginValue ? resolveSpawnOriginValue : std::function<bool(SpawnPosition&)>([](SpawnPosition&) { return false; }))
        , trySpawnThroughFactory(trySpawnThroughFactoryValue)
        , onUnitSpawned(onUnitSpawnedValue)
        , logInfo(logInfoValue ? logInfoValue : std::function<void(const std::string&)>([](const std::string&) {}))
        , logError(logErrorValue ? logErrorValue : std::function<void(const std::string&)>([](const std::string&) {}))
        , traceDebug(traceDebugValue ? traceDebugValue : std::function<void(const std::string&)>([](const std::string&) {}))
    {
    }
};

class SpawnManager
{
public:
    SpawnManager();
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
    float reminderAccumulator_;
    bool populatedAreaHintIssued_;
};
