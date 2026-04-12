#include "ArmyDiagnostics.h"
#include "ArmyRuntimeManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kBaseFormationRadius = 2.0f;
    constexpr float kRingSpacing = 2.0f;
    constexpr float kMaxTeleportCatchupDistance = 25.0f;
    constexpr float kEscortRefreshIntervalSeconds = 0.50f;

    float ComputeDistanceSquared(const SpawnPosition& a, const SpawnPosition& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return (dx * dx) + (dy * dy) + (dz * dz);
    }

    void TraceDebug(const ArmyRuntimeEnvironment& environment, const std::string& message)
    {
        if (environment.traceDebug)
        {
            environment.traceDebug(message);
        }
    }
}

void ArmyRuntimeManager::SetEnvironment(const ArmyRuntimeEnvironment& environment)
{
    environment_ = environment;
}

bool ArmyRuntimeManager::IsConfigured() const
{
    return
        static_cast<bool>(environment_.isGameLoaded) &&
        static_cast<bool>(environment_.resolveLeader) &&
        static_cast<bool>(environment_.resolveLeaderHandleId) &&
        static_cast<bool>(environment_.resolveCurrentPlayerPlatoon) &&
        static_cast<bool>(environment_.resolveActivePlatoon) &&
        static_cast<bool>(environment_.resolvePlayerFaction) &&
        static_cast<bool>(environment_.resolveLeaderFaction) &&
        static_cast<bool>(environment_.getCharacterHandleId) &&
        static_cast<bool>(environment_.getPlatoonHandleId) &&
        static_cast<bool>(environment_.resolveCharacterHandleId) &&
        static_cast<bool>(environment_.isCharacterDead) &&
        static_cast<bool>(environment_.isCharacterUnconscious) &&
        static_cast<bool>(environment_.getCharacterPosition) &&
        static_cast<bool>(environment_.applyFaction) &&
        static_cast<bool>(environment_.teleportCharacter) &&
        static_cast<bool>(environment_.setFollowTarget) &&
        static_cast<bool>(environment_.setEscortOrder) &&
        static_cast<bool>(environment_.logInfo) &&
        static_cast<bool>(environment_.logError);
}

bool ArmyRuntimeManager::CaptureLeaderContext(ArmySession& session) const
{
    TraceDebug(
        environment_,
        std::string("[TRACE] ArmyRuntimeManager::CaptureLeaderContext entree | ") +
            BuildArmySessionDebugLine(session));

    Character* leader = environment_.resolveLeader();
    if (leader == nullptr)
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::CaptureLeaderContext echec : leader nul.");
        return false;
    }

    session.leaderHandleId = environment_.resolveLeaderHandleId();
    if (session.leaderHandleId == 0)
    {
        session.leaderHandleId = environment_.getCharacterHandleId(leader);
    }
    if (session.leaderHandleId == 0)
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::CaptureLeaderContext echec : handle leader nul.");
        return false;
    }

    Platoon* playerPlatoon = environment_.resolveCurrentPlayerPlatoon();
    session.leaderPlatoonHandleId = environment_.getPlatoonHandleId(playerPlatoon);
    session.escortRefreshAccumulator = 0.0f;
    TraceDebug(
        environment_,
        std::string("[TRACE] ArmyRuntimeManager::CaptureLeaderContext succes | ") +
            BuildArmySessionDebugLine(session));
    return true;
}

