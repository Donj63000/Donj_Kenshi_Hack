#include "ArmyDiagnostics.h"
#include "ArmyCommandSpec.h"
#include "ArmyRuntimeManager.h"
#include "RuntimeIdentity.h"
#include "SpawnManager.h"
#include "TerminalBackend.h"

#include <Debug.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>

#define NOMINMAX
#define BOOST_ALL_NO_LIB
#define BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_USE_WINDOWS_H
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <kenshi/Character.h>
#include <kenshi/Damages.h>
#include <kenshi/GameData.h>
#include <kenshi/CharMovement.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Platoon.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/gui/TitleScreen.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <core/Functions.h>

namespace
{
    constexpr const char* kArmyBuildStamp = __DATE__ " " __TIME__;

    TerminalBackend g_terminal;
    SpawnManager g_spawnManager;
    ArmyRuntimeManager g_armyRuntime;

    MyGUI::Window* g_window = nullptr;
    MyGUI::EditBox* g_historyBox = nullptr;
    MyGUI::EditBox* g_inputBox = nullptr;
    MyGUI::Button* g_executeButton = nullptr;
    MyGUI::TextBox* g_hintLabel = nullptr;

    std::array<bool, 256> g_virtualKeyStates = {};
    bool g_lastKnownGameWorldReady = false;
    std::unordered_map<ArmyHandleId, std::string> g_armyHandleLookup;

    TitleScreen* (*TitleScreen_orig)(TitleScreen*) = nullptr;
    void (*TitleScreen_update_orig)(TitleScreen*) = nullptr;
    void (*GameWorld_mainLoop_orig)(GameWorld*, float) = nullptr;
    RootObject* (*RootObjectFactory_createRandomCharacter_orig)(
        RootObjectFactory*,
        Faction*,
        Ogre::Vector3,
        RootObjectContainer*,
        GameData*,
        Building*,
        float) = nullptr;
    RootObjectFactory* g_lastObservedArmyFactory = nullptr;
    RootObjectFactory* g_currentArmyReplayFactory = nullptr;
    bool g_armyReplayOpportunityActive = false;
    int g_armyReplayDepth = 0;

    struct ObservedNaturalSpawnContext
    {
        RootObjectFactory* factory = nullptr;
        Faction* faction = nullptr;
        RootObjectContainer* owner = nullptr;
        Building* home = nullptr;
        float age = 1.0f;
        bool valid = false;
    };

    ObservedNaturalSpawnContext g_observedNaturalSpawnContext;

    bool IsGameWorldReady();
    bool IsArmyGameLoaded();
    bool IsArmyReplayHookInstalled();
    bool HasNaturalArmySpawnOpportunity();
    Platoon* GetCurrentArmyPlayerPlatoon();
    ArmyHandleId RegisterArmyHandleId(const hand& handleValue);
    Faction* ResolveArmyPlayerFaction();
    RootObjectContainer* ResolveArmySpawnOwnerContainer();
    Ogre::Vector3 ComputeArmyFactorySpawnPosition(const SpawnPosition& spawnOrigin, int requestIndex);
    Character* ResolveArmyCharacterFromCreatedRoot(RootObject* createdRoot);
    void WriteCrashSafeArmyTrace(const std::string& message);
    std::string GetArmyTraceFilePath();
    void ResetObservedNaturalSpawnContext();
    bool TryDismissArmyCharacterNoCrash(Character* character, unsigned int& outExceptionCode);
    bool TryInvokeArmyCreateRandomCharacterNoCrash(
        RootObjectFactory* factory,
        Faction* faction,
        const Ogre::Vector3& position,
        RootObjectContainer* owner,
        GameData* templateData,
        Building* home,
        float age,
        RootObject*& outCreatedRoot,
        unsigned int& outExceptionCode);
    bool TryResolveArmyCharacterFromCreatedRootNoCrash(
        RootObject* createdRoot,
        Character*& outCharacter,
        unsigned int& outExceptionCode);

    bool TryResolveArmyLeaderFromHandle(const hand& candidateHandle, Character*& outLeader, ArmyHandleId* outHandleId)
    {
        if (candidateHandle.isNull())
        {
            return false;
        }

        Character* resolvedLeader = candidateHandle.getCharacter();
        if (resolvedLeader == nullptr)
        {
            return false;
        }

        outLeader = resolvedLeader;
        if (outHandleId != nullptr)
        {
            *outHandleId = RegisterArmyHandleId(candidateHandle);
        }
        return true;
    }

    Character* ResolveArmyLeaderCharacter()
    {
        if (!IsGameWorldReady() || ou->player == nullptr)
        {
            return nullptr;
        }

        Character* leader = nullptr;
        ArmyHandleId ignoredHandleId = 0;
        if (TryResolveArmyLeaderFromHandle(ou->player->selectedCharacter, leader, &ignoredHandleId))
        {
            return leader;
        }

        if (TryResolveArmyLeaderFromHandle(ou->player->trackedCharacterHandle, leader, &ignoredHandleId))
        {
            return leader;
        }

        if (ou->player->playerCharacters.size() > 0)
        {
            return ou->player->playerCharacters[0];
        }

        return nullptr;
    }

    ArmyHandleId ResolveArmyLeaderHandleId()
    {
        if (!IsGameWorldReady() || ou->player == nullptr)
        {
            return 0;
        }

        Character* ignoredLeader = nullptr;
        ArmyHandleId handleId = 0;
        if (TryResolveArmyLeaderFromHandle(ou->player->selectedCharacter, ignoredLeader, &handleId))
        {
            return handleId;
        }

        if (TryResolveArmyLeaderFromHandle(ou->player->trackedCharacterHandle, ignoredLeader, &handleId))
        {
            return handleId;
        }

        return 0;
    }

    void DebugTrace(const std::string& message)
    {
        WriteCrashSafeArmyTrace(message);
        DebugLog(message.c_str());
    }

    template <typename... TArgs>
    void DebugTraceFormat(const char* format, TArgs... args)
    {
        char buffer[768] = {};
        std::snprintf(buffer, sizeof(buffer), format, std::forward<TArgs>(args)...);
        WriteCrashSafeArmyTrace(buffer);
        DebugLog(buffer);
    }

