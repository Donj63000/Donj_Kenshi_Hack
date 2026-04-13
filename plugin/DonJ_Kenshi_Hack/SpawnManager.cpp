#include "ArmyDiagnostics.h"
#include "SpawnManager.h"

#include <algorithm>
#include <cstdio>

namespace
{
    void WriteFormattedLog(
        const std::function<void(const std::string&)>& writer,
        const char* format,
        int valueA = 0,
        int valueB = 0)
    {
        if (!writer)
        {
            return;
        }

        char buffer[256] = {};
        DonjSnprintf(buffer, sizeof(buffer), format, valueA, valueB);
        writer(buffer);
    }

    void TraceDebug(const SpawnManagerEnvironment& environment, const std::string& message)
    {
        if (environment.traceDebug)
        {
            environment.traceDebug(message);
        }
    }
}

SpawnManager::SpawnManager()
    : reminderAccumulator_(0.0f)
    , populatedAreaHintIssued_(false)
{
}

void SpawnManager::SetConfig(const SpawnManagerConfig& config)
{
    config_ = config;
}

void SpawnManager::SetEnvironment(const SpawnManagerEnvironment& environment)
{
    environment_ = environment;
}

void SpawnManager::Reset()
{
    spawnQueue_.clear();
    reminderAccumulator_ = 0.0f;
    populatedAreaHintIssued_ = false;
}

bool SpawnManager::IsConfigured() const
{
    return
        static_cast<bool>(environment_.isGameLoaded) &&
        static_cast<bool>(environment_.isFactoryAvailable) &&
        static_cast<bool>(environment_.resolveTemplate) &&
        static_cast<bool>(environment_.resolveSpawnOrigin) &&
        static_cast<bool>(environment_.logInfo) &&
        static_cast<bool>(environment_.logError);
}

bool SpawnManager::HasPendingRequests() const
{
    return !spawnQueue_.empty();
}

std::size_t SpawnManager::GetPendingRequestCount() const
{
    return spawnQueue_.size();
}

const char* SpawnManager::GetModeLabel() const
{
    if (!environment_.isReplayHookInstalled())
    {
        return "queue only";
    }

    if (!environment_.isFactoryAvailable())
    {
        return "factory indisponible";
    }

    return "replay factory";
}

bool SpawnManager::AdoptSessionRequests(ArmySession& session)
{
    if (session.pendingRequests.empty())
    {
        SyncPendingCount(session);
        return false;
    }

    while (!session.pendingRequests.empty())
    {
        spawnQueue_.push_back(session.pendingRequests.front());
        session.pendingRequests.pop_front();
    }

    session.pendingRequestCount = static_cast<int>(spawnQueue_.size());
    session.currentWaveTarget = DetermineWaveTarget(config_, session.requestedCount, session.spawnedCount);
    session.waitingForReplayOpportunity = false;
    reminderAccumulator_ = 0.0f;
    populatedAreaHintIssued_ = false;

    WriteFormattedLog(
        environment_.logInfo,
        "[INFO] SpawnManager : %d requetes en file. Premiere sous-vague cible : %d.",
        session.pendingRequestCount,
        session.currentWaveTarget);

    return true;
}

