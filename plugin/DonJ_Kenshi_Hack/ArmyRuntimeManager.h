#pragma once

#include "ArmySession.h"
#include "SpawnManager.h"

#include <functional>
#include <string>

class ActivePlatoon;
class Character;
class Faction;
class Platoon;

struct ArmyEscortOrder
{
    enum Type
    {
        DefensiveCombat,
        ChaseTarget
    };

    Type value;

    ArmyEscortOrder(Type orderValue = DefensiveCombat)
        : value(orderValue)
    {
    }

    operator Type() const
    {
        return value;
    }
};

struct ArmyRuntimeEnvironment
{
    std::function<bool()> isGameLoaded;
    std::function<Character*()> resolveLeader;
    std::function<Platoon*()> resolveCurrentPlayerPlatoon;
    std::function<ActivePlatoon*(Platoon*)> resolveActivePlatoon;
    std::function<Faction*()> resolvePlayerFaction;
    std::function<Faction*(Character*)> resolveLeaderFaction;
    std::function<ArmyHandleId(Character*)> getCharacterHandleId;
    std::function<ArmyHandleId(Platoon*)> getPlatoonHandleId;
    std::function<Character*(ArmyHandleId)> resolveCharacterHandleId;
    std::function<bool(Character*)> isCharacterDead;
    std::function<bool(Character*)> isCharacterUnconscious;
    std::function<SpawnPosition(Character*)> getCharacterPosition;
    std::function<void(Character*, Faction*, ActivePlatoon*)> applyFaction;
    std::function<void(Character*, const std::string&)> renameCharacter;
    std::function<void(Character*, const SpawnPosition&)> teleportCharacter;
    std::function<void(Character*, Character*)> setFollowTarget;
    std::function<void(Character*, ArmyEscortOrder)> setEscortOrder;
    std::function<void(Character*)> setEscortRole;
    std::function<void(Character*)> rethinkAi;
    std::function<void(const std::string&)> logInfo;
    std::function<void(const std::string&)> logError;
    std::function<void(const std::string&)> traceDebug;
    std::function<ArmyHandleId()> resolveLeaderHandleId;
    std::function<void(Character*)> dismissCharacter;

    ArmyRuntimeEnvironment()
        : isGameLoaded([]() { return false; })
        , resolveLeader([]() { return static_cast<Character*>(NULL); })
        , resolveCurrentPlayerPlatoon([]() { return static_cast<Platoon*>(NULL); })
        , resolveActivePlatoon([](Platoon*) { return static_cast<ActivePlatoon*>(NULL); })
        , resolvePlayerFaction([]() { return static_cast<Faction*>(NULL); })
        , resolveLeaderFaction([](Character*) { return static_cast<Faction*>(NULL); })
        , getCharacterHandleId([](Character*) { return static_cast<ArmyHandleId>(0); })
        , getPlatoonHandleId([](Platoon*) { return static_cast<ArmyHandleId>(0); })
        , resolveCharacterHandleId([](ArmyHandleId) { return static_cast<Character*>(NULL); })
        , isCharacterDead([](Character*) { return false; })
        , isCharacterUnconscious([](Character*) { return false; })
        , getCharacterPosition([](Character*) { return SpawnPosition(); })
        , logInfo([](const std::string&) {})
        , logError([](const std::string&) {})
        , traceDebug([](const std::string&) {})
        , resolveLeaderHandleId([]() { return static_cast<ArmyHandleId>(0); })
        , dismissCharacter([](Character*) {})
    {
    }