bool ArmyRuntimeManager::ConfigureSpawnedUnit(ArmySession& session, const SpawnRequest& request, Character* character)
{
    if (character == nullptr)
    {
        environment_.logError("[ERREUR] Post-traitement d'invocation : personnage nul.");
        return false;
    }

    Character* leader = environment_.resolveLeader();
    if (leader == nullptr && session.leaderHandleId != 0)
    {
        leader = environment_.resolveCharacterHandleId(session.leaderHandleId);
    }

    if (leader == nullptr)
    {
        environment_.logError("[ERREUR] Post-traitement d'invocation : leader introuvable.");
        return false;
    }

    if (session.leaderHandleId == 0 && !CaptureLeaderContext(session))
    {
        environment_.logError("[ERREUR] Post-traitement d'invocation : impossible de capturer le contexte du leader.");
        return false;
    }

    Faction* faction = environment_.resolvePlayerFaction();
    if (faction == nullptr)
    {
        faction = environment_.resolveLeaderFaction(leader);
        if (faction != nullptr && !session.factionBootstrappedFromLeader)
        {
            session.factionBootstrappedFromLeader = true;
            environment_.logInfo("[INFO] Faction Bootstrap : faction joueur resolue depuis le leader.");
        }
    }

    if (faction == nullptr)
    {
        environment_.logError("[ERREUR] Post-traitement d'invocation : faction joueur introuvable.");
        return false;
    }

    Platoon* currentPlayerPlatoon = environment_.resolveCurrentPlayerPlatoon();
    ActivePlatoon* activePlatoon = environment_.resolveActivePlatoon(currentPlayerPlatoon);
    const SpawnPosition leaderPosition = environment_.getCharacterPosition(leader);
    const SpawnPosition formationPosition = ComputeFormationPosition(leaderPosition, request.index);

    environment_.applyFaction(character, faction, activePlatoon);

    if (environment_.renameCharacter)
    {
        char nameBuffer[96] = {};
        std::snprintf(
            nameBuffer,
            sizeof(nameBuffer),
            "Armee des morts %02d",
            request.index + 1);
        environment_.renameCharacter(character, nameBuffer);
    }

    environment_.teleportCharacter(character, formationPosition);

    // IMPORTANT :
    // On ne pousse pas encore les ordres d'escorte ici.
    // La creation du personnage vient juste d'avoir lieu dans un contexte
    // factory sensible. On limite donc le post-traitement immediat a :
    // - faction
    // - nom
    // - position
    //
    // Les ordres "follow / protect / rethink AI" seront reappliques un peu plus
    // tard depuis le game tick normal, ce qui reduit fortement le risque de crash.
    session.escortRefreshAccumulator = kEscortRefreshIntervalSeconds;

    const ArmyHandleId characterHandleId = environment_.getCharacterHandleId(character);
    if (characterHandleId != 0)
    {
        session.activeUnitHandleIds.push_back(characterHandleId);
    }

    char logMessage[256] = {};
    std::snprintf(
        logMessage,
        sizeof(logMessage),
        "[INFO] Escorte armee : unite %d creee, faction appliquee et positionnee. Escorte differee au tick suivant.",
        request.index + 1);
    environment_.logInfo(logMessage);
    return true;
}

SpawnPosition ArmyRuntimeManager::ComputeFormationPosition(const SpawnPosition& leaderPosition, int formationIndex) const
{
    int safeIndex = std::max(formationIndex, 0);
    int ringIndex = 0;
    int slotsInRing = 6;

    while (safeIndex >= slotsInRing)
    {
        safeIndex -= slotsInRing;
        ++ringIndex;
        slotsInRing += 4;
    }

    const float radius = std::min(8.0f, kBaseFormationRadius + (static_cast<float>(ringIndex) * kRingSpacing));
    const float angleStep = (2.0f * kPi) / static_cast<float>(slotsInRing);
    const float angle = (angleStep * static_cast<float>(safeIndex)) + (static_cast<float>(ringIndex) * 0.35f);

    SpawnPosition position = leaderPosition;
    position.x += std::cos(angle) * radius;
    position.z += std::sin(angle) * radius;
    return position;
}