std::size_t SpawnManager::Tick(ArmySession& session, float deltaSeconds)
{
    const float safeDeltaSeconds = std::max(deltaSeconds, 0.0f);
    reminderAccumulator_ += safeDeltaSeconds;

    if (session.state == ArmyState::Spawning || !spawnQueue_.empty() || !session.pendingRequests.empty())
    {
        char traceBuffer[768] = {};
        std::snprintf(
            traceBuffer,
            sizeof(traceBuffer),
            "[TRACE] SpawnManager::Tick entree | dt=%.3f | queue_interne=%zu | %s",
            static_cast<double>(safeDeltaSeconds),
            spawnQueue_.size(),
            BuildArmySessionDebugLine(session).c_str());
        TraceDebug(environment_, traceBuffer);
    }

    if (session.state == ArmyState::Idle || session.state == ArmyState::Dismissing)
    {
        if (!spawnQueue_.empty())
        {
            TraceDebug(environment_, "[TRACE] SpawnManager::Tick : purge defensive de la queue interne.");
            Reset();
        }

        SyncPendingCount(session);
        return 0;
    }

    if (session.state != ArmyState::Spawning)
    {
        SyncPendingCount(session);
        return 0;
    }

    if (!spawnQueue_.empty() || !session.pendingRequests.empty())
    {
        TraceDebug(
            environment_,
            std::string("[TRACE] SpawnManager::Tick adoption requetes | ") +
                BuildArmySessionDebugLine(session));
        AdoptSessionRequests(session);
        TraceDebug(
            environment_,
            std::string("[TRACE] SpawnManager::Tick adoption terminee | ") +
                BuildArmySessionDebugLine(session));
    }

    SyncPendingCount(session);
    if (spawnQueue_.empty())
    {
        if (session.spawnedCount >= session.requestedCount && session.requestedCount > 0)
        {
            session.state = ArmyState::Active;
            session.active = true;
            session.waitingForReplayOpportunity = false;
            session.remainingSeconds = session.durationSeconds;

            char buffer[192] = {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "[OK] Armee active pour %d secondes.",
                static_cast<int>(session.durationSeconds));
            environment_.logInfo(buffer);
        }

        return 0;
    }

    if (!environment_.isGameLoaded())
    {
        TraceDebug(environment_, "[TRACE] SpawnManager::Tick stop : partie non chargee.");
        return 0;
    }

    if (!environment_.isFactoryAvailable())
    {
        TraceDebug(environment_, "[TRACE] SpawnManager::Tick stop : factory Kenshi indisponible.");
        if (reminderAccumulator_ >= config_.reminderIntervalSeconds)
        {
            environment_.logInfo("[INFO] Spawn differe : factory Kenshi indisponible.");
            reminderAccumulator_ = 0.0f;
        }
        return 0;
    }

    if (!environment_.isReplayHookInstalled())
    {
        session.waitingForReplayOpportunity = true;
        TraceDebug(environment_, "[TRACE] SpawnManager::Tick stop : replay hook non installe.");
        LogDeferredReplayHint();
        return 0;
    }

    session.waitingForReplayOpportunity = false;
    session.currentWaveTarget = DetermineWaveTarget(config_, session.requestedCount, session.spawnedCount);

    if (session.totalSpawnAttempts >= config_.maxTotalAttempts)
    {
        environment_.logError("[ERREUR] /army abandonnee : budget maximal de tentatives atteint.");
        spawnQueue_.clear();
        session.pendingRequests.clear();
        session.pendingRequestCount = 0;
        session.waitingForReplayOpportunity = false;
        session.active = false;
        session.remainingSeconds = 0.0f;
        session.state = ArmyState::Dismissing;
        return 0;
    }

    int remainingWaveBudget = std::max(0, session.currentWaveTarget - session.spawnedCount);
    if (remainingWaveBudget <= 0)
    {
        AdvanceWaveIfNeeded(session);
        remainingWaveBudget = std::max(0, session.currentWaveTarget - session.spawnedCount);
    }

    std::size_t spawnedThisTick = 0;
    const int attemptBudget = std::min(config_.maxAttemptsPerTick, remainingWaveBudget);

    for (int attemptIndex = 0; attemptIndex < attemptBudget && !spawnQueue_.empty(); ++attemptIndex)
    {
        if (!environment_.hasNaturalSpawnOpportunity())
        {
            session.waitingForReplayOpportunity = true;
            ++session.deferredSpawnAttempts;
            TraceDebug(environment_, "[TRACE] SpawnManager::Tick stop : aucune opportunite de replay native sur ce tick.");
            LogDeferredReplayHint();
            break;
        }

        SpawnRequest request = spawnQueue_.front();
        spawnQueue_.pop_front();
        SyncPendingCount(session);
        ++session.totalSpawnAttempts;

        TraceDebug(
            environment_,
            std::string("[TRACE] SpawnManager::Tick tentative | ") +
                BuildSpawnRequestDebugLine(request) +
                " | " +
                BuildArmySessionDebugLine(session));

        void* templateHandle = environment_.resolveTemplate(request.templateName);
        if (templateHandle == nullptr)
        {
            ++session.failedSpawnAttempts;

            char buffer[256] = {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "[ERREUR] Le template %s est introuvable.",
                request.templateName.c_str());
            environment_.logError(buffer);
            continue;
        }

        Faction* playerFaction = nullptr;
        if (environment_.resolvePlayerFaction)
        {
            playerFaction = environment_.resolvePlayerFaction();
        }

        if (playerFaction == nullptr)
        {
            TraceDebug(environment_, "[TRACE] SpawnManager::Tick : faction joueur nulle, bootstrap leader autorise apres le spawn.");
        }

        SpawnPosition spawnOrigin;
        if (!environment_.resolveSpawnOrigin(spawnOrigin))
        {
            ++session.deferredSpawnAttempts;
            spawnQueue_.push_front(request);
            TraceDebug(environment_, "[TRACE] SpawnManager::Tick defer : position de spawn introuvable.");
            environment_.logInfo("[INFO] Spawn differe : position de spawn introuvable.");
            break;
        }

        TraceDebug(
            environment_,
            std::string("[TRACE] SpawnManager::Tick origine resolue | ") +
                BuildSpawnRequestDebugLine(request) +
                " | position=" +
                BuildSpawnPositionDebugLine(spawnOrigin));

        if (!environment_.trySpawnThroughFactory)
        {
            ++session.deferredSpawnAttempts;
            spawnQueue_.push_front(request);
            TraceDebug(environment_, "[TRACE] SpawnManager::Tick defer : callback factory absente.");
            environment_.logInfo("[INFO] Spawn differe : callback factory Kenshi absente.");
            break;
        }

        const SpawnAttemptResult result = environment_.trySpawnThroughFactory(
            request,
            templateHandle,
            playerFaction,
            spawnOrigin);

        TraceDebug(
            environment_,
            std::string("[TRACE] SpawnManager::Tick resultat | outcome=") +
                ToString(result.outcome) +
                " | requeue=" +
                (result.shouldRequeue ? "yes" : "no") +
                (result.detail.empty() ? "" : " | detail=" + result.detail));

        switch (result.outcome)
        {
        case SpawnAttemptOutcome::Spawned:
            if (result.character != nullptr && environment_.onUnitSpawned)
            {
                if (!environment_.onUnitSpawned(session, request, result.character))
                {
                    ++session.failedSpawnAttempts;
                    environment_.logError("[ERREUR] Spawn cree mais post-traitement escorte impossible.");
                    break;
                }
            }

            ++session.spawnedCount;
            ++spawnedThisTick;
            reminderAccumulator_ = 0.0f;
            LogWaveProgress(session);
            break;

        case SpawnAttemptOutcome::DeferredAwaitingReplayHook:
        case SpawnAttemptOutcome::DeferredAwaitingReplayOpportunity:
        case SpawnAttemptOutcome::DeferredFactoryUnavailable:
            ++session.deferredSpawnAttempts;
            if (result.shouldRequeue)
            {
                spawnQueue_.push_front(request);
            }
            if (!result.detail.empty())
            {
                environment_.logInfo(result.detail);
            }
            break;

        case SpawnAttemptOutcome::FailedTemplateMissing:
        case SpawnAttemptOutcome::FailedFactionUnavailable:
        case SpawnAttemptOutcome::FailedSpawnOriginUnavailable:
        case SpawnAttemptOutcome::FailedFactoryCall:
            ++session.failedSpawnAttempts;
            if (result.shouldRequeue)
            {
                spawnQueue_.push_back(request);
            }
            if (!result.detail.empty())
            {
                environment_.logError(result.detail);
            }
            break;

        case SpawnAttemptOutcome::FailedFactoryCallFatal:
            ++session.failedSpawnAttempts;
            spawnQueue_.clear();
            session.pendingRequests.clear();
            session.pendingRequestCount = 0;
            session.waitingForReplayOpportunity = false;
            session.active = false;
            session.remainingSeconds = 0.0f;
            session.state = ArmyState::Dismissing;
            if (!result.detail.empty())
            {
                environment_.logError(result.detail);
            }
            else
            {
                environment_.logError("[ERREUR] Spawn factory fatal : l'invocation est interrompue pour proteger la partie.");
            }
            TraceDebug(
                environment_,
                std::string("[TRACE] SpawnManager::Tick fatal factory error | ") +
                    BuildArmySessionDebugLine(session));
            return spawnedThisTick;
        }

        SyncPendingCount(session);
        AdvanceWaveIfNeeded(session);
    }

    if (spawnQueue_.empty() && session.spawnedCount >= session.requestedCount)
    {
        session.state = ArmyState::Active;
        session.active = true;
        session.waitingForReplayOpportunity = false;
        session.remainingSeconds = session.durationSeconds;

        char buffer[192] = {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "[OK] Armee active pour %d secondes.",
            static_cast<int>(session.durationSeconds));
        environment_.logInfo(buffer);
    }

    if (session.state == ArmyState::Spawning || session.state == ArmyState::Active)
    {
        TraceDebug(
            environment_,
            std::string("[TRACE] SpawnManager::Tick sortie | ") +
                BuildArmySessionDebugLine(session));
    }

    return spawnedThisTick;
}

