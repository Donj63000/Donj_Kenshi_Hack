#include "ArmyCommandSpec.h"
#include "ArmyRuntimeManager.h"
#include "RuntimeIdentity.h"
#include "SpawnManager.h"
#include "TerminalBackend.h"

#include <Debug.h>

#include <array>
#include <cstdio>
#include <string>
#include <unordered_map>

#define NOMINMAX
#define BOOST_ALL_NO_LIB
#define BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_USE_WINDOWS_H
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <kenshi/Character.h>
#include <kenshi/CharMovement.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Platoon.h>
#include <kenshi/PlayerInterface.h>
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
        DebugLog(message.c_str());
    }

    void WriteErrorToTerminalAndDebug(const std::string& message)
    {
        g_terminal.AppendOutputLine(message);
        ErrorLog(message.c_str());
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

        return ou->player->getCurrentPlatoon();
    }

    bool HasResolvableArmyLeader()
    {
        return IsArmyGameLoaded() && ou->player->getAnyPlayerCharacter() != nullptr;
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
        return IsGameWorldReady() && ou->theFactory != nullptr && ou->player != nullptr && ou->player->getFaction() != nullptr;
    }

    bool IsArmyReplayHookInstalled()
    {
        // La le replay sur creation native reste volontairement non branche tant que le hook factory exact n'est pas valide sur cette build.
        return false;
    }

    bool HasNaturalArmySpawnOpportunity()
    {
        // La je reserve ce callback pour le vrai replay sur creation native.
        return false;
    }

    bool TryResolveArmySpawnOrigin(SpawnPosition& outPosition)
    {
        if (!HasResolvableArmyLeader())
        {
            return false;
        }

        Character* leader = ou->player->getAnyPlayerCharacter();
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

    SpawnAttemptResult TrySpawnArmyUnitThroughFactory(
        const SpawnRequest& request,
        void* resolvedTemplate,
        Faction* playerFaction,
        const SpawnPosition& spawnOrigin)
    {
        (void)request;
        (void)resolvedTemplate;
        (void)playerFaction;
        (void)spawnOrigin;

        SpawnAttemptResult result;
        result.outcome = SpawnAttemptOutcome::DeferredAwaitingReplayHook;
        result.shouldRequeue = true;
        result.detail = "[INFO] Spawn differe : le replay factory Kenshi n'est pas encore branche a la creation native.";
        return result;
    }

    void TickArmySpawnManager(float deltaSeconds)
    {
        ArmySession& session = g_terminal.GetArmySession();
        const std::size_t spawnedThisTick = g_spawnManager.Tick(session, deltaSeconds);
        g_armyRuntime.Tick(session, deltaSeconds);
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

            ResetKeyboardEdgeTracking("retour menu");
            g_armyHandleLookup.clear();
        }
    }

    void ProcessDeferredTerminalCommands(const char* sourceName)
    {
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
            if (!g_terminal.IsInputActive())
            {
                g_terminal.ActivateInput();
                DebugLog("DonJ Kenshi Hack : saisie activee.");
            }
            else if (g_terminal.SubmitCurrentInput())
            {
                DebugLog("DonJ Kenshi Hack : commande soumise.");
            }
            else
            {
                DebugLog("DonJ Kenshi Hack : ligne vide ignoree.");
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

        if (g_window != nullptr)
        {
            ProcessTerminalMouseInput();
            ProcessTerminalKeyboardInput();
        }

        ProcessDeferredTerminalCommands("le game tick");
        g_terminal.TickGameplay(time);
        TickArmySpawnManager(time);
        RefreshTerminalUi();
    }
}

__declspec(dllexport) void startPlugin()
{
    g_terminal.SetArmyCommandEnvironment({
        []() { return IsArmyGameLoaded(); },
        []() { return HasResolvableArmyLeader(); },
        []() { return AreArmyTemplatesConfigured(); },
        []() { return IsArmySpawnSystemInitialized(); }
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
            return IsArmyGameLoaded() ? ou->player->getFaction() : nullptr;
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
        }
    });

    g_armyRuntime.SetEnvironment({
        []() { return IsArmyGameLoaded(); },
        []() -> Character*
        {
            return HasResolvableArmyLeader() ? ou->player->getAnyPlayerCharacter() : nullptr;
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
            return IsArmyGameLoaded() ? ou->player->getFaction() : nullptr;
        },
        [](Character* leader) -> Faction*
        {
            return leader != nullptr ? leader->getFaction() : nullptr;
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

    DebugLog("DonJ Kenshi Hack : hooks constructeur, update et game tick installes.");
}