void ArmyRuntimeManager::Tick(ArmySession& session, float deltaSeconds)
{
    if (session.state == ArmyState::Idle)
    {
        return;
    }

    if (session.state == ArmyState::Spawning || session.state == ArmyState::Dismissing || session.leaderHandleId == 0)
    {
        char traceBuffer[768] = {};
        std::snprintf(
            traceBuffer,
            sizeof(traceBuffer),
            "[TRACE] ArmyRuntimeManager::Tick entree | dt=%.3f | %s",
            static_cast<double>(deltaSeconds),
            BuildArmySessionDebugLine(session).c_str());
        TraceDebug(environment_, traceBuffer);
    }

    if (session.state == ArmyState::Dismissing)
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::Tick : finalisation immediate du dismiss.");
        FinalizeDismiss(session);
        return;
    }

    if (session.state == ArmyState::Spawning &&
        session.spawnedCount == 0 &&
        session.activeUnits.empty() &&
        session.activeUnitHandleIds.empty())
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::Tick : attente passive, aucune unite materialisee pour l'instant.");
        return;
    }

    if (!environment_.isGameLoaded())
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::Tick : partie non chargee, dismiss defensif.");
        BeginDismiss(session, "partie rechargee ou monde incoherent.");
        FinalizeDismiss(session);
        return;
    }

    Character* leader = nullptr;
    if (!IsLeaderContextStillValid(session, leader))
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::Tick : contexte leader invalide.");
        BeginDismiss(session, "leader introuvable, KO ou escouade changee.");
        FinalizeDismiss(session);
        return;
    }

    PruneInactiveUnits(session);
    if (session.spawnedCount > 0 && session.pendingRequestCount == 0 && session.activeUnitHandleIds.empty())
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::Tick : plus aucune unite valide, dismiss defensif.");
        BeginDismiss(session, "toutes les invocations ont disparu avant la fin du timer.");
        FinalizeDismiss(session);
        return;
    }

    if (session.activeUnits.empty())
    {
        if (session.state == ArmyState::Spawning)
        {
            TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::Tick : aucune unite active, attente de materialisation.");
        }
        return;
    }

    session.escortRefreshAccumulator += std::max(deltaSeconds, 0.0f);
    if (session.escortRefreshAccumulator < kEscortRefreshIntervalSeconds)
    {
        return;
    }

    session.escortRefreshAccumulator = 0.0f;
    RefreshEscortOrders(session, leader);
}

bool ArmyRuntimeManager::IsLeaderContextStillValid(ArmySession& session, Character*& leader) const
{
    if (session.leaderHandleId == 0 && !CaptureLeaderContext(session))
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::IsLeaderContextStillValid echec : impossible de capturer le leader.");
        return false;
    }

    leader = environment_.resolveCharacterHandleId(session.leaderHandleId);
    if (leader == nullptr)
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::IsLeaderContextStillValid echec : leader non resolu depuis le handle.");
        return false;
    }

    if (environment_.isCharacterDead(leader) || environment_.isCharacterUnconscious(leader))
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::IsLeaderContextStillValid echec : leader mort ou KO.");
        return false;
    }

    if (session.leaderPlatoonHandleId != 0)
    {
        Platoon* currentPlatoon = environment_.resolveCurrentPlayerPlatoon();
        const ArmyHandleId currentPlatoonHandleId = environment_.getPlatoonHandleId(currentPlatoon);
        if (currentPlatoonHandleId == 0 || currentPlatoonHandleId != session.leaderPlatoonHandleId)
        {
            TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::IsLeaderContextStillValid echec : platoon courant different.");
            return false;
        }
    }

    if (session.state == ArmyState::Spawning && session.activeUnits.empty())
    {
        TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::IsLeaderContextStillValid succes pendant le spawn.");
    }
    return true;
}