    std::string BuildArmyBuildMarker()
    {
        char buffer[256] = {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "DonJ Kenshi Hack : startPlugin | build=%s | msvc=%u",
            kArmyBuildStamp,
            static_cast<unsigned int>(_MSC_FULL_VER));
        return buffer;
    }

    std::string GetArmyTraceFilePath()
    {
        char modulePath[MAX_PATH] = {};
        const DWORD pathLength = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        if (pathLength == 0 || pathLength >= MAX_PATH)
        {
            return "DonJ_Kenshi_Hack_trace.log";
        }

        std::string fullPath(modulePath, pathLength);
        const std::string::size_type separatorIndex = fullPath.find_last_of("\\/");
        if (separatorIndex == std::string::npos)
        {
            return "DonJ_Kenshi_Hack_trace.log";
        }

        return fullPath.substr(0, separatorIndex) + "\\DonJ_Kenshi_Hack_trace.log";
    }

    void ResetObservedNaturalSpawnContext()
    {
        g_observedNaturalSpawnContext = ObservedNaturalSpawnContext();
    }

    void DismissArmyCharacter(Character* character)
    {
        if (character == nullptr || character->isDead())
        {
            return;
        }

        Damages lethalDamage(500, 500, 500, 500, 0);
        for (std::size_t anatomyIndex = 0; anatomyIndex < character->medical.anatomy.size(); ++anatomyIndex)
        {
            // La je borne explicitement l'index sur le type attendu par le conteneur Kenshi.
            const std::uint32_t safeAnatomyIndex = static_cast<std::uint32_t>(anatomyIndex);
            if (character->medical.anatomy[safeAnatomyIndex] != nullptr)
            {
                character->medical.anatomy[safeAnatomyIndex]->applyDamage(lethalDamage);
            }
        }
    }

    bool TryDismissArmyCharacterNoCrash(Character* character, unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            DismissArmyCharacter(character);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }


    bool ShouldTraceArmyTick(const ArmySession& session)
    {
        return
            g_terminal.HasPendingCommands() ||
            g_terminal.HasPendingGameplayCommands() ||
            session.state == ArmyState::Preparing ||
            session.state == ArmyState::Spawning ||
            session.state == ArmyState::Dismissing;
    }

    void TraceArmySessionPoint(const char* label, const ArmySession& session)
    {
        if (!ShouldTraceArmyTick(session))
        {
            return;
        }

        DebugTrace(std::string("[TRACE] ") + label + " | " + BuildArmySessionDebugLine(session));
    }

    void TraceArmyEnvironmentPoint(const char* label)
    {
        const bool worldReady = IsGameWorldReady();
        Character* leader = ResolveArmyLeaderCharacter();
        Platoon* platoon = worldReady && ou->player != nullptr ? ou->player->currentPlatoon : nullptr;
        Faction* faction = ResolveArmyPlayerFaction();

        DebugTraceFormat(
            "[TRACE] %s | world=%s player=%p leader=%p platoon=%p faction=%p world_factory=%p observed_factory=%p replay_hook=%s natural_spawn=%s",
            label,
            worldReady ? "ready" : "not_ready",
            worldReady ? static_cast<void*>(ou->player) : nullptr,
            static_cast<void*>(leader),
            static_cast<void*>(platoon),
            static_cast<void*>(faction),
            worldReady ? static_cast<void*>(ou->theFactory) : nullptr,
            static_cast<void*>(g_lastObservedArmyFactory),
            IsArmyReplayHookInstalled() ? "yes" : "no",
            HasNaturalArmySpawnOpportunity() ? "yes" : "no");
    }

    bool ConsumeVirtualKeyPress(int virtualKey)
    {
        const unsigned int safeIndex = static_cast<unsigned int>(virtualKey) & 0xFFu;
        const bool isDown = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
        const bool wasDown = g_virtualKeyStates[safeIndex];
        g_virtualKeyStates[safeIndex] = isDown;
        return isDown && !wasDown;
    }

    bool TryTranslateVirtualKey(int virtualKey, char* outCharacter)
    {
        if (outCharacter == nullptr)
        {
            return false;
        }

        BYTE keyboardState[256] = {};
        if (!GetKeyboardState(keyboardState))
        {
            return false;
        }

        WCHAR translatedBuffer[4] = {};
        const UINT scanCode = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC);
        const int translatedCount = ToUnicode(
            static_cast<UINT>(virtualKey),
            scanCode,
            keyboardState,
            translatedBuffer,
            4,
            0);

        if (translatedCount < 0)
        {
            BYTE emptyKeyboardState[256] = {};
            WCHAR deadKeyBuffer[4] = {};
            ToUnicode(
                static_cast<UINT>(virtualKey),
                scanCode,
                emptyKeyboardState,
                deadKeyBuffer,
                4,
                0);
            return false;
        }

        if (translatedCount <= 0)
        {
            return false;
        }

        const WCHAR translatedCharacter = translatedBuffer[0];
        if (translatedCharacter < 32 || translatedCharacter > 126)
        {
            return false;
        }