int SpawnManager::DetermineWaveTarget(const SpawnManagerConfig& config, int requestedCount, int alreadySpawned)
{
    const int safeRequestedCount = std::max(requestedCount, 0);
    if (safeRequestedCount <= 0)
    {
        return 0;
    }

    for (std::size_t index = 0; index < config.validationWaveTargets.size(); ++index)
    {
        const int configuredTarget = config.validationWaveTargets[index];
        const int clampedTarget = std::min(configuredTarget, safeRequestedCount);
        if (clampedTarget > alreadySpawned)
        {
            return clampedTarget;
        }
    }

    return safeRequestedCount;
}

void SpawnManager::SyncPendingCount(ArmySession& session) const
{
    session.pendingRequestCount = static_cast<int>(spawnQueue_.size() + session.pendingRequests.size());
}

void SpawnManager::AdvanceWaveIfNeeded(ArmySession& session)
{
    if (session.spawnedCount < session.currentWaveTarget)
    {
        return;
    }

    const int nextWaveTarget = DetermineWaveTarget(config_, session.requestedCount, session.spawnedCount);
    if (nextWaveTarget <= session.currentWaveTarget)
    {
        return;
    }

    char buffer[256] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "[INFO] Sous-vague validee : %d / %d. Prochaine cible : %d.",
        session.spawnedCount,
        session.requestedCount,
        nextWaveTarget);
    environment_.logInfo(buffer);
    session.currentWaveTarget = nextWaveTarget;
}

void SpawnManager::LogWaveProgress(const ArmySession& session) const
{
    char buffer[256] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "[INFO] Spawn en cours : %d / %d unites creees.",
        session.spawnedCount,
        session.requestedCount);
    environment_.logInfo(buffer);
}

void SpawnManager::LogDeferredReplayHint()
{
    if (reminderAccumulator_ < config_.reminderIntervalSeconds)
    {
        return;
    }

    reminderAccumulator_ = 0.0f;
    environment_.logInfo("[INFO] Spawn differe : en attente d'une opportunite de replay Kenshi.");

    if (!populatedAreaHintIssued_)
    {
        environment_.logInfo("[INFO] Astuce test : approche-toi d'une zone peuplee pour declencher des creations natives.");
        populatedAreaHintIssued_ = true;
    }
}