    ArmyRuntimeEnvironment(
        const std::function<bool()>& isGameLoadedValue,
        const std::function<Character*()>& resolveLeaderValue,
        const std::function<Platoon*()>& resolveCurrentPlayerPlatoonValue,
        const std::function<ActivePlatoon*(Platoon*)>& resolveActivePlatoonValue,
        const std::function<Faction*()>& resolvePlayerFactionValue,
        const std::function<Faction*(Character*)>& resolveLeaderFactionValue,
        const std::function<ArmyHandleId(Character*)>& getCharacterHandleIdValue,
        const std::function<ArmyHandleId(Platoon*)>& getPlatoonHandleIdValue,
        const std::function<Character*(ArmyHandleId)>& resolveCharacterHandleIdValue,
        const std::function<bool(Character*)>& isCharacterDeadValue,
        const std::function<bool(Character*)>& isCharacterUnconsciousValue,
        const std::function<SpawnPosition(Character*)>& getCharacterPositionValue,
        const std::function<void(Character*, Faction*, ActivePlatoon*)>& applyFactionValue,
        const std::function<void(Character*, const std::string&)>& renameCharacterValue = std::function<void(Character*, const std::string&)>(),
        const std::function<void(Character*, const SpawnPosition&)>& teleportCharacterValue = std::function<void(Character*, const SpawnPosition&)>(),
        const std::function<void(Character*, Character*)>& setFollowTargetValue = std::function<void(Character*, Character*)>(),
        const std::function<void(Character*, ArmyEscortOrder)>& setEscortOrderValue = std::function<void(Character*, ArmyEscortOrder)>(),
        const std::function<void(Character*)>& setEscortRoleValue = std::function<void(Character*)>(),
        const std::function<void(Character*)>& rethinkAiValue = std::function<void(Character*)>(),
        const std::function<void(const std::string&)>& logInfoValue = std::function<void(const std::string&)>(),
        const std::function<void(const std::string&)>& logErrorValue = std::function<void(const std::string&)>(),
        const std::function<void(const std::string&)>& traceDebugValue = std::function<void(const std::string&)>(),
        const std::function<ArmyHandleId()>& resolveLeaderHandleIdValue = std::function<ArmyHandleId()>(),
        const std::function<void(Character*)>& dismissCharacterValue = std::function<void(Character*)>())
        : isGameLoaded(isGameLoadedValue ? isGameLoadedValue : std::function<bool()>([]() { return false; }))
        , resolveLeader(resolveLeaderValue ? resolveLeaderValue : std::function<Character*()>([]() { return static_cast<Character*>(NULL); }))
        , resolveCurrentPlayerPlatoon(resolveCurrentPlayerPlatoonValue ? resolveCurrentPlayerPlatoonValue : std::function<Platoon*()>([]() { return static_cast<Platoon*>(NULL); }))
        , resolveActivePlatoon(resolveActivePlatoonValue ? resolveActivePlatoonValue : std::function<ActivePlatoon*(Platoon*)>([](Platoon*) { return static_cast<ActivePlatoon*>(NULL); }))
        , resolvePlayerFaction(resolvePlayerFactionValue ? resolvePlayerFactionValue : std::function<Faction*()>([]() { return static_cast<Faction*>(NULL); }))
        , resolveLeaderFaction(resolveLeaderFactionValue ? resolveLeaderFactionValue : std::function<Faction*(Character*)>([](Character*) { return static_cast<Faction*>(NULL); }))
        , getCharacterHandleId(getCharacterHandleIdValue ? getCharacterHandleIdValue : std::function<ArmyHandleId(Character*)>([](Character*) { return static_cast<ArmyHandleId>(0); }))
        , getPlatoonHandleId(getPlatoonHandleIdValue ? getPlatoonHandleIdValue : std::function<ArmyHandleId(Platoon*)>([](Platoon*) { return static_cast<ArmyHandleId>(0); }))
        , resolveCharacterHandleId(resolveCharacterHandleIdValue ? resolveCharacterHandleIdValue : std::function<Character*(ArmyHandleId)>([](ArmyHandleId) { return static_cast<Character*>(NULL); }))
        , isCharacterDead(isCharacterDeadValue ? isCharacterDeadValue : std::function<bool(Character*)>([](Character*) { return false; }))
        , isCharacterUnconscious(isCharacterUnconsciousValue ? isCharacterUnconsciousValue : std::function<bool(Character*)>([](Character*) { return false; }))
        , getCharacterPosition(getCharacterPositionValue ? getCharacterPositionValue : std::function<SpawnPosition(Character*)>([](Character*) { return SpawnPosition(); }))
        , applyFaction(applyFactionValue ? applyFactionValue : std::function<void(Character*, Faction*, ActivePlatoon*)>([](Character*, Faction*, ActivePlatoon*) {}))
        , renameCharacter(renameCharacterValue)
        , teleportCharacter(teleportCharacterValue)
        , setFollowTarget(setFollowTargetValue)
        , setEscortOrder(setEscortOrderValue)
        , setEscortRole(setEscortRoleValue)
        , rethinkAi(rethinkAiValue)
        , logInfo(logInfoValue ? logInfoValue : std::function<void(const std::string&)>([](const std::string&) {}))
        , logError(logErrorValue ? logErrorValue : std::function<void(const std::string&)>([](const std::string&) {}))
        , traceDebug(traceDebugValue ? traceDebugValue : std::function<void(const std::string&)>([](const std::string&) {}))
        , resolveLeaderHandleId(resolveLeaderHandleIdValue ? resolveLeaderHandleIdValue : std::function<ArmyHandleId()>([]() { return static_cast<ArmyHandleId>(0); }))
        , dismissCharacter(dismissCharacterValue ? dismissCharacterValue : std::function<void(Character*)>([](Character*) {}))
    {
    }
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
    void FinalizePendingSpawnUnits(ArmySession& session, Character* leader) const;
    void PruneInactiveUnits(ArmySession& session) const;
    void RefreshEscortOrders(ArmySession& session, Character* leader) const;
    void BeginDismiss(ArmySession& session, const std::string& reason) const;
    void FinalizeDismiss(ArmySession& session);

    ArmyRuntimeEnvironment environment_;
};
