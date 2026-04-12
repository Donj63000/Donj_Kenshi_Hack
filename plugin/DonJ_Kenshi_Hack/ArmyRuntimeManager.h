#pragma once

#include "ArmySession.h"
#include "SpawnManager.h"

#include <functional>
#include <string>

class ActivePlatoon;
class Character;
class Faction;
class Platoon;

enum class ArmyEscortOrder
{
    DefensiveCombat,
    ChaseTarget
};

struct ArmyRuntimeEnvironment
{
    std::function<bool()> isGameLoaded = []() { return false; };
    std::function<Character*()> resolveLeader = []() { return static_cast<Character*>(nullptr); };
    std::function<Platoon*()> resolveCurrentPlayerPlatoon = []() { return static_cast<Platoon*>(nullptr); };
    std::function<ActivePlatoon*(Platoon*)> resolveActivePlatoon = [](Platoon*) { return static_cast<ActivePlatoon*>(nullptr); };
    std::function<Faction*()> resolvePlayerFaction = []() { return static_cast<Faction*>(nullptr); };
    std::function<Faction*(Character*)> resolveLeaderFaction = [](Character*) { return static_cast<Faction*>(nullptr); };
    std::function<ArmyHandleId(Character*)> getCharacterHandleId = [](Character*) { return static_cast<ArmyHandleId>(0); };
    std::function<ArmyHandleId(Platoon*)> getPlatoonHandleId = [](Platoon*) { return static_cast<ArmyHandleId>(0); };
    std::function<Character*(ArmyHandleId)> resolveCharacterHandleId = [](ArmyHandleId) { return static_cast<Character*>(nullptr); };
    std::function<bool(Character*)> isCharacterDead = [](Character*) { return false; };
    std::function<bool(Character*)> isCharacterUnconscious = [](Character*) { return false; };
    std::function<SpawnPosition(Character*)> getCharacterPosition = [](Character*) { return SpawnPosition(); };
    std::function<void(Character*, Faction*, ActivePlatoon*)> applyFaction;
    std::function<void(Character*, const std::string&)> renameCharacter;
    std::function<void(Character*, const SpawnPosition&)> teleportCharacter;
    std::function<void(Character*, Character*)> setFollowTarget;
    std::function<void(Character*, ArmyEscortOrder)> setEscortOrder;
    std::function<void(Character*)> setEscortRole;
    std::function<void(Character*)> rethinkAi;
    std::function<void(const std::string&)> logInfo = [](const std::string&) {};
    std::function<void(const std::string&)> logError = [](const std::string&) {};
    std::function<void(const std::string&)> traceDebug = [](const std::string&) {};
    std::function<ArmyHandleId()> resolveLeaderHandleId = []() { return static_cast<ArmyHandleId>(0); };
};

class ArmyRuntimeManager
{
public:
    void SetEnvironment(const ArmyRuntimeEnvironment& environment);
    bool IsConfigured() const;

    bool CaptureLeaderContext(ArmySession& session) const;
    bool ConfigureSpawnedUnit(ArmySession& session, const SpawnRequest& request, Character* character);
    SpawnPosition ComputeFormationPosition(const SpawnPosition& leaderPosition, int formationIndex) const;
    void Tick(ArmySession& session, float deltaSeconds);

private:
    bool IsLeaderContextStillValid(ArmySession& session, Character*& leader) const;
    void PruneInactiveUnits(ArmySession& session) const;
    void RefreshEscortOrders(ArmySession& session, Character* leader) const;
    void BeginDismiss(ArmySession& session, const std::string& reason) const;
    void FinalizeDismiss(ArmySession& session);

    ArmyRuntimeEnvironment environment_;
};
