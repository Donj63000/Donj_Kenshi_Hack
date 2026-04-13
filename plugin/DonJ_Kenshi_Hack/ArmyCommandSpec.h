#pragma once

#include "ArmySession.h"

#include <array>

struct ArmyPreflightCode
{
    enum Type
    {
        Ok,
        GameNotLoaded,
        LeaderUnavailable,
        ArmyAlreadyActive,
        MissingTemplates,
        SpawnSystemUnavailable,
        FactoryUnavailable,
        ReplayHookUnavailable
    };
};

struct ArmyCommandSpec
{
    const char* commandName;
    const char* displayName;
    int unitCount;
    float durationSeconds;
    bool singleArmyAtATime;
    std::array<const char*, 3> templateNames;
};

inline const ArmyCommandSpec& GetArmyCommandSpec()
{
    static const ArmyCommandSpec spec = {
        "/army",
        "Invocation de l'armee des morts",
        30,
        180.0f,
        true,
        { {
            "DonJ_ArmyOfDead_Warrior_A",
            "DonJ_ArmyOfDead_Warrior_B",
            "DonJ_ArmyOfDead_Warrior_C"
        } }
    };

    return spec;
}

inline const char* ToStatusLabel(ArmyState::Type state)
{
    switch (state)
    {
    case ArmyState::Idle:
        return "inactive";
    case ArmyState::Preparing:
        return "en preparation";
    case ArmyState::Spawning:
        return "en spawn";
    case ArmyState::Active:
        return "active";
    case ArmyState::Dismissing:
        return "en nettoyage";
    default:
        return "inconnu";
    }
}

inline const char* ToPreflightMessage(ArmyPreflightCode::Type code)
{
    switch (code)
    {
    case ArmyPreflightCode::Ok:
        return "[OK] /army prete.";
    case ArmyPreflightCode::GameNotLoaded:
        return "[INFO] /army refusee : aucune partie chargee.";
    case ArmyPreflightCode::LeaderUnavailable:
        return "[INFO] /army refusee : leader introuvable.";
    case ArmyPreflightCode::ArmyAlreadyActive:
        return "[INFO] /army refusee : une armee est deja active.";
    case ArmyPreflightCode::MissingTemplates:
        return "[ERREUR] Le template DonJ_ArmyOfDead_Warrior est introuvable.";
    case ArmyPreflightCode::SpawnSystemUnavailable:
        return "[INFO] /army refusee : systeme de spawn non initialise.";
    case ArmyPreflightCode::FactoryUnavailable:
        return "[INFO] /army refusee : factory Kenshi indisponible.";
    case ArmyPreflightCode::ReplayHookUnavailable:
        return "[INFO] /army refusee : hook de replay Kenshi indisponible.";
    default:
        return "[ERREUR] /army refusee : verification inconnue.";
    }
}