void ArmyRuntimeManager::PruneInactiveUnits(ArmySession& session) const
{
    std::vector<ArmyHandleId> validHandles;
    std::vector<Character*> validUnits;
    validHandles.reserve(session.activeUnitHandleIds.size());
    validUnits.reserve(session.activeUnitHandleIds.size());

    for (ArmyHandleId handleId : session.activeUnitHandleIds)
    {
        if (handleId == 0)
        {
            continue;
        }

        Character* character = environment_.resolveCharacterHandleId(handleId);
        if (character == nullptr)
        {
            continue;
        }

        if (environment_.isCharacterDead(character))
        {
            continue;
        }

        validHandles.push_back(handleId);
        validUnits.push_back(character);
    }

    session.activeUnitHandleIds.swap(validHandles);
    session.activeUnits.swap(validUnits);
}

void ArmyRuntimeManager::RefreshEscortOrders(ArmySession& session, Character* leader) const
{
    const SpawnPosition leaderPosition = environment_.getCharacterPosition(leader);

    for (std::size_t index = 0; index < session.activeUnits.size(); ++index)
    {
        Character* unit = session.activeUnits[index];
        if (unit == nullptr)
        {
            continue;
        }

        const SpawnPosition desiredPosition = ComputeFormationPosition(
            leaderPosition,
            static_cast<int>(index));

        const SpawnPosition currentPosition = environment_.getCharacterPosition(unit);
        if (environment_.teleportCharacter &&
            ComputeDistanceSquared(currentPosition, desiredPosition) >
                (kMaxTeleportCatchupDistance * kMaxTeleportCatchupDistance))
        {
            environment_.teleportCharacter(unit, desiredPosition);
        }

        if (environment_.setEscortRole)
        {
            environment_.setEscortRole(unit);
        }

        environment_.setEscortOrder(unit, ArmyEscortOrder::DefensiveCombat);
        environment_.setEscortOrder(unit, ArmyEscortOrder::ChaseTarget);
        environment_.setFollowTarget(unit, leader);

        if (environment_.rethinkAi)
        {
            environment_.rethinkAi(unit);
        }
    }
}

void ArmyRuntimeManager::BeginDismiss(ArmySession& session, const std::string& reason) const
{
    if (session.state == ArmyState::Dismissing)
    {
        return;
    }

    session.state = ArmyState::Dismissing;
    session.active = false;
    session.remainingSeconds = 0.0f;

    if (!reason.empty())
    {
        environment_.logInfo(std::string("[INFO] Invocation dissoute : ") + reason);
    }

    TraceDebug(
        environment_,
        std::string("[TRACE] ArmyRuntimeManager::BeginDismiss | raison=") +
            reason +
            " | " +
            BuildArmySessionDebugLine(session));
}

void ArmyRuntimeManager::FinalizeDismiss(ArmySession& session)
{
    if (session.state != ArmyState::Dismissing)
    {
        return;
    }

    std::vector<Character*> charactersToDismiss;
    charactersToDismiss.reserve(session.activeUnitHandleIds.size() + session.activeUnits.size());

    for (ArmyHandleId handleId : session.activeUnitHandleIds)
    {
        Character* character = environment_.resolveCharacterHandleId(handleId);
        if (character != nullptr &&
            std::find(charactersToDismiss.begin(), charactersToDismiss.end(), character) == charactersToDismiss.end())
        {
            charactersToDismiss.push_back(character);
        }
    }

    for (Character* character : session.activeUnits)
    {
        if (character != nullptr &&
            std::find(charactersToDismiss.begin(), charactersToDismiss.end(), character) == charactersToDismiss.end())
        {
            charactersToDismiss.push_back(character);
        }
    }

    if (environment_.dismissCharacter)
    {
        for (Character* character : charactersToDismiss)
        {
            environment_.dismissCharacter(character);
        }
    }

    TraceDebug(
        environment_,
        std::string("[TRACE] ArmyRuntimeManager::FinalizeDismiss : unites a dissoudre=") +
            std::to_string(charactersToDismiss.size()));

    ResetArmySession(session);
    TraceDebug(environment_, "[TRACE] ArmyRuntimeManager::FinalizeDismiss : session reinitialisee.");
}