        *outCharacter = static_cast<char>(translatedCharacter);
        return true;
    }

    void ConsumePrintableKeys(bool appendCharacters)
    {
        for (int virtualKey = 0x20; virtualKey <= 0xFE; ++virtualKey)
        {
            if (!ConsumeVirtualKeyPress(virtualKey))
            {
                continue;
            }

            if (!appendCharacters)
            {
                continue;
            }

            char translatedCharacter = '\0';
            if (TryTranslateVirtualKey(virtualKey, &translatedCharacter))
            {
                g_terminal.AppendInputCharacter(translatedCharacter);
            }
        }
    }

    bool IsPointInsideWidget(MyGUI::Widget* widget, int x, int y)
    {
        if (widget == nullptr || !widget->getVisible())
        {
            return false;
        }

        const MyGUI::IntRect widgetRect = widget->getAbsoluteRect();
        return
            x >= widgetRect.left &&
            y >= widgetRect.top &&
            x < widgetRect.right &&
            y < widgetRect.bottom;
    }

    void RefreshTerminalUi()
    {
        if (g_historyBox != nullptr && g_terminal.ConsumeOutputDirty())
        {
            g_historyBox->setOnlyText(g_terminal.BuildOutputText());
            g_historyBox->setTextCursor(g_historyBox->getTextLength());
            g_historyBox->setVScrollPosition(g_historyBox->getVScrollRange());
        }

        if (g_inputBox != nullptr && g_terminal.ConsumeInputDirty())
        {
            g_inputBox->setOnlyText(g_terminal.BuildInputText());
            g_inputBox->setTextCursor(g_inputBox->getTextLength());
        }
    }

    bool IsGameWorldReady()
    {
        return ou != nullptr && ou->initialized;
    }

    ArmyHandleId RegisterArmyHandleId(const hand& handleValue)
    {
        if (handleValue.isNull())
        {
            return 0;
        }

        const std::string serialisedHandle = handleValue.toString();
        ArmyHandleId handleId = static_cast<ArmyHandleId>(std::hash<std::string>{}(serialisedHandle));
        if (handleId == 0)
        {
            handleId = 1;
        }

        while (true)
        {
            const auto existingIt = g_armyHandleLookup.find(handleId);
            if (existingIt == g_armyHandleLookup.end())
            {
                g_armyHandleLookup.emplace(handleId, serialisedHandle);
                return handleId;
            }

            if (existingIt->second == serialisedHandle)
            {
                return handleId;
            }

            ++handleId;
        }
    }

    hand ResolveArmyHandleId(ArmyHandleId handleId)
    {
        const auto it = g_armyHandleLookup.find(handleId);
        if (it == g_armyHandleLookup.end())
        {
            return hand();
        }

        hand resolvedHandle;
        resolvedHandle.fromString(it->second);
        return resolvedHandle;
    }

    Character* ResolveArmyCharacterHandleId(ArmyHandleId handleId)
    {
        if (handleId == 0)
        {
            return nullptr;
        }

        hand resolvedHandle = ResolveArmyHandleId(handleId);
        return resolvedHandle.getCharacter();
    }

    void WriteInfoToTerminalAndDebug(const std::string& message)
    {
        g_terminal.AppendOutputLine(message);
        WriteCrashSafeArmyTrace(message);
        DebugLog(message.c_str());
    }

    void WriteErrorToTerminalAndDebug(const std::string& message)
    {
        g_terminal.AppendOutputLine(message);
        WriteCrashSafeArmyTrace(message);
        ErrorLog(message.c_str());
    }

    void WriteCrashSafeArmyTrace(const std::string& message)
    {
        if (message.empty())
        {
            return;
        }

        const std::string tracePath = GetArmyTraceFilePath();
        HANDLE fileHandle = CreateFileA(
            tracePath.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        char lineBuffer[1024] = {};
        const unsigned long long tickValue = static_cast<unsigned long long>(GetTickCount64());
        const int lineLength = std::snprintf(
            lineBuffer,
            sizeof(lineBuffer),
            "%010llu | %s\r\n",
            tickValue,
            message.c_str());

        if (lineLength > 0)
        {
            DWORD written = 0;
            WriteFile(
                fileHandle,
                lineBuffer,
                static_cast<DWORD>(lineLength),
                &written,
                nullptr);
        }

        CloseHandle(fileHandle);
    }

    bool TryInvokeArmyCreateRandomCharacterNoCrash(
        RootObjectFactory* factory,
        Faction* faction,
        const Ogre::Vector3& position,
        RootObjectContainer* owner,
        GameData* templateData,
        Building* home,
        float age,
        RootObject*& outCreatedRoot,
        unsigned int& outExceptionCode)
    {
        outCreatedRoot = nullptr;
        outExceptionCode = 0;

        __try
        {
            outCreatedRoot = RootObjectFactory_createRandomCharacter_orig(
                factory,
                faction,
                position,
                owner,
                templateData,
                home,
                age);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryResolveArmyCharacterFromCreatedRootNoCrash(
        RootObject* createdRoot,
        Character*& outCharacter,
        unsigned int& outExceptionCode)
    {
        outCharacter = nullptr;
        outExceptionCode = 0;

        __try
        {
            outCharacter = ResolveArmyCharacterFromCreatedRoot(createdRoot);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    GameData* ResolveArmyTemplateByName(const std::string& templateName)
    {
        if (!IsGameWorldReady())
        {
            return nullptr;
        }

        GameData* templateData = ou->gamedata.getDataByName(templateName, CHARACTER);
        if (templateData == nullptr)
        {
            templateData = ou->gamedata.getDataByName(templateName, HUMAN_CHARACTER);
        }
        if (templateData == nullptr)
        {
            templateData = ou->gamedata.getDataByName(templateName, ANIMAL_CHARACTER);
        }

        return templateData;
    }

    Faction* ResolveArmyPlayerFaction()
    {
        if (!IsArmyGameLoaded())
        {
            return nullptr;
        }

        if (ou->player->participant != nullptr)
        {
            return ou->player->participant;
        }

        Character* leader = ResolveArmyLeaderCharacter();
        return leader != nullptr ? leader->owner : nullptr;
    }

    RootObjectContainer* ResolveArmySpawnOwnerContainer()
    {
        if (!IsArmyGameLoaded())
        {
            return nullptr;
        }

        RootObjectContainer* ownerContainer = ou->player->getCurrentActivePlatoon();
        if (ownerContainer != nullptr)
        {
            return ownerContainer;
        }

        Platoon* currentPlatoon = GetCurrentArmyPlayerPlatoon();
        if (currentPlatoon != nullptr && currentPlatoon->activePlatoon != nullptr)
        {
            return currentPlatoon->activePlatoon;
        }

        return nullptr;
    }

    bool IsArmyGameLoaded()
    {
        return IsGameWorldReady() && ou->player != nullptr;
    }

    Platoon* GetCurrentArmyPlayerPlatoon()
    {
        if (!IsArmyGameLoaded())
        {
            return nullptr;
        }

        return ou->player->currentPlatoon;
    }

    bool HasResolvableArmyLeader()
    {
        return ResolveArmyLeaderCharacter() != nullptr;
    }

    bool AreArmyTemplatesConfigured()
    {
        const ArmyCommandSpec& spec = GetArmyCommandSpec();
        for (const char* templateName : spec.templateNames)
        {
            if (templateName == nullptr || templateName[0] == '\0')
            {
                return false;
            }

            if (ResolveArmyTemplateByName(templateName) == nullptr)
            {
                return false;
            }
        }

        return true;
    }

    bool IsArmySpawnSystemInitialized()
    {
        // La je verrouille le fait que le spawn et le post-traitement escorte doivent etre poses ensemble pour accepter /army.
        return g_spawnManager.IsConfigured() && g_armyRuntime.IsConfigured();
    }

    bool IsArmyFactoryAvailable()
    {
        if (!IsArmyGameLoaded())
        {
            return false;
        }

        return RootObjectFactory_createRandomCharacter_orig != nullptr;
    }

    bool IsArmyReplayHookInstalled()
    {
        // La j'utilise le flag historique "replay hook" pour signaler que le hook factory
        // natif est bien installe et peut capturer une vraie instance RootObjectFactory.
        return RootObjectFactory_createRandomCharacter_orig != nullptr;
    }

    bool HasNaturalArmySpawnOpportunity()
    {
        // La je n'autorise le spawn que pendant une vraie creation native observee.
        // Ca evite l'appel direct a la factory depuis le game tick, qui a fait planter le jeu.
        return
            g_armyReplayOpportunityActive &&
            g_observedNaturalSpawnContext.valid &&
            g_observedNaturalSpawnContext.factory != nullptr;
    }

    bool TryResolveArmySpawnOrigin(SpawnPosition& outPosition)
    {
        Character* leader = ResolveArmyLeaderCharacter();
        if (leader == nullptr)
        {
            return false;
        }

        const Ogre::Vector3 leaderPosition = leader->getPosition();
        outPosition.x = leaderPosition.x;
        outPosition.y = leaderPosition.y;
        outPosition.z = leaderPosition.z;
        return true;
    }

    SpawnPosition GetArmyCharacterPosition(Character* character)
    {
        if (character == nullptr)
        {
            return SpawnPosition();
        }

        const Ogre::Vector3 position = character->getPosition();
        SpawnPosition spawnPosition;
        spawnPosition.x = position.x;
        spawnPosition.y = position.y;
        spawnPosition.z = position.z;
        return spawnPosition;
    }

    void ApplyArmyFaction(Character* character, Faction* faction, ActivePlatoon* platoon)
    {
        if (character == nullptr || faction == nullptr)
        {
            return;
        }

        character->setFaction(faction, platoon);
    }

    void RenameArmyCharacter(Character* character, const std::string& name)
    {
        if (character == nullptr || name.empty())
        {
            return;
        }

        character->setName(name);
    }

    void TeleportArmyCharacter(Character* character, const SpawnPosition& position)
    {
        if (character == nullptr)
        {
            return;
        }

        Ogre::Vector3 targetPosition = character->getPosition();
        targetPosition.x = position.x;
        targetPosition.y = position.y;
        targetPosition.z = position.z;
        character->teleport(targetPosition);
    }

    void SetArmyFollowTarget(Character* character, Character* leader)
    {
        if (character == nullptr || leader == nullptr)
        {
            return;
        }

        CharMovement* movement = character->getMovement();
        if (movement != nullptr)
        {
            movement->setDestination(leader, HIGH_PRIORITY);
        }
    }

    void ApplyArmyEscortOrder(Character* character, ArmyEscortOrder order)
    {
        if (character == nullptr)
        {
            return;
        }

        switch (order)
        {
        case ArmyEscortOrder::DefensiveCombat:
            character->setStandingOrder(MessageForB::M_SET_ORDER_DEFENSIVE_COMBAT, true);
            break;
        case ArmyEscortOrder::ChaseTarget:
            character->setStandingOrder(MessageForB::M_SET_ORDER_CHASE, true);
            break;
        default:
            break;
        }
    }

    void SetArmyEscortRole(Character* character)
    {
        if (character == nullptr)
        {
            return;
        }

        character->setSquadMemberType(SQUAD_1);
    }

    void RethinkArmyAi(Character* character)
    {
        if (character == nullptr)
        {
            return;
        }

        character->reThinkCurrentAIAction();
    }

    Ogre::Vector3 ComputeArmyFactorySpawnPosition(const SpawnPosition& spawnOrigin, int requestIndex)
    {
        constexpr float kPi = 3.14159265358979323846f;

        const int safeIndex = requestIndex < 0 ? 0 : requestIndex;
        const int slotIndex = safeIndex % 8;
        const int ringIndex = safeIndex / 8;
        const float radius = 1.5f + (static_cast<float>(ringIndex) * 1.15f);
        const float angle = (2.0f * kPi * static_cast<float>(slotIndex) / 8.0f) + (static_cast<float>(ringIndex) * 0.33f);

        Character* leader = ResolveArmyLeaderCharacter();
        if (leader == nullptr)
        {
            return ou->getCameraCenter();
        }

        Ogre::Vector3 center = leader->getPosition();
        center.x = spawnOrigin.x;
        center.y = spawnOrigin.y;
        center.z = spawnOrigin.z;

        Ogre::Vector3 candidate = center;
        candidate.x += std::cos(angle) * radius;
        candidate.z += std::sin(angle) * radius;

        if (IsGameWorldReady())
        {
            ou->findValidSpawnPos(candidate, center);
        }

        return candidate;
    }

    Character* ResolveArmyCharacterFromCreatedRoot(RootObject* createdRoot)
    {
        if (createdRoot == nullptr)
        {
            return nullptr;
        }

        const hand& createdHandle = createdRoot->getHandle();
        if (!createdHandle.isNull())
        {
            Character* createdCharacter = createdHandle.getCharacter();
            if (createdCharacter != nullptr)
            {
                return createdCharacter;
            }
        }

        const itemType createdType = createdRoot->getDataType();
        if (createdType == CHARACTER || createdType == HUMAN_CHARACTER || createdType == ANIMAL_CHARACTER)
        {
            return static_cast<Character*>(createdRoot);
        }

        return nullptr;
    }

    SpawnAttemptResult TrySpawnArmyUnitThroughFactory(
        const SpawnRequest& request,
        void* resolvedTemplate,
        Faction* playerFaction,
        const SpawnPosition& spawnOrigin)
    {
        SpawnAttemptResult result;

        if (!IsArmyReplayHookInstalled())
        {
            result.outcome = SpawnAttemptOutcome::DeferredAwaitingReplayHook;
            result.shouldRequeue = true;
            result.detail = "[INFO] Spawn differe : hook factory Kenshi indisponible.";
            return result;
        }

        if (!HasNaturalArmySpawnOpportunity())
        {
            result.outcome = SpawnAttemptOutcome::DeferredAwaitingReplayOpportunity;
            result.shouldRequeue = true;
            result.detail = "[INFO] Spawn differe : en attente d'une creation native pour rejouer la factory Kenshi.";
            return result;
        }

        GameData* templateData = static_cast<GameData*>(resolvedTemplate);
        if (templateData == nullptr)
        {
            result.outcome = SpawnAttemptOutcome::FailedTemplateMissing;
            result.shouldRequeue = false;
            result.detail = std::string("[ERREUR] Template introuvable pour ") + request.templateName + ".";
            return result;
        }

        (void)playerFaction;

        const ObservedNaturalSpawnContext replayContext = g_observedNaturalSpawnContext;
        if (!replayContext.valid || replayContext.factory == nullptr)
        {
            result.outcome = SpawnAttemptOutcome::DeferredFactoryUnavailable;
            result.shouldRequeue = true;
            result.detail = "[INFO] Spawn differe : contexte factory naturel incomplet.";
            return result;
        }

        const Ogre::Vector3 factorySpawnPosition = ComputeArmyFactorySpawnPosition(spawnOrigin, request.index);
        DebugTraceFormat(
            "[TRACE] TrySpawnArmyUnitThroughFactory tentative | req=%s | template=%p | faction_nat=%p | owner_nat=%p | factory=%p | pos=(%.2f, %.2f, %.2f)",
            request.templateName.c_str(),
            static_cast<void*>(templateData),
            static_cast<void*>(replayContext.faction),
            static_cast<void*>(replayContext.owner),
            static_cast<void*>(replayContext.factory),
            static_cast<double>(factorySpawnPosition.x),
            static_cast<double>(factorySpawnPosition.y),
            static_cast<double>(factorySpawnPosition.z));

        ++g_armyReplayDepth;
        RootObject* createdRoot = nullptr;
        unsigned int factoryExceptionCode = 0;
        const bool factoryCallSucceeded = TryInvokeArmyCreateRandomCharacterNoCrash(
            replayContext.factory,
            replayContext.faction,
            factorySpawnPosition,
            replayContext.owner,
            templateData,
            replayContext.home,
            replayContext.age > 0.0f ? replayContext.age : 1.0f,
            createdRoot,
            factoryExceptionCode);
        --g_armyReplayDepth;

        if (!factoryCallSucceeded)
        {
            char buffer[320] = {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "[ERREUR] Factory Kenshi : exception structuree 0x%08X pendant createRandomCharacter.",
                factoryExceptionCode);
            result.outcome = SpawnAttemptOutcome::FailedFactoryCallFatal;
            result.shouldRequeue = false;
            result.detail = buffer;
            return result;
        }

        if (createdRoot == nullptr)
        {
            result.outcome = SpawnAttemptOutcome::FailedFactoryCall;
            result.shouldRequeue = true;
            result.detail = "[ERREUR] Factory Kenshi : createRandomCharacter a retourne null.";
            return result;
        }

        Character* createdCharacter = nullptr;
        unsigned int resolveExceptionCode = 0;
        const bool resolveSucceeded = TryResolveArmyCharacterFromCreatedRootNoCrash(
            createdRoot,
            createdCharacter,
            resolveExceptionCode);
        if (!resolveSucceeded)
        {
            char buffer[320] = {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "[ERREUR] Factory Kenshi : exception structuree 0x%08X pendant la resolution du Character cree.",
                resolveExceptionCode);
            result.outcome = SpawnAttemptOutcome::FailedFactoryCallFatal;
            result.shouldRequeue = false;
            result.detail = buffer;
            return result;
        }

        if (createdCharacter == nullptr)
        {
            result.outcome = SpawnAttemptOutcome::FailedFactoryCallFatal;
            result.shouldRequeue = false;
            result.detail = "[ERREUR] Factory Kenshi : l'objet cree n'a pas pu etre resolu en Character.";
            return result;
        }

        RegisterArmyHandleId(createdCharacter->getHandle());
        DebugTraceFormat(
            "[TRACE] TrySpawnArmyUnitThroughFactory succes | root=%p | character=%p | handle=%s",
            static_cast<void*>(createdRoot),
            static_cast<void*>(createdCharacter),
            createdCharacter->getHandle().toString().c_str());

        result.outcome = SpawnAttemptOutcome::Spawned;
        result.character = createdCharacter;
        result.shouldRequeue = false;
        return result;
    }

    RootObject* RootObjectFactory_createRandomCharacter_hook(
        RootObjectFactory* thisptr,
        Faction* faction,
        Ogre::Vector3 position,
        RootObjectContainer* owner,
        GameData* characterTemplate,
        Building* home,
        float age)
    {
        if (g_armyReplayDepth == 0)
        {
            g_lastObservedArmyFactory = thisptr;
            g_observedNaturalSpawnContext.factory = thisptr;
            g_observedNaturalSpawnContext.faction = faction;
            g_observedNaturalSpawnContext.owner = owner;
            g_observedNaturalSpawnContext.home = home;
            g_observedNaturalSpawnContext.age = age;
            g_observedNaturalSpawnContext.valid = (thisptr != nullptr);

            DebugTraceFormat(
                "[TRACE] RootObjectFactory::createRandomCharacter observe | factory=%p | faction=%p | owner=%p | template=%p | pos=(%.2f, %.2f, %.2f) | age=%.2f",
                static_cast<void*>(thisptr),
                static_cast<void*>(faction),
                static_cast<void*>(owner),
                static_cast<void*>(characterTemplate),
                static_cast<double>(position.x),
                static_cast<double>(position.y),
                static_cast<double>(position.z),
                static_cast<double>(age));
        }

        RootObject* createdRoot = RootObjectFactory_createRandomCharacter_orig(
            thisptr,
            faction,
            position,
            owner,
            characterTemplate,
            home,
            age);

        const bool canReplayArmySpawn =
            thisptr != nullptr &&
            g_armyReplayDepth == 0 &&
            g_terminal.GetArmySession().state == ArmyState::Spawning;

        if (canReplayArmySpawn)
        {
            g_currentArmyReplayFactory = g_observedNaturalSpawnContext.factory;
            g_armyReplayOpportunityActive = true;

            ArmySession& session = g_terminal.GetArmySession();
            TraceArmySessionPoint("hook factory avant replay", session);
            const std::size_t spawnedByReplay = g_spawnManager.Tick(session, 0.0f);
            if (spawnedByReplay > 0)
            {
                DebugTraceFormat(
                    "[TRACE] RootObjectFactory::createRandomCharacter replay succes | spawned=%zu",
                    spawnedByReplay);
            }
            TraceArmySessionPoint("hook factory apres replay", session);

            g_armyReplayOpportunityActive = false;
            g_currentArmyReplayFactory = nullptr;
        }

        return createdRoot;
    }

    void TickArmySpawnManager(float deltaSeconds)
    {
        ArmySession& session = g_terminal.GetArmySession();
        if (ShouldTraceArmyTick(session))
        {
            TraceArmyEnvironmentPoint("TickArmySpawnManager entree");
            TraceArmySessionPoint("avant SpawnManager.Tick", session);
        }

        const std::size_t spawnedThisTick = g_spawnManager.Tick(session, deltaSeconds);
        if (ShouldTraceArmyTick(session))
        {
            DebugTraceFormat(
                "[TRACE] TickArmySpawnManager apres SpawnManager.Tick | spawnedThisTick=%zu",
                spawnedThisTick);
            TraceArmySessionPoint("apres SpawnManager.Tick", session);
        }

        g_armyRuntime.Tick(session, deltaSeconds);
        if (ShouldTraceArmyTick(session))
        {
            TraceArmySessionPoint("apres ArmyRuntimeManager.Tick", session);
        }

        if (session.state == ArmyState::Idle)
        {
            g_spawnManager.Reset();
        }

        if (spawnedThisTick > 0)
        {
            char logMessage[160] = {};
            std::snprintf(
                logMessage,
                sizeof(logMessage),
                "DonJ Kenshi Hack : %zu invocation(s) traitee(s) par le SpawnManager.",
                spawnedThisTick);
            DebugLog(logMessage);
        }
    }

    void ResetKeyboardEdgeTracking(const char* reason)
    {
        g_virtualKeyStates.fill(false);

        char logMessage[160] = {};
        std::snprintf(
            logMessage,
            sizeof(logMessage),
            "DonJ Kenshi Hack : etat clavier reinitialise (%s).",
            reason);
        DebugLog(logMessage);
    }

    void SyncExecutionContextState()
    {
        const bool gameWorldReady = IsGameWorldReady();
        if (gameWorldReady == g_lastKnownGameWorldReady)
        {
            return;
        }

        g_lastKnownGameWorldReady = gameWorldReady;
        if (gameWorldReady)
        {
            g_spawnManager.Reset();
            g_lastObservedArmyFactory = nullptr;
            g_currentArmyReplayFactory = nullptr;
            g_armyReplayOpportunityActive = false;
            g_armyReplayDepth = 0;
            ResetObservedNaturalSpawnContext();
            ResetKeyboardEdgeTracking("entree en jeu");
        }
        else
        {
            ArmySession& session = g_terminal.GetArmySession();
            if (session.state != ArmyState::Idle)
            {
                WriteInfoToTerminalAndDebug("[INFO] Invocation dissoute : retour au menu ou changement de monde.");
                ResetArmySession(session);
                g_terminal.MarkUiDirty();
            }

            g_spawnManager.Reset();
            ResetKeyboardEdgeTracking("retour menu");
            g_armyHandleLookup.clear();
            g_lastObservedArmyFactory = nullptr;
            g_currentArmyReplayFactory = nullptr;
            g_armyReplayOpportunityActive = false;
            g_armyReplayDepth = 0;
            ResetObservedNaturalSpawnContext();
        }
    }

    void ProcessDeferredTerminalCommands(const char* sourceName)
    {
        if (g_terminal.GetPendingCommandCount() > 0)
        {
            DebugTraceFormat(
                "[TRACE] ProcessDeferredTerminalCommands | source=%s | pending_ui=%zu | pending_gameplay=%zu",
                sourceName,
                g_terminal.GetPendingCommandCount(),
                g_terminal.GetPendingGameplayCommandCount());
        }

        const size_t processedCount = g_terminal.ProcessPendingCommands();
        if (processedCount == 0)
        {
            return;
        }

        char logMessage[160] = {};
        std::snprintf(
            logMessage,
            sizeof(logMessage),
            "DonJ Kenshi Hack : %zu commande(s) executee(s) sur %s.",
            processedCount,
            sourceName);
        DebugLog(logMessage);
        TraceArmySessionPoint("apres ProcessPendingCommands", g_terminal.GetArmySession());
    }

    void CreateTerminalUi()
    {
        if (g_window != nullptr)
        {
            return;
        }

        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (gui == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : MyGUI::Gui introuvable.");
            return;
        }

        g_window = gui->createWidgetReal<MyGUI::Window>(
            "Kenshi_WindowCX",
            0.08f, 0.08f, 0.45f, 0.43f,
            MyGUI::Align::Default,
            "Window",
            "DonJHackWindow");

        if (g_window == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer la fenetre du terminal.");
            return;
        }

        g_window->setCaption("DonJ Kenshi Hack");

        MyGUI::Widget* clientWidget = g_window->getClientWidget();
        if (clientWidget == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : client widget introuvable.");
            return;
        }

        MyGUI::Widget* historyPanel = clientWidget->createWidgetReal<MyGUI::Widget>(
            "Kenshi_GenericTextBoxSkin",
            0.04f, 0.05f, 0.92f, 0.58f,
            MyGUI::Align::Default,
            "DonJHackHistoryPanel");

        g_historyBox = historyPanel->createWidgetReal<MyGUI::EditBox>(
            "Kenshi_EditBoxStrechEmpty",
            0.025f, 0.07f, 0.95f, 0.88f,
            MyGUI::Align::Stretch,
            "DonJHackHistoryBox");

        if (g_historyBox == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer l'historique du terminal.");
            return;
        }

        g_historyBox->setEditReadOnly(true);
        g_historyBox->setEditStatic(true);
        g_historyBox->setEditMultiLine(true);
        g_historyBox->setEditWordWrap(true);
        g_historyBox->setVisibleVScroll(true);
        g_historyBox->setNeedMouseFocus(false);
        g_historyBox->setNeedKeyFocus(false);
        g_historyBox->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);

        MyGUI::Widget* inputPanel = clientWidget->createWidgetReal<MyGUI::Widget>(
            "Kenshi_GenericTextBoxSkin",
            0.04f, 0.68f, 0.67f, 0.15f,
            MyGUI::Align::Default,
            "DonJHackInputPanel");

        g_inputBox = inputPanel->createWidgetReal<MyGUI::EditBox>(
            "Kenshi_EditBoxStrechEmpty",
            0.025f, 0.18f, 0.95f, 0.64f,
            MyGUI::Align::Stretch,
            "DonJHackInputBox");

        if (g_inputBox == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer la zone de saisie.");
            return;
        }

        g_inputBox->setEditReadOnly(true);
        g_inputBox->setEditStatic(true);
        g_inputBox->setNeedMouseFocus(false);
        g_inputBox->setNeedKeyFocus(false);
        g_inputBox->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

        g_executeButton = clientWidget->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1",
            0.75f, 0.68f, 0.21f, 0.15f,
            MyGUI::Align::Default,
            "DonJHackExecuteButton");

        if (g_executeButton == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer le bouton Executer.");
            return;
        }

        g_executeButton->setCaption("Executer");

        g_hintLabel = clientWidget->createWidgetReal<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            0.04f, 0.87f, 0.92f, 0.08f,
            MyGUI::Align::Default,
            "DonJHackHintLabel");

        if (g_hintLabel != nullptr)
        {
            g_hintLabel->setCaption("Entree : ouvrir ou valider | Echap : annuler | Fleches : historique");
            g_hintLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        }

        g_terminal.MarkUiDirty();
        RefreshTerminalUi();
        DebugLog("DonJ Kenshi Hack : fenetre terminal creee.");
    }

    void ProcessTerminalMouseInput()
    {
        if (key == nullptr || !key->mLUp)
        {
            return;
        }

        const int cursorX = static_cast<int>(key->mPosAbs.x);
        const int cursorY = static_cast<int>(key->mPosAbs.y);

        if (IsPointInsideWidget(g_inputBox, cursorX, cursorY))
        {
            g_terminal.ActivateInput();
            DebugLog("DonJ Kenshi Hack : saisie activee par clic.");
            return;
        }

        if (IsPointInsideWidget(g_executeButton, cursorX, cursorY))
        {
            g_terminal.ActivateInput();
            if (g_terminal.SubmitCurrentInput())
            {
                DebugLog("DonJ Kenshi Hack : commande soumise par le bouton Executer.");
            }
            else
            {
                DebugLog("DonJ Kenshi Hack : bouton Executer presse sans commande.");
            }
        }
    }

    void ProcessTerminalKeyboardInput()
    {
        const bool enterPressed = ConsumeVirtualKeyPress(VK_RETURN);
        const bool escapePressed = ConsumeVirtualKeyPress(VK_ESCAPE);
        const bool backspacePressed = ConsumeVirtualKeyPress(VK_BACK);
        const bool upPressed = ConsumeVirtualKeyPress(VK_UP);
        const bool downPressed = ConsumeVirtualKeyPress(VK_DOWN);

        if (escapePressed && g_terminal.IsInputActive())
        {
            g_terminal.CancelInput();
            DebugLog("DonJ Kenshi Hack : saisie annulee.");
            ConsumePrintableKeys(false);
            return;
        }

        if (enterPressed)
        {
            DebugTraceFormat(
                "[TRACE] input_enter_pressed | input_active=%s | pending_ui=%zu | pending_gameplay=%zu",
                g_terminal.IsInputActive() ? "yes" : "no",
                g_terminal.GetPendingCommandCount(),
                g_terminal.GetPendingGameplayCommandCount());

            if (!g_terminal.IsInputActive())
            {
                g_terminal.ActivateInput();
                DebugTrace("DonJ Kenshi Hack : saisie activee.");
            }
            else if (g_terminal.SubmitCurrentInput())
            {
                DebugTrace("DonJ Kenshi Hack : commande soumise.");
            }
            else
            {
                DebugTrace("DonJ Kenshi Hack : ligne vide ignoree.");
            }

            ConsumePrintableKeys(false);
            return;
        }

        if (!g_terminal.IsInputActive())
        {
            ConsumePrintableKeys(false);
            return;
        }

        if (backspacePressed)
        {
            g_terminal.BackspaceInput();
        }

        if (upPressed)
        {
            g_terminal.NavigateCommandHistory(-1);
        }

        if (downPressed)
        {
            g_terminal.NavigateCommandHistory(1);
        }

        ConsumePrintableKeys(true);
    }

    TitleScreen* TitleScreen_hook(TitleScreen* thisptr)
    {
        TitleScreen* titleScreen = TitleScreen_orig(thisptr);
        CreateTerminalUi();
        return titleScreen;
    }

    void TitleScreen_update_hook(TitleScreen* thisptr)
    {
        TitleScreen_update_orig(thisptr);
        SyncExecutionContextState();

        if (g_window == nullptr)
        {
            return;
        }

        if (!IsGameWorldReady())
        {
            ProcessTerminalMouseInput();
            ProcessTerminalKeyboardInput();
            ProcessDeferredTerminalCommands("le tick titre");
            RefreshTerminalUi();
        }
    }

    void GameWorld_mainLoop_hook(GameWorld* thisptr, float time)
    {
        GameWorld_mainLoop_orig(thisptr, time);
        SyncExecutionContextState();

        const ArmySession& sessionBeforeInput = g_terminal.GetArmySession();
        if (ShouldTraceArmyTick(sessionBeforeInput))
        {
            TraceArmyEnvironmentPoint("GameWorld_mainLoop_hook debut");
            TraceArmySessionPoint("game tick debut", sessionBeforeInput);
        }

        if (g_window != nullptr)
        {
            ProcessTerminalMouseInput();
            ProcessTerminalKeyboardInput();
        }

        ProcessDeferredTerminalCommands("le game tick");
        TraceArmySessionPoint("avant TickGameplay", g_terminal.GetArmySession());
        g_terminal.TickGameplay(time);
        TraceArmySessionPoint("apres TickGameplay", g_terminal.GetArmySession());
        TickArmySpawnManager(time);
        TraceArmySessionPoint("game tick fin", g_terminal.GetArmySession());
        RefreshTerminalUi();
    }
}

__declspec(dllexport) void startPlugin()
{
    DeleteFileA(GetArmyTraceFilePath().c_str());
    const std::string buildMarker = BuildArmyBuildMarker();
    WriteCrashSafeArmyTrace(buildMarker);
    DebugLog(buildMarker.c_str());

    g_terminal.SetArmyCommandEnvironment({
        []() { return IsArmyGameLoaded(); },
        []() { return HasResolvableArmyLeader(); },
        []() { return AreArmyTemplatesConfigured(); },
        []() { return IsArmySpawnSystemInitialized(); },
        [](const std::string& message)
        {
            DebugTrace(message);
        }
    });

    g_spawnManager.SetEnvironment({
        []() { return IsArmyGameLoaded(); },
        []() { return IsArmyFactoryAvailable(); },
        []() { return IsArmyReplayHookInstalled(); },
        []() { return HasNaturalArmySpawnOpportunity(); },
        [](const std::string& templateName) -> void*
        {
            return ResolveArmyTemplateByName(templateName);
        },
        []() -> Faction*
        {
            return ResolveArmyPlayerFaction();
        },
        [](SpawnPosition& outPosition) -> bool
        {
            return TryResolveArmySpawnOrigin(outPosition);
        },
        [](const SpawnRequest& request, void* resolvedTemplate, Faction* playerFaction, const SpawnPosition& spawnOrigin) -> SpawnAttemptResult
        {
            return TrySpawnArmyUnitThroughFactory(request, resolvedTemplate, playerFaction, spawnOrigin);
        },
        [](ArmySession& session, const SpawnRequest& request, Character* character) -> bool
        {
            return g_armyRuntime.ConfigureSpawnedUnit(session, request, character);
        },
        [](const std::string& message)
        {
            WriteInfoToTerminalAndDebug(message);
        },
        [](const std::string& message)
        {
            WriteErrorToTerminalAndDebug(message);
        },
        [](const std::string& message)
        {
            DebugTrace(message);
        }
    });

    g_armyRuntime.SetEnvironment({
        []() { return IsArmyGameLoaded(); },
        []() -> Character*
        {
            return ResolveArmyLeaderCharacter();
        },
        []() -> Platoon*
        {
            return GetCurrentArmyPlayerPlatoon();
        },
        [](Platoon* platoon) -> ActivePlatoon*
        {
            return platoon != nullptr ? platoon->getActivePlatoon() : nullptr;
        },
        []() -> Faction*
        {
            return ResolveArmyPlayerFaction();
        },
        [](Character* leader) -> Faction*
        {
            return leader != nullptr ? leader->owner : nullptr;
        },
        [](Character* character) -> ArmyHandleId
        {
            return character != nullptr ? RegisterArmyHandleId(character->getHandle()) : 0;
        },
        [](Platoon* platoon) -> ArmyHandleId
        {
            // La je garde une identite de platoon purement locale a la session.
            // Je n'ai pas besoin d'un hand serialisable ici, juste d'un identifiant
            // stable tant que le platoon reste vivant, ce qui evite une conversion
            // runtime dangereuse via hand(platoon) sur cette build.
            return MakeRuntimePointerIdentity(platoon);
        },
        [](ArmyHandleId handleId) -> Character*
        {
            return ResolveArmyCharacterHandleId(handleId);
        },
        [](Character* character) -> bool
        {
            return character == nullptr || character->isDead();
        },
        [](Character* character) -> bool
        {
            return character != nullptr && character->isUnconcious();
        },
        [](Character* character) -> SpawnPosition
        {
            return GetArmyCharacterPosition(character);
        },
        [](Character* character, Faction* faction, ActivePlatoon* platoon)
        {
            ApplyArmyFaction(character, faction, platoon);
        },
        [](Character* character, const std::string& name)
        {
            RenameArmyCharacter(character, name);
        },
        [](Character* character, const SpawnPosition& position)
        {
            TeleportArmyCharacter(character, position);
        },
        [](Character* character, Character* leader)
        {
            SetArmyFollowTarget(character, leader);
        },
        [](Character* character, ArmyEscortOrder order)
        {
            ApplyArmyEscortOrder(character, order);
        },
        [](Character* character)
        {
            SetArmyEscortRole(character);
        },
        [](Character* character)
        {
            RethinkArmyAi(character);
        },
        [](const std::string& message)
        {
            WriteInfoToTerminalAndDebug(message);
        },
        [](const std::string& message)
        {
            WriteErrorToTerminalAndDebug(message);
        },
        [](const std::string& message)
        {
            DebugTrace(message);
        },
        []() -> ArmyHandleId
        {
            return ResolveArmyLeaderHandleId();
        },
        [](Character* character)
        {
            unsigned int dismissExceptionCode = 0;
            if (!TryDismissArmyCharacterNoCrash(character, dismissExceptionCode))
            {
                DebugTraceFormat(
                    "[TRACE] DismissArmyCharacter exception structuree | code=0x%08X | character=%p",
                    dismissExceptionCode,
                    static_cast<void*>(character));
            }
        }
    });

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&TitleScreen::_CONSTRUCTOR),
            TitleScreen_hook,
            &TitleScreen_orig))
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook TitleScreen::_CONSTRUCTOR.");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&TitleScreen::_NV_update),
            TitleScreen_update_hook,
            &TitleScreen_update_orig))
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook TitleScreen::_NV_update.");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),
            GameWorld_mainLoop_hook,
            &GameWorld_mainLoop_orig))
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook GameWorld::_NV_mainLoop_GPUSensitiveStuff.");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&RootObjectFactory::createRandomCharacter),
            RootObjectFactory_createRandomCharacter_hook,
            &RootObjectFactory_createRandomCharacter_orig))
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook RootObjectFactory::createRandomCharacter.");
    }
    else
    {
        DebugLog("DonJ Kenshi Hack : hook RootObjectFactory::createRandomCharacter installe.");
    }

    DebugLog("DonJ Kenshi Hack : hooks constructeur, update et game tick installes.");
}
