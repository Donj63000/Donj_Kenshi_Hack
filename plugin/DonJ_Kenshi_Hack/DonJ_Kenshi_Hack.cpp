#include "ArmyDiagnostics.h"
#include "ArmyCommandSpec.h"
#include "ArmyRuntimeManager.h"
#include "BuildGuard.h"
#include "HookAddressResolver.h"
#include "RuntimeIdentity.h"
#include "SpawnManager.h"
#include "TerminalUiBootstrap.h"
#include "TerminalBackend.h"

#include <Debug.h>

#include <array>
#include <cmath>
#include <cstdarg>
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
    const char* kArmyBuildStamp = __DATE__ " " __TIME__;

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

    struct TerminalUiBootstrapTracker
    {
        DonJTerminalUiBootstrap::State state;
        bool attemptLogged;
        bool alreadyPresentLogged;
        bool guiUnavailableLogged;

        TerminalUiBootstrapTracker()
            : state(DonJTerminalUiBootstrap::NotCreated)
            , attemptLogged(false)
            , alreadyPresentLogged(false)
            , guiUnavailableLogged(false)
        {
        }
    };

    struct TerminalUiRuntimeState
    {
        bool windowCreated;
        bool consoleVisible;
        bool inputActive;

        TerminalUiRuntimeState()
            : windowCreated(false)
            , consoleVisible(false)
            , inputActive(false)
        {
        }
    };

    struct HookInstallationState
    {
        bool titleScreenConstructorInstalled;
        bool titleScreenShowInstalled;
        bool titleScreenUpdateInstalled;
        bool guiWindowUpdateInstalled;
        bool inputHandlerKeyDownInstalled;
        bool gameWorldMainLoopInstalled;
        bool createRandomCharacterInstalled;

        HookInstallationState()
            : titleScreenConstructorInstalled(false)
            , titleScreenShowInstalled(false)
            , titleScreenUpdateInstalled(false)
            , guiWindowUpdateInstalled(false)
            , inputHandlerKeyDownInstalled(false)
            , gameWorldMainLoopInstalled(false)
            , createRandomCharacterInstalled(false)
        {
        }
    };

    TerminalUiBootstrapTracker g_terminalUiBootstrap;
    TerminalUiRuntimeState g_terminalUiState;
    HookInstallationState g_hookState;

    TitleScreen* (*TitleScreen_orig)(TitleScreen*) = nullptr;
    void (*TitleScreen_show_orig)(TitleScreen*, bool) = nullptr;
    void (*TitleScreen_update_orig)(TitleScreen*) = nullptr;
    void (*GUIWindow_update_orig)(GUIWindow*) = nullptr;
    void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = nullptr;
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
    bool g_titleScreenUpdateObserved = false;
    bool g_guiWindowUpdateObserved = false;
    bool g_inputHandlerKeyObserved = false;

    struct ObservedNaturalSpawnContext
    {
        RootObjectFactory* factory;
        Faction* faction;
        RootObjectContainer* owner;
        Building* home;
        float age;
        bool valid;

        ObservedNaturalSpawnContext()
            : factory(nullptr)
            , faction(nullptr)
            , owner(nullptr)
            , home(nullptr)
            , age(1.0f)
            , valid(false)
        {
        }
    };

    ObservedNaturalSpawnContext g_observedNaturalSpawnContext;

    bool IsGameWorldReady();
    bool IsArmyGameLoaded();
    bool IsArmyReplayHookInstalled();
    bool HasNaturalArmySpawnOpportunity();
    Platoon* GetCurrentArmyPlayerPlatoon();
    ArmyHandleId RegisterArmyHandleId(const hand& handleValue);
    hand ResolveArmyHandleId(ArmyHandleId handleId);
    Faction* ResolveArmyPlayerFaction();
    RootObjectContainer* ResolveArmySpawnOwnerContainer();
    SpawnPosition ComputeArmyFactorySpawnPosition(const SpawnPosition& spawnOrigin, int requestIndex);
    Character* ResolveArmyCharacterFromCreatedRoot(RootObject* createdRoot);
    void WriteCrashSafeArmyTrace(const std::string& message);
    std::string GetArmyTraceFilePath();
    void ResetObservedNaturalSpawnContext();
    bool ConsumeVirtualKeyPress(int virtualKey);
    void ClearTerminalUiPointers();
    void InvalidateTerminalUi(const char* reason);
    void RecoverTerminalUiFromStructuredException(const char* operationName, unsigned int exceptionCode);
    bool TryDestroyTerminalUiNoCrash(unsigned int& outExceptionCode);
    bool TryReadWidgetVisibleNoCrash(MyGUI::Widget* widget, bool& outVisible, unsigned int& outExceptionCode);
    bool TryReadWidgetAbsoluteRectNoCrash(MyGUI::Widget* widget, MyGUI::IntRect& outRect, unsigned int& outExceptionCode);
    void DestroyTerminalUi();
    void RefreshTerminalUi();
    void ResetKeyboardEdgeTracking(const char* reason);
    void LatchVirtualKeyState(int virtualKey);
    void PollTerminalToggleHotkey(const char* sourceName);
    void ResetTerminalUiBootstrapState(const char* reason);
    void SyncExecutionContextState();
    bool TryCreateTerminalUi();
    void MaybeBootstrapTerminalUi(const char* sourceName, bool hasTitleScreenContext);
    void SyncTerminalUiRuntimeState();
    bool SetTerminalUiVisibility(bool visible, const char* sourceName);
    void HandleTerminalToggleRequest(const char* sourceName);
    void DebugTrace(const std::string& message);
    void DebugTraceFormat(const char* format, ...);
    intptr_t ResolveArmyHookTargetFromRva(std::uintptr_t rva)
    {
        const HMODULE gameModule = GetModuleHandleA(nullptr);
        if (gameModule == nullptr)
        {
            return 0;
        }

        return static_cast<intptr_t>(
            DonJHookAddressResolver::ResolveModuleRva(
                reinterpret_cast<std::uintptr_t>(gameModule),
                rva));
    }

    template <typename TOriginal>
    bool InstallArmyHookFromRva(
        const char* hookName,
        std::uintptr_t rva,
        void* detour,
        TOriginal** original,
        bool required)
    {
        const intptr_t target = ResolveArmyHookTargetFromRva(rva);

        DebugTraceFormat(
            "[TRACE] InstallArmyHookFromRva | hook=%s | rva=0x%I64X | target=%p",
            hookName,
            static_cast<unsigned long long>(rva),
            reinterpret_cast<void*>(target));

        if (target == 0)
        {
            char buffer[256] = {};
            DonjSnprintf(
                buffer,
                sizeof(buffer),
                "DonJ Kenshi Hack : adresse hook introuvable pour %s (RVA=0x%I64X).",
                hookName,
                static_cast<unsigned long long>(rva));
            ErrorLog(buffer);
            DebugTrace(buffer);
            return false;
        }

        if (KenshiLib::SUCCESS != KenshiLib::AddHook(target, detour, original))
        {
            char buffer[256] = {};
            DonjSnprintf(
                buffer,
                sizeof(buffer),
                "DonJ Kenshi Hack : impossible d'installer le hook %s via RVA 0x%I64X.",
                hookName,
                static_cast<unsigned long long>(rva));
            ErrorLog(buffer);
            DebugTrace(buffer);
            return false;
        }

        char buffer[256] = {};
        DonjSnprintf(
            buffer,
            sizeof(buffer),
            "DonJ Kenshi Hack : hook %s installe via RVA 0x%I64X.",
            hookName,
            static_cast<unsigned long long>(rva));
        DebugLog(buffer);
        DebugTrace(buffer);

        if (!required)
        {
            DebugTraceFormat(
                "[TRACE] InstallArmyHookFromRva optionnel actif | hook=%s",
                hookName);
        }

        return true;
    }

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
    bool TryResolveArmyCharacterFromHandleNoCrash(
        const hand& candidateHandle,
        Character*& outCharacter,
        unsigned int& outExceptionCode)
    {
        outCharacter = nullptr;
        outExceptionCode = 0;

        __try
        {
            outCharacter = candidateHandle.getCharacter();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryReadArmyCharacterPositionNoCrash(
        Character* character,
        SpawnPosition& outPosition,
        unsigned int& outExceptionCode)
    {
        outPosition.x = 0.0f;
        outPosition.y = 0.0f;
        outPosition.z = 0.0f;
        outExceptionCode = 0;

        if (character == nullptr)
        {
            return false;
        }

        __try
        {
            const Ogre::Vector3 position = character->getPosition();
            outPosition.x = position.x;
            outPosition.y = position.y;
            outPosition.z = position.z;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryReadArmyCharacterOwnerNoCrash(
        Character* character,
        Faction*& outOwner,
        unsigned int& outExceptionCode)
    {
        outOwner = nullptr;
        outExceptionCode = 0;

        if (character == nullptr)
        {
            return false;
        }

        __try
        {
            outOwner = character->owner;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryRegisterArmyHandleIdNoCrash(
        Character* character,
        ArmyHandleId& outHandleId,
        unsigned int& outExceptionCode)
    {
        outHandleId = 0;
        outExceptionCode = 0;

        if (character == nullptr)
        {
            return false;
        }

        __try
        {
            outHandleId = RegisterArmyHandleId(character->getHandle());
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryResolveArmyCharacterHandleIdNoCrash(
        ArmyHandleId handleId,
        Character*& outCharacter,
        unsigned int& outExceptionCode)
    {
        outCharacter = nullptr;
        outExceptionCode = 0;

        if (handleId == 0)
        {
            return false;
        }

        __try
        {
            hand resolvedHandle = ResolveArmyHandleId(handleId);
            outCharacter = resolvedHandle.getCharacter();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryResolveArmyLeaderFromHandle(const hand& candidateHandle, Character*& outLeader, ArmyHandleId* outHandleId)
    {
        if (candidateHandle.isNull())
        {
            return false;
        }

        Character* resolvedLeader = nullptr;
        unsigned int resolveExceptionCode = 0;
        if (!TryResolveArmyCharacterFromHandleNoCrash(candidateHandle, resolvedLeader, resolveExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] TryResolveArmyLeaderFromHandle : exception structuree 0x%08X pendant handle.getCharacter().",
                resolveExceptionCode);
            return false;
        }

        if (resolvedLeader == nullptr)
        {
            return false;
        }

        SpawnPosition ignoredLeaderPosition;
        unsigned int positionExceptionCode = 0;
        if (!TryReadArmyCharacterPositionNoCrash(resolvedLeader, ignoredLeaderPosition, positionExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] TryResolveArmyLeaderFromHandle : leader rejete, getPosition() a leve 0x%08X.",
                positionExceptionCode);
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

        for (std::size_t index = 0; index < ou->player->playerCharacters.size(); ++index)
        {
            Character* playerCharacter = ou->player->playerCharacters[index];
            SpawnPosition ignoredLeaderPosition;
            unsigned int positionExceptionCode = 0;
            if (TryReadArmyCharacterPositionNoCrash(playerCharacter, ignoredLeaderPosition, positionExceptionCode))
            {
                return playerCharacter;
            }

            DebugTraceFormat(
                "[TRACE] ResolveArmyLeaderCharacter : personnage joueur rejete, getPosition() a leve 0x%08X.",
                positionExceptionCode);
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

    void DebugTraceFormat(const char* format, ...)
    {
        char buffer[768] = {};
        va_list args;
        va_start(args, format);
#if defined(_MSC_VER) && (_MSC_VER <= 1600)
        _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
#else
        std::vsnprintf(buffer, sizeof(buffer), format, args);
#endif
        va_end(args);
        WriteCrashSafeArmyTrace(buffer);
        DebugLog(buffer);
    }

    std::string BuildArmyBuildMarker()
    {
        char buffer[256] = {};
        DonjSnprintf(
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

    void ClearTerminalUiPointers()
    {
        g_hintLabel = nullptr;
        g_executeButton = nullptr;
        g_inputBox = nullptr;
        g_historyBox = nullptr;
        g_window = nullptr;
        g_terminalUiState.windowCreated = false;
        g_terminalUiState.consoleVisible = false;
        g_terminalUiState.inputActive = g_terminal.IsInputActive();
    }

    void InvalidateTerminalUi(const char* reason)
    {
        g_terminal.CancelInput(false);
        ClearTerminalUiPointers();
        g_terminal.MarkUiDirty();
        g_terminalUiBootstrap = TerminalUiBootstrapTracker();

        DebugTraceFormat(
            "DonJ Kenshi Hack : UI terminal invalidee (%s).",
            (reason != nullptr && reason[0] != '\0') ? reason : "raison inconnue");
    }

    void RecoverTerminalUiFromStructuredException(const char* operationName, unsigned int exceptionCode)
    {
        char buffer[256] = {};
        DonjSnprintf(
            buffer,
            sizeof(buffer),
            "DonJ Kenshi Hack : exception structuree 0x%08X pendant %s.",
            exceptionCode,
            (operationName != nullptr && operationName[0] != '\0') ? operationName : "operation UI");
        ErrorLog(buffer);
        DebugTrace(buffer);
        InvalidateTerminalUi(operationName);
    }

    bool TryDestroyTerminalUiNoCrash(unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
            if (gui != nullptr && g_window != nullptr)
            {
                gui->destroyWidget(g_window);
            }
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryReadWidgetVisibleNoCrash(MyGUI::Widget* widget, bool& outVisible, unsigned int& outExceptionCode)
    {
        outVisible = false;
        outExceptionCode = 0;

        if (widget == nullptr)
        {
            return true;
        }

        __try
        {
            outVisible = widget->getVisible();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryReadWidgetAbsoluteRectNoCrash(MyGUI::Widget* widget, MyGUI::IntRect& outRect, unsigned int& outExceptionCode)
    {
        outRect = MyGUI::IntRect();
        outExceptionCode = 0;

        if (widget == nullptr)
        {
            return true;
        }

        __try
        {
            outRect = widget->getAbsoluteRect();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    void DestroyTerminalUi()
    {
        unsigned int destroyExceptionCode = 0;
        if (!TryDestroyTerminalUiNoCrash(destroyExceptionCode))
        {
            RecoverTerminalUiFromStructuredException("DestroyTerminalUi", destroyExceptionCode);
            return;
        }

        g_terminal.CancelInput(false);
        ClearTerminalUiPointers();
        g_terminal.MarkUiDirty();
    }

    void SyncTerminalUiRuntimeState()
    {
        g_terminalUiState.windowCreated = (g_window != nullptr);
        g_terminalUiState.consoleVisible = false;
        g_terminalUiState.inputActive = g_terminal.IsInputActive();

        if (g_window == nullptr)
        {
            return;
        }

        bool visible = false;
        unsigned int exceptionCode = 0;
        if (!TryReadWidgetVisibleNoCrash(g_window, visible, exceptionCode))
        {
            RecoverTerminalUiFromStructuredException("SyncTerminalUiRuntimeState/getVisible", exceptionCode);
            return;
        }

        g_terminalUiState.consoleVisible = visible;
    }

    bool SetTerminalUiVisibility(bool visible, const char* sourceName)
    {
        if (g_window == nullptr)
        {
            SyncTerminalUiRuntimeState();
            return false;
        }

        if (!visible)
        {
            g_terminal.CancelInput(false);
        }

        unsigned int exceptionCode = 0;
        __try
        {
            g_window->setVisible(visible);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptionCode = static_cast<unsigned int>(GetExceptionCode());
        }

        if (exceptionCode != 0)
        {
            RecoverTerminalUiFromStructuredException("SetTerminalUiVisibility/setVisible", exceptionCode);
            return false;
        }

        SyncTerminalUiRuntimeState();
        g_terminal.MarkUiDirty();
        RefreshTerminalUi();
        ResetKeyboardEdgeTracking(visible ? "console ouverte" : "console fermee");

        DebugTraceFormat(
            "DonJ Kenshi Hack : console %s (%s).",
            visible ? "visible" : "cachee",
            sourceName != nullptr ? sourceName : "raison inconnue");

        return true;
    }

    void ResetTerminalUiBootstrapState(const char* reason)
    {
        g_terminalUiBootstrap = TerminalUiBootstrapTracker();
        g_titleScreenUpdateObserved = false;
        g_guiWindowUpdateObserved = false;
        g_inputHandlerKeyObserved = false;
        SyncTerminalUiRuntimeState();
        if (g_window != nullptr)
        {
            g_terminalUiBootstrap.state = DonJTerminalUiBootstrap::Created;
        }

        if (reason != nullptr && reason[0] != '\0')
        {
            DebugTraceFormat(
                "DonJ Kenshi Hack : bootstrap UI reinitialise (%s) | fenetre=%s.",
                reason,
                g_window != nullptr ? "presente" : "absente");
        }
    }

    void LatchVirtualKeyState(int virtualKey)
    {
        const unsigned int safeIndex = static_cast<unsigned int>(virtualKey) & 0xFFu;
        g_virtualKeyStates[safeIndex] = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    }

    void PollTerminalToggleHotkey(const char* sourceName)
    {
        if (!ConsumeVirtualKeyPress(VK_F10))
        {
            return;
        }

        DebugTraceFormat(
            "DonJ Kenshi Hack : F10 detecte par polling (%s).",
            sourceName != nullptr ? sourceName : "source inconnue");

        HandleTerminalToggleRequest(sourceName);
        LatchVirtualKeyState(VK_F10);
    }

    void HandleTerminalToggleRequest(const char* sourceName)
    {
        SyncExecutionContextState();
        SyncTerminalUiRuntimeState();

        const DonJTerminalUiBootstrap::ToggleDecision toggleDecision =
            DonJTerminalUiBootstrap::EvaluateToggle(
                DonJTerminalUiBootstrap::ToggleContext(
                    true,
                    MyGUI::Gui::getInstancePtr() != nullptr,
                    g_window != nullptr,
                    g_terminalUiState.consoleVisible,
                    IsGameWorldReady()));

        if (toggleDecision == DonJTerminalUiBootstrap::CreateAndShowWindow)
        {
            DebugTraceFormat(
                "DonJ Kenshi Hack : F10 demande la creation puis l'ouverture (%s).",
                sourceName != nullptr ? sourceName : "source inconnue");
            if (TryCreateTerminalUi())
            {
                SetTerminalUiVisibility(true, sourceName);
            }
            else
            {
                ErrorLog("DonJ Kenshi Hack : F10 n'a pas pu creer le terminal.");
            }
            LatchVirtualKeyState(VK_F10);
            return;
        }

        if (toggleDecision == DonJTerminalUiBootstrap::ShowExistingWindow)
        {
            SetTerminalUiVisibility(true, sourceName);
            LatchVirtualKeyState(VK_F10);
            return;
        }

        if (toggleDecision == DonJTerminalUiBootstrap::HideWindow)
        {
            SetTerminalUiVisibility(false, sourceName);
            LatchVirtualKeyState(VK_F10);
            return;
        }

        DebugTraceFormat(
            "DonJ Kenshi Hack : F10 ignore (%s) | gui=%s | fenetre=%s | visible=%s | gameplay=%s.",
            sourceName != nullptr ? sourceName : "source inconnue",
            MyGUI::Gui::getInstancePtr() != nullptr ? "oui" : "non",
            g_window != nullptr ? "oui" : "non",
            g_terminalUiState.consoleVisible ? "oui" : "non",
            IsGameWorldReady() ? "oui" : "non");

        LatchVirtualKeyState(VK_F10);
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
        if (widget == nullptr)
        {
            return false;
        }

        bool visible = false;
        unsigned int visibleExceptionCode = 0;
        if (!TryReadWidgetVisibleNoCrash(widget, visible, visibleExceptionCode))
        {
            RecoverTerminalUiFromStructuredException("IsPointInsideWidget/getVisible", visibleExceptionCode);
            return false;
        }

        if (!visible)
        {
            return false;
        }

        MyGUI::IntRect widgetRect;
        unsigned int rectExceptionCode = 0;
        if (!TryReadWidgetAbsoluteRectNoCrash(widget, widgetRect, rectExceptionCode))
        {
            RecoverTerminalUiFromStructuredException("IsPointInsideWidget/getAbsoluteRect", rectExceptionCode);
            return false;
        }

        return
            x >= widgetRect.left &&
            y >= widgetRect.top &&
            x < widgetRect.right &&
            y < widgetRect.bottom;
    }

    void RefreshTerminalUi()
    {
#if (_MSC_VER == 1600)
        // La je garde un fallback VC100, car ce compilateur refuse __try
        // dans ce chemin quand MyGUI manipule des temporaires C++ de texte.
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
#else
        unsigned int exceptionCode = 0;

        __try
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
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptionCode = static_cast<unsigned int>(GetExceptionCode());
        }

        if (exceptionCode != 0)
        {
            RecoverTerminalUiFromStructuredException("RefreshTerminalUi", exceptionCode);
        }
#endif
    }

    bool IsGameWorldReady()
    {
        return ou != nullptr && ou->initialized;
    }

    ArmyHandleId HashArmyHandleString(const std::string& value)
    {
        const ArmyHandleId prime = 1099511628211ULL;
        ArmyHandleId hash = 1469598103934665603ULL;

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            hash ^= static_cast<unsigned char>(value[index]);
            hash *= prime;
        }

        return hash;
    }

    ArmyHandleId RegisterArmyHandleId(const hand& handleValue)
    {
        if (handleValue.isNull())
        {
            return 0;
        }

        const std::string serialisedHandle = handleValue.toString();
        ArmyHandleId handleId = HashArmyHandleString(serialisedHandle);
        if (handleId == 0)
        {
            handleId = 1;
        }

        while (true)
        {
            std::unordered_map<ArmyHandleId, std::string>::const_iterator existingIt = g_armyHandleLookup.find(handleId);
            if (existingIt == g_armyHandleLookup.end())
            {
                g_armyHandleLookup[handleId] = serialisedHandle;
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
        std::unordered_map<ArmyHandleId, std::string>::const_iterator it = g_armyHandleLookup.find(handleId);
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
        Character* resolvedCharacter = nullptr;
        unsigned int resolveExceptionCode = 0;
        if (!TryResolveArmyCharacterHandleIdNoCrash(handleId, resolvedCharacter, resolveExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] ResolveArmyCharacterHandleId : exception structuree 0x%08X pendant la resolution du handle %llu.",
                resolveExceptionCode,
                static_cast<unsigned long long>(handleId));
            return nullptr;
        }

        return resolvedCharacter;
    }

    bool IsArmyCharacterDeadSafe(Character* character)
    {
        if (character == nullptr)
        {
            return true;
        }

        __try
        {
            return character->isDead();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            DebugTraceFormat(
                "[TRACE] IsArmyCharacterDeadSafe : exception structuree 0x%08X.",
                static_cast<unsigned int>(GetExceptionCode()));
            return true;
        }
    }

    bool IsArmyCharacterUnconsciousSafe(Character* character)
    {
        if (character == nullptr)
        {
            return false;
        }

        __try
        {
            return character->isUnconcious();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            DebugTraceFormat(
                "[TRACE] IsArmyCharacterUnconsciousSafe : exception structuree 0x%08X.",
                static_cast<unsigned int>(GetExceptionCode()));
            return true;
        }
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
        if (leader == nullptr)
        {
            return nullptr;
        }

        Faction* resolvedOwner = nullptr;
        unsigned int ownerExceptionCode = 0;
        if (!TryReadArmyCharacterOwnerNoCrash(leader, resolvedOwner, ownerExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] ResolveArmyPlayerFaction : owner inaccessible, exception 0x%08X.",
                ownerExceptionCode);
            return nullptr;
        }

        return resolvedOwner;
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
        for (std::size_t index = 0; index < spec.templateNames.size(); ++index)
        {
            const char* templateName = spec.templateNames[index];
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

        return
            g_hookState.createRandomCharacterInstalled &&
            RootObjectFactory_createRandomCharacter_orig != nullptr;
    }

    bool IsArmyReplayHookInstalled()
    {
        return
            g_hookState.createRandomCharacterInstalled &&
            RootObjectFactory_createRandomCharacter_orig != nullptr;
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
            DebugTrace("[TRACE] TryResolveArmySpawnOrigin : aucun leader resolvable.");
            return false;
        }

        SpawnPosition leaderPosition;
        unsigned int positionExceptionCode = 0;
        if (!TryReadArmyCharacterPositionNoCrash(leader, leaderPosition, positionExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] TryResolveArmySpawnOrigin : getPosition() a leve 0x%08X.",
                positionExceptionCode);
            return false;
        }

        outPosition.x = leaderPosition.x;
        outPosition.y = leaderPosition.y;
        outPosition.z = leaderPosition.z;
        DebugTraceFormat(
            "[TRACE] TryResolveArmySpawnOrigin succes | leader=%p | pos=(%.2f, %.2f, %.2f)",
            static_cast<void*>(leader),
            static_cast<double>(leaderPosition.x),
            static_cast<double>(leaderPosition.y),
            static_cast<double>(leaderPosition.z));
        return true;
    }

    SpawnPosition GetArmyCharacterPosition(Character* character)
    {
        SpawnPosition spawnPosition;
        unsigned int positionExceptionCode = 0;
        if (!TryReadArmyCharacterPositionNoCrash(character, spawnPosition, positionExceptionCode))
        {
            if (character != nullptr)
            {
                DebugTraceFormat(
                    "[TRACE] GetArmyCharacterPosition : getPosition() a leve 0x%08X.",
                    positionExceptionCode);
            }
            return spawnPosition;
        }
        return spawnPosition;
    }

    bool TryApplyArmyFactionNoCrash(
        Character* character,
        Faction* faction,
        ActivePlatoon* platoon,
        unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            character->setFaction(faction, platoon);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryRenameArmyCharacterNoCrash(
        Character* character,
        const std::string& name,
        unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            character->setName(name);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryTeleportArmyCharacterNoCrash(
        Character* character,
        const SpawnPosition& position,
        unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            Ogre::Vector3 targetPosition = character->getPosition();
            targetPosition.x = position.x;
            targetPosition.y = position.y;
            targetPosition.z = position.z;
            character->teleport(targetPosition);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TrySetArmyFollowTargetNoCrash(
        Character* character,
        Character* leader,
        unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            CharMovement* movement = character->getMovement();
            if (movement != nullptr)
            {
                movement->setDestination(leader, HIGH_PRIORITY);
            }
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryApplyArmyEscortOrderNoCrash(
        Character* character,
        ArmyEscortOrder order,
        unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
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
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TrySetArmyEscortRoleNoCrash(Character* character, unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            character->setSquadMemberType(SQUAD_1);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryRethinkArmyAiNoCrash(Character* character, unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            character->reThinkCurrentAIAction();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    bool TryFindValidArmySpawnPosNoCrash(
        Ogre::Vector3& inOutCandidate,
        const Ogre::Vector3& center,
        unsigned int& outExceptionCode)
    {
        outExceptionCode = 0;

        __try
        {
            ou->findValidSpawnPos(inOutCandidate, center);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
    }

    void ApplyArmyFaction(Character* character, Faction* faction, ActivePlatoon* platoon)
    {
        if (character == nullptr || faction == nullptr)
        {
            return;
        }

        unsigned int exceptionCode = 0;
        if (!TryApplyArmyFactionNoCrash(character, faction, platoon, exceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] ApplyArmyFaction : exception structuree 0x%08X.",
                exceptionCode);
        }
    }

    void RenameArmyCharacter(Character* character, const std::string& name)
    {
        if (character == nullptr || name.empty())
        {
            return;
        }

        unsigned int exceptionCode = 0;
        if (!TryRenameArmyCharacterNoCrash(character, name, exceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] RenameArmyCharacter : exception structuree 0x%08X.",
                exceptionCode);
        }
    }

    void TeleportArmyCharacter(Character* character, const SpawnPosition& position)
    {
        if (character == nullptr)
        {
            return;
        }

        unsigned int exceptionCode = 0;
        if (!TryTeleportArmyCharacterNoCrash(character, position, exceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] TeleportArmyCharacter : exception structuree 0x%08X.",
                exceptionCode);
        }
    }

    void SetArmyFollowTarget(Character* character, Character* leader)
    {
        if (character == nullptr || leader == nullptr)
        {
            return;
        }

        unsigned int exceptionCode = 0;
        if (!TrySetArmyFollowTargetNoCrash(character, leader, exceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] SetArmyFollowTarget : exception structuree 0x%08X.",
                exceptionCode);
        }
    }

    void ApplyArmyEscortOrder(Character* character, ArmyEscortOrder order)
    {
        if (character == nullptr)
        {
            return;
        }

        unsigned int exceptionCode = 0;
        if (!TryApplyArmyEscortOrderNoCrash(character, order, exceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] ApplyArmyEscortOrder : exception structuree 0x%08X.",
                exceptionCode);
        }
    }

    void SetArmyEscortRole(Character* character)
    {
        if (character == nullptr)
        {
            return;
        }

        unsigned int exceptionCode = 0;
        if (!TrySetArmyEscortRoleNoCrash(character, exceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] SetArmyEscortRole : exception structuree 0x%08X.",
                exceptionCode);
        }
    }

    void RethinkArmyAi(Character* character)
    {
        if (character == nullptr)
        {
            return;
        }

        unsigned int exceptionCode = 0;
        if (!TryRethinkArmyAiNoCrash(character, exceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] RethinkArmyAi : exception structuree 0x%08X.",
                exceptionCode);
        }
    }

    SpawnPosition ComputeArmyFactorySpawnPosition(const SpawnPosition& spawnOrigin, int requestIndex)
    {
        constexpr float kPi = 3.14159265358979323846f;

        const int safeIndex = requestIndex < 0 ? 0 : requestIndex;
        const int slotIndex = safeIndex % 8;
        const int ringIndex = safeIndex / 8;
        const float radius = 1.5f + (static_cast<float>(ringIndex) * 1.15f);
        const float angle = (2.0f * kPi * static_cast<float>(slotIndex) / 8.0f) + (static_cast<float>(ringIndex) * 0.33f);

        DONJ_ALIGNAS_16 unsigned char centerStorage[sizeof(Ogre::Vector3)] = {};
        DONJ_ALIGNAS_16 unsigned char candidateStorage[sizeof(Ogre::Vector3)] = {};
        Ogre::Vector3& center = *reinterpret_cast<Ogre::Vector3*>(centerStorage);
        Ogre::Vector3& candidate = *reinterpret_cast<Ogre::Vector3*>(candidateStorage);

        center.x = spawnOrigin.x;
        center.y = spawnOrigin.y;
        center.z = spawnOrigin.z;

        candidate.x = center.x + (std::cos(angle) * radius);
        candidate.y = center.y;
        candidate.z = center.z + (std::sin(angle) * radius);

        if (IsGameWorldReady())
        {
            unsigned int findSpawnExceptionCode = 0;
            if (!TryFindValidArmySpawnPosNoCrash(candidate, center, findSpawnExceptionCode))
            {
                DebugTraceFormat(
                    "[TRACE] ComputeArmyFactorySpawnPosition : findValidSpawnPos a leve 0x%08X.",
                    findSpawnExceptionCode);
            }
        }

        SpawnPosition spawnPosition;
        spawnPosition.x = candidate.x;
        spawnPosition.y = candidate.y;
        spawnPosition.z = candidate.z;
        return spawnPosition;
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

        if (!g_hookState.createRandomCharacterInstalled ||
            RootObjectFactory_createRandomCharacter_orig == nullptr)
        {
            result.outcome = SpawnAttemptOutcome::DeferredAwaitingReplayHook;
            result.shouldRequeue = true;
            result.detail = "[INFO] Spawn differe : hook RootObjectFactory::createRandomCharacter non installe sur cette build.";
            return result;
        }

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

        const SpawnPosition factorySpawnPosition = ComputeArmyFactorySpawnPosition(spawnOrigin, request.index);
        DONJ_ALIGNAS_16 unsigned char factorySpawnStorage[sizeof(Ogre::Vector3)] = {};
        Ogre::Vector3& factorySpawnVector = *reinterpret_cast<Ogre::Vector3*>(factorySpawnStorage);
        factorySpawnVector.x = factorySpawnPosition.x;
        factorySpawnVector.y = factorySpawnPosition.y;
        factorySpawnVector.z = factorySpawnPosition.z;
        DebugTraceFormat(
            "[TRACE] TrySpawnArmyUnitThroughFactory tentative | req=%s | template=%p | faction_nat=%p | owner_nat=%p | factory=%p | pos=(%.2f, %.2f, %.2f)",
            request.templateName.c_str(),
            static_cast<void*>(templateData),
            static_cast<void*>(replayContext.faction),
            static_cast<void*>(replayContext.owner),
            static_cast<void*>(replayContext.factory),
            static_cast<double>(factorySpawnVector.x),
            static_cast<double>(factorySpawnVector.y),
            static_cast<double>(factorySpawnVector.z));

        ++g_armyReplayDepth;
        RootObject* createdRoot = nullptr;
        unsigned int factoryExceptionCode = 0;
        const bool factoryCallSucceeded = TryInvokeArmyCreateRandomCharacterNoCrash(
            replayContext.factory,
            replayContext.faction,
            factorySpawnVector,
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

        ArmyHandleId createdHandleId = 0;
        unsigned int handleExceptionCode = 0;
        if (!TryRegisterArmyHandleIdNoCrash(createdCharacter, createdHandleId, handleExceptionCode))
        {
            char buffer[320] = {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "[ERREUR] Factory Kenshi : exception structuree 0x%08X pendant la lecture du handle du Character cree.",
                handleExceptionCode);
            result.outcome = SpawnAttemptOutcome::FailedFactoryCallFatal;
            result.shouldRequeue = false;
            result.detail = buffer;
            return result;
        }

        DebugTraceFormat(
            "[TRACE] TrySpawnArmyUnitThroughFactory succes | root=%p | character=%p | handle_id=%llu",
            static_cast<void*>(createdRoot),
            static_cast<void*>(createdCharacter),
            static_cast<unsigned long long>(createdHandleId));

        result.outcome = SpawnAttemptOutcome::Spawned;
        result.character = createdCharacter;
        result.shouldRequeue = false;
        return result;
    }

    bool TryTickArmyReplayNoCrash(std::size_t& outSpawnedByReplay, unsigned int& outExceptionCode)
    {
        outSpawnedByReplay = 0;
        outExceptionCode = 0;

        __try
        {
            ArmySession& session = g_terminal.GetArmySession();
            outSpawnedByReplay = g_spawnManager.Tick(session, 0.0f);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outExceptionCode = static_cast<unsigned int>(GetExceptionCode());
            return false;
        }
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
        if (RootObjectFactory_createRandomCharacter_orig == nullptr)
        {
            DebugTrace("[ERREUR] RootObjectFactory_createRandomCharacter_hook : pointeur original nul.");
            return nullptr;
        }

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

            std::size_t spawnedByReplay = 0;
            unsigned int replayExceptionCode = 0;
            const bool replaySucceeded = TryTickArmyReplayNoCrash(spawnedByReplay, replayExceptionCode);

            if (!replaySucceeded)
            {
                DebugTraceFormat(
                    "[ERREUR] Replay /army dans RootObjectFactory::createRandomCharacter : exception structuree 0x%08X. Session neutralisee defensivement.",
                    replayExceptionCode);

                g_spawnManager.Reset();
                session.pendingRequests.clear();
                session.pendingFinalizeUnits.clear();
                session.pendingRequestCount = 0;
                session.waitingForReplayOpportunity = false;
                session.active = false;
                session.remainingSeconds = 0.0f;
                session.state = ArmyState::Dismissing;
            }
            else if (spawnedByReplay > 0)
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
            if (g_window != nullptr)
            {
                DestroyTerminalUi();
            }

            ResetKeyboardEdgeTracking("entree en jeu");
            ResetTerminalUiBootstrapState("entree en jeu");
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
            g_armyHandleLookup.clear();
            g_lastObservedArmyFactory = nullptr;
            g_currentArmyReplayFactory = nullptr;
            g_armyReplayOpportunityActive = false;
            g_armyReplayDepth = 0;
            ResetObservedNaturalSpawnContext();
            if (g_window != nullptr)
            {
                DestroyTerminalUi();
            }

            ResetKeyboardEdgeTracking("retour menu");
            ResetTerminalUiBootstrapState("retour menu");
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

    bool TryCreateTerminalUi()
    {
        if (g_window != nullptr)
        {
            SyncTerminalUiRuntimeState();
            return true;
        }

        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (gui == nullptr)
        {
            return false;
        }

        ClearTerminalUiPointers();

        g_window = gui->createWidgetReal<MyGUI::Window>(
            "Kenshi_WindowCX",
            0.08f, 0.08f, 0.45f, 0.43f,
            MyGUI::Align::Default,
            "Window",
            "DonJHackWindow");

        if (g_window == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer la fenetre du terminal.");
            return false;
        }

        g_window->setCaption("DonJ Kenshi Hack");
        g_window->setVisible(false);

        MyGUI::Widget* clientWidget = g_window->getClientWidget();
        if (clientWidget == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : client widget introuvable.");
            DestroyTerminalUi();
            return false;
        }

        MyGUI::Widget* historyPanel = clientWidget->createWidgetReal<MyGUI::Widget>(
            "Kenshi_GenericTextBoxSkin",
            0.04f, 0.05f, 0.92f, 0.58f,
            MyGUI::Align::Default,
            "DonJHackHistoryPanel");
        if (historyPanel == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer le panneau d'historique.");
            DestroyTerminalUi();
            return false;
        }

        g_historyBox = historyPanel->createWidgetReal<MyGUI::EditBox>(
            "Kenshi_EditBoxStrechEmpty",
            0.025f, 0.07f, 0.95f, 0.88f,
            MyGUI::Align::Stretch,
            "DonJHackHistoryBox");

        if (g_historyBox == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer l'historique du terminal.");
            DestroyTerminalUi();
            return false;
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
        if (inputPanel == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer le panneau de saisie.");
            DestroyTerminalUi();
            return false;
        }

        g_inputBox = inputPanel->createWidgetReal<MyGUI::EditBox>(
            "Kenshi_EditBoxStrechEmpty",
            0.025f, 0.18f, 0.95f, 0.64f,
            MyGUI::Align::Stretch,
            "DonJHackInputBox");

        if (g_inputBox == nullptr)
        {
            ErrorLog("DonJ Kenshi Hack : impossible de creer la zone de saisie.");
            DestroyTerminalUi();
            return false;
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
            DestroyTerminalUi();
            return false;
        }

        g_executeButton->setCaption("Executer");

        g_hintLabel = clientWidget->createWidgetReal<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            0.04f, 0.87f, 0.92f, 0.08f,
            MyGUI::Align::Default,
            "DonJHackHintLabel");

        if (g_hintLabel != nullptr)
        {
            g_hintLabel->setCaption("F10 : ouvrir ou fermer | Entree : ouvrir ou valider | Echap : annuler");
            g_hintLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        }

        SyncTerminalUiRuntimeState();
        g_terminal.MarkUiDirty();
        RefreshTerminalUi();
        return true;
    }

    void MaybeBootstrapTerminalUi(const char* sourceName, bool hasTitleScreenContext)
    {
        const DonJTerminalUiBootstrap::Context context(
            hasTitleScreenContext,
            MyGUI::Gui::getInstancePtr() != nullptr,
            g_window != nullptr,
            IsGameWorldReady());

        const DonJTerminalUiBootstrap::Decision decision =
            DonJTerminalUiBootstrap::Evaluate(context, g_terminalUiBootstrap.state);

        if (decision == DonJTerminalUiBootstrap::WindowAlreadyPresent)
        {
            g_terminalUiBootstrap.state = DonJTerminalUiBootstrap::Created;
            SyncTerminalUiRuntimeState();
            if (!g_terminalUiBootstrap.alreadyPresentLogged)
            {
                DebugTraceFormat(
                    "DonJ Kenshi Hack : creation UI ignoree (%s), fenetre deja presente.",
                    sourceName);
                g_terminalUiBootstrap.alreadyPresentLogged = true;
            }
            return;
        }

        if (decision == DonJTerminalUiBootstrap::GuiUnavailable)
        {
            if (!g_terminalUiBootstrap.guiUnavailableLogged)
            {
                ErrorLog("DonJ Kenshi Hack : MyGUI::Gui introuvable.");
                DebugTraceFormat(
                    "DonJ Kenshi Hack : creation UI impossible (%s), MyGUI::Gui indisponible.",
                    sourceName);
                g_terminalUiBootstrap.guiUnavailableLogged = true;
            }
            return;
        }

        if (decision != DonJTerminalUiBootstrap::AttemptCreate)
        {
            return;
        }

        if (!g_terminalUiBootstrap.attemptLogged)
        {
            DebugTraceFormat(
                "DonJ Kenshi Hack : tentative creation terminal (%s).",
                sourceName);
            g_terminalUiBootstrap.attemptLogged = true;
        }

        if (TryCreateTerminalUi())
        {
            g_terminalUiBootstrap.state = DonJTerminalUiBootstrap::Created;
            SyncTerminalUiRuntimeState();
            DebugLog("DonJ Kenshi Hack : fenetre terminal creee.");
            DebugTraceFormat("DonJ Kenshi Hack : creation UI OK (%s).", sourceName);
            return;
        }

        g_terminalUiBootstrap.state = DonJTerminalUiBootstrap::Failed;
        DebugTraceFormat("DonJ Kenshi Hack : creation UI en echec (%s).", sourceName);
    }

    void ProcessTerminalMouseInput()
    {
        SyncTerminalUiRuntimeState();
        if (!DonJTerminalUiBootstrap::ShouldCaptureKeyboard(
                g_terminalUiState.windowCreated,
                g_terminalUiState.consoleVisible))
        {
            return;
        }

        if (key == nullptr || !key->mLUp)
        {
            return;
        }

        const int cursorX = static_cast<int>(key->mPosAbs.x);
        const int cursorY = static_cast<int>(key->mPosAbs.y);

        if (IsPointInsideWidget(g_inputBox, cursorX, cursorY))
        {
            g_terminal.ActivateInput();
            SyncTerminalUiRuntimeState();
            DebugLog("DonJ Kenshi Hack : saisie activee par clic.");
            return;
        }

        if (IsPointInsideWidget(g_executeButton, cursorX, cursorY))
        {
            g_terminal.ActivateInput();
            SyncTerminalUiRuntimeState();
            if (g_terminal.SubmitCurrentInput())
            {
                SyncTerminalUiRuntimeState();
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
        SyncTerminalUiRuntimeState();
        if (!DonJTerminalUiBootstrap::ShouldCaptureKeyboard(
                g_terminalUiState.windowCreated,
                g_terminalUiState.consoleVisible))
        {
            ConsumePrintableKeys(false);
            return;
        }

        const bool enterPressed = ConsumeVirtualKeyPress(VK_RETURN);
        const bool escapePressed = ConsumeVirtualKeyPress(VK_ESCAPE);
        const bool backspacePressed = ConsumeVirtualKeyPress(VK_BACK);
        const bool upPressed = ConsumeVirtualKeyPress(VK_UP);
        const bool downPressed = ConsumeVirtualKeyPress(VK_DOWN);

        if (escapePressed && g_terminal.IsInputActive())
        {
            g_terminal.CancelInput();
            SyncTerminalUiRuntimeState();
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
                SyncTerminalUiRuntimeState();
                DebugTrace("DonJ Kenshi Hack : saisie activee.");
            }
            else if (g_terminal.SubmitCurrentInput())
            {
                SyncTerminalUiRuntimeState();
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
            SyncTerminalUiRuntimeState();
        }

        if (upPressed)
        {
            g_terminal.NavigateCommandHistory(-1);
            SyncTerminalUiRuntimeState();
        }

        if (downPressed)
        {
            g_terminal.NavigateCommandHistory(1);
            SyncTerminalUiRuntimeState();
        }

        ConsumePrintableKeys(true);
        SyncTerminalUiRuntimeState();
    }

    void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
    {
        InputHandler_keyDownEvent_orig(thisptr, keyCode);
        SyncExecutionContextState();

        if (!g_inputHandlerKeyObserved)
        {
            DebugTrace("[TRACE] InputHandler_keyDownEvent_hook observe.");
            g_inputHandlerKeyObserved = true;
        }

        if (keyCode == OIS::KC_F10)
        {
            HandleTerminalToggleRequest("InputHandler::keyDownEvent");
            LatchVirtualKeyState(VK_F10);
            SyncTerminalUiRuntimeState();
            if (!g_terminalUiState.consoleVisible)
            {
                return;
            }
        }

        if (IsGameWorldReady() || g_window == nullptr)
        {
            return;
        }

        ProcessTerminalKeyboardInput();
        ProcessDeferredTerminalCommands("la saisie menu");
        RefreshTerminalUi();
    }

    TitleScreen* TitleScreen_hook(TitleScreen* thisptr)
    {
        TitleScreen* titleScreen = TitleScreen_orig(thisptr);
        SyncExecutionContextState();
        MaybeBootstrapTerminalUi("TitleScreen::_CONSTRUCTOR", thisptr != nullptr);
        return titleScreen;
    }

    void TitleScreen_show_hook(TitleScreen* thisptr, bool on)
    {
        TitleScreen_show_orig(thisptr, on);
        SyncExecutionContextState();
        if (on)
        {
            DebugTrace("[TRACE] TitleScreen_show_hook observe.");
            MaybeBootstrapTerminalUi("TitleScreen::_NV_show", thisptr != nullptr);
        }
    }

    void TitleScreen_update_hook(TitleScreen* thisptr)
    {
        TitleScreen_update_orig(thisptr);
        SyncExecutionContextState();
        if (!g_titleScreenUpdateObserved)
        {
            DebugTrace("[TRACE] TitleScreen_update_hook observe.");
            g_titleScreenUpdateObserved = true;
        }
        MaybeBootstrapTerminalUi("TitleScreen::_NV_update", thisptr != nullptr);
        PollTerminalToggleHotkey("TitleScreen::_NV_update polling");

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

    void GUIWindow_update_hook(GUIWindow* thisptr)
    {
        GUIWindow_update_orig(thisptr);
        SyncExecutionContextState();

        if (!g_guiWindowUpdateObserved)
        {
            DebugTrace("[TRACE] GUIWindow_update_hook observe.");
            g_guiWindowUpdateObserved = true;
        }

        if (g_window == nullptr && !IsGameWorldReady())
        {
            // La je garde GUIWindow uniquement comme filet de securite de
            // bootstrap si TitleScreen ne declenche pas la creation sur cette build.
            MaybeBootstrapTerminalUi("GUIWindow::_NV_update (fallback)", true);
        }

        PollTerminalToggleHotkey("GUIWindow::_NV_update polling");
    }

    void GameWorld_mainLoop_hook(GameWorld* thisptr, float time)
    {
        GameWorld_mainLoop_orig(thisptr, time);
        SyncExecutionContextState();
        PollTerminalToggleHotkey("GameWorld::_NV_mainLoop_GPUSensitiveStuff polling");

        const ArmySession& sessionBeforeInput = g_terminal.GetArmySession();
        if (ShouldTraceArmyTick(sessionBeforeInput))
        {
            TraceArmyEnvironmentPoint("GameWorld_mainLoop_hook debut");
            TraceArmySessionPoint("game tick debut", sessionBeforeInput);
        }

        ProcessTerminalKeyboardInput();
        if (g_window != nullptr)
        {
            ProcessTerminalMouseInput();
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

// La j'exporte startPlugin comme dans les exemples KenshiLib, car RE_Kenshi
// s'appuie sur cette convention d'export C++ pour retrouver l'entree plugin.
__declspec(dllexport) void startPlugin()
{
    DeleteFileA(GetArmyTraceFilePath().c_str());
    const std::string buildMarker = BuildArmyBuildMarker();
    WriteCrashSafeArmyTrace(buildMarker);
    DebugLog(buildMarker.c_str());
#if (_MSC_VER != 1600)
    DebugTrace("[WARN] Build toolset : ce plugin tourne avec un compilateur MSVC moderne, pas avec Visual C++ 2010.");
#endif

    ArmyCommandEnvironment armyCommandEnvironment;
    armyCommandEnvironment.isGameLoaded = []() { return IsArmyGameLoaded(); };
    armyCommandEnvironment.hasResolvableLeader = []() { return HasResolvableArmyLeader(); };
    armyCommandEnvironment.areArmyTemplatesAvailable = []() { return AreArmyTemplatesConfigured(); };
    armyCommandEnvironment.isSpawnSystemReady = []() { return IsArmySpawnSystemInitialized(); };
    armyCommandEnvironment.debugTrace = [](const std::string& message) { DebugTrace(message); };
    armyCommandEnvironment.isFactoryAvailable = []() { return IsArmyFactoryAvailable(); };
    armyCommandEnvironment.isReplayHookInstalled = []() { return IsArmyReplayHookInstalled(); };
    g_terminal.SetArmyCommandEnvironment(armyCommandEnvironment);

    SpawnManagerEnvironment spawnManagerEnvironment;
    spawnManagerEnvironment.isGameLoaded = []() { return IsArmyGameLoaded(); };
    spawnManagerEnvironment.isFactoryAvailable = []() { return IsArmyFactoryAvailable(); };
    spawnManagerEnvironment.isReplayHookInstalled = []() { return IsArmyReplayHookInstalled(); };
    spawnManagerEnvironment.hasNaturalSpawnOpportunity = []() { return HasNaturalArmySpawnOpportunity(); };
    spawnManagerEnvironment.resolveTemplate = [](const std::string& templateName) -> void*
    {
        return ResolveArmyTemplateByName(templateName);
    };
    spawnManagerEnvironment.resolvePlayerFaction = []() -> Faction*
    {
        return ResolveArmyPlayerFaction();
    };
    spawnManagerEnvironment.resolveSpawnOrigin = [](SpawnPosition& outPosition) -> bool
    {
        return TryResolveArmySpawnOrigin(outPosition);
    };
    spawnManagerEnvironment.trySpawnThroughFactory = [](const SpawnRequest& request, void* resolvedTemplate, Faction* playerFaction, const SpawnPosition& spawnOrigin) -> SpawnAttemptResult
    {
        return TrySpawnArmyUnitThroughFactory(request, resolvedTemplate, playerFaction, spawnOrigin);
    };
    spawnManagerEnvironment.onUnitSpawned = [](ArmySession& session, const SpawnRequest& request, Character* character) -> bool
    {
        return g_armyRuntime.ConfigureSpawnedUnit(session, request, character);
    };
    spawnManagerEnvironment.logInfo = [](const std::string& message)
    {
        WriteInfoToTerminalAndDebug(message);
    };
    spawnManagerEnvironment.logError = [](const std::string& message)
    {
        WriteErrorToTerminalAndDebug(message);
    };
    spawnManagerEnvironment.traceDebug = [](const std::string& message)
    {
        DebugTrace(message);
    };
    g_spawnManager.SetEnvironment(spawnManagerEnvironment);

    ArmyRuntimeEnvironment armyRuntimeEnvironment;
    armyRuntimeEnvironment.isGameLoaded = []() { return IsArmyGameLoaded(); };
    armyRuntimeEnvironment.resolveLeader = []() -> Character*
    {
        return ResolveArmyLeaderCharacter();
    };
    armyRuntimeEnvironment.resolveCurrentPlayerPlatoon = []() -> Platoon*
    {
        return GetCurrentArmyPlayerPlatoon();
    };
    armyRuntimeEnvironment.resolveActivePlatoon = [](Platoon* platoon) -> ActivePlatoon*
    {
        return platoon != nullptr ? platoon->getActivePlatoon() : nullptr;
    };
    armyRuntimeEnvironment.resolvePlayerFaction = []() -> Faction*
    {
        return ResolveArmyPlayerFaction();
    };
    armyRuntimeEnvironment.resolveLeaderFaction = [](Character* leader) -> Faction*
    {
        Faction* resolvedOwner = nullptr;
        unsigned int ownerExceptionCode = 0;
        if (!TryReadArmyCharacterOwnerNoCrash(leader, resolvedOwner, ownerExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] resolveLeaderFaction : owner inaccessible, exception 0x%08X.",
                ownerExceptionCode);
            return static_cast<Faction*>(nullptr);
        }

        return resolvedOwner;
    };
    armyRuntimeEnvironment.getCharacterHandleId = [](Character* character) -> ArmyHandleId
    {
        ArmyHandleId handleId = 0;
        unsigned int handleExceptionCode = 0;
        if (!TryRegisterArmyHandleIdNoCrash(character, handleId, handleExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] getCharacterHandleId : exception structuree 0x%08X.",
                handleExceptionCode);
            return 0;
        }

        return handleId;
    };
    armyRuntimeEnvironment.getPlatoonHandleId = [](Platoon* platoon) -> ArmyHandleId
    {
        // La je garde une identite de platoon purement locale a la session.
        // Je n'ai pas besoin d'un hand serialisable ici, juste d'un identifiant
        // stable tant que le platoon reste vivant, ce qui evite une conversion
        // runtime dangereuse via hand(platoon) sur cette build.
        return MakeRuntimePointerIdentity(platoon);
    };
    armyRuntimeEnvironment.resolveCharacterHandleId = [](ArmyHandleId handleId) -> Character*
    {
        return ResolveArmyCharacterHandleId(handleId);
    };
    armyRuntimeEnvironment.isCharacterDead = [](Character* character) -> bool
    {
        return IsArmyCharacterDeadSafe(character);
    };
    armyRuntimeEnvironment.isCharacterUnconscious = [](Character* character) -> bool
    {
        return IsArmyCharacterUnconsciousSafe(character);
    };
    armyRuntimeEnvironment.getCharacterPosition = [](Character* character) -> SpawnPosition
    {
        return GetArmyCharacterPosition(character);
    };
    armyRuntimeEnvironment.applyFaction = [](Character* character, Faction* faction, ActivePlatoon* platoon)
    {
        ApplyArmyFaction(character, faction, platoon);
    };
    armyRuntimeEnvironment.renameCharacter = [](Character* character, const std::string& name)
    {
        RenameArmyCharacter(character, name);
    };
    armyRuntimeEnvironment.teleportCharacter = [](Character* character, const SpawnPosition& position)
    {
        TeleportArmyCharacter(character, position);
    };
    armyRuntimeEnvironment.setFollowTarget = [](Character* character, Character* leader)
    {
        SetArmyFollowTarget(character, leader);
    };
    armyRuntimeEnvironment.setEscortOrder = [](Character* character, ArmyEscortOrder order)
    {
        ApplyArmyEscortOrder(character, order);
    };
    armyRuntimeEnvironment.setEscortRole = [](Character* character)
    {
        SetArmyEscortRole(character);
    };
    armyRuntimeEnvironment.rethinkAi = [](Character* character)
    {
        RethinkArmyAi(character);
    };
    armyRuntimeEnvironment.logInfo = [](const std::string& message)
    {
        WriteInfoToTerminalAndDebug(message);
    };
    armyRuntimeEnvironment.logError = [](const std::string& message)
    {
        WriteErrorToTerminalAndDebug(message);
    };
    armyRuntimeEnvironment.traceDebug = [](const std::string& message)
    {
        DebugTrace(message);
    };
    armyRuntimeEnvironment.resolveLeaderHandleId = []() -> ArmyHandleId
    {
        return ResolveArmyLeaderHandleId();
    };
    armyRuntimeEnvironment.dismissCharacter = [](Character* character)
    {
        unsigned int dismissExceptionCode = 0;
        if (!TryDismissArmyCharacterNoCrash(character, dismissExceptionCode))
        {
            DebugTraceFormat(
                "[TRACE] DismissArmyCharacter exception structuree | code=0x%08X | character=%p",
                dismissExceptionCode,
                static_cast<void*>(character));
        }
    };
    g_armyRuntime.SetEnvironment(armyRuntimeEnvironment);

    g_hookState.titleScreenConstructorInstalled =
        InstallArmyHookFromRva(
            "TitleScreen::_CONSTRUCTOR",
            DonJHookAddressResolver::kTitleScreenConstructorRva,
            TitleScreen_hook,
            &TitleScreen_orig,
            false);

    if (!g_hookState.titleScreenConstructorInstalled)
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook TitleScreen::_CONSTRUCTOR.");
    }

    g_hookState.titleScreenShowInstalled =
        InstallArmyHookFromRva(
            "TitleScreen::_NV_show",
            DonJHookAddressResolver::kTitleScreenShowRva,
            TitleScreen_show_hook,
            &TitleScreen_show_orig,
            false);

    if (!g_hookState.titleScreenShowInstalled)
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook TitleScreen::_NV_show.");
    }

    g_hookState.titleScreenUpdateInstalled =
        InstallArmyHookFromRva(
            "TitleScreen::_NV_update",
            DonJHookAddressResolver::kTitleScreenUpdateRva,
            TitleScreen_update_hook,
            &TitleScreen_update_orig,
            true);

    if (!g_hookState.titleScreenUpdateInstalled)
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook TitleScreen::_NV_update.");
        return;
    }

    g_hookState.guiWindowUpdateInstalled =
        InstallArmyHookFromRva(
            "GUIWindow::_NV_update",
            DonJHookAddressResolver::kGuiWindowUpdateRva,
            GUIWindow_update_hook,
            &GUIWindow_update_orig,
            false);

    if (!g_hookState.guiWindowUpdateInstalled)
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook GUIWindow::_NV_update.");
    }

    g_hookState.inputHandlerKeyDownInstalled =
        InstallArmyHookFromRva(
            "InputHandler::keyDownEvent",
            DonJHookAddressResolver::kInputHandlerKeyDownEventRva,
            InputHandler_keyDownEvent_hook,
            &InputHandler_keyDownEvent_orig,
            false);

    if (!g_hookState.inputHandlerKeyDownInstalled)
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook InputHandler::keyDownEvent.");
    }

    g_hookState.gameWorldMainLoopInstalled =
        InstallArmyHookFromRva(
            "GameWorld::_NV_mainLoop_GPUSensitiveStuff",
            DonJHookAddressResolver::kGameWorldMainLoopRva,
            GameWorld_mainLoop_hook,
            &GameWorld_mainLoop_orig,
            true);

    if (!g_hookState.gameWorldMainLoopInstalled)
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook GameWorld::_NV_mainLoop_GPUSensitiveStuff.");
        return;
    }

    g_hookState.createRandomCharacterInstalled =
        InstallArmyHookFromRva(
            "RootObjectFactory::createRandomCharacter",
            DonJHookAddressResolver::kCreateRandomCharacterRva,
            RootObjectFactory_createRandomCharacter_hook,
            &RootObjectFactory_createRandomCharacter_orig,
            false);

    if (!g_hookState.createRandomCharacterInstalled)
    {
        ErrorLog("DonJ Kenshi Hack : impossible d'installer le hook RootObjectFactory::createRandomCharacter.");
    }

    DebugTraceFormat(
        "DonJ Kenshi Hack : etat hooks | title_ctor=%s | title_show=%s | title_update=%s | gui_update=%s | input_key=%s | game_loop=%s | factory_create=%s",
        g_hookState.titleScreenConstructorInstalled ? "ok" : "ko",
        g_hookState.titleScreenShowInstalled ? "ok" : "ko",
        g_hookState.titleScreenUpdateInstalled ? "ok" : "ko",
        g_hookState.guiWindowUpdateInstalled ? "ok" : "ko",
        g_hookState.inputHandlerKeyDownInstalled ? "ok" : "ko",
        g_hookState.gameWorldMainLoopInstalled ? "ok" : "ko",
        g_hookState.createRandomCharacterInstalled ? "ok" : "ko");

    ResetTerminalUiBootstrapState("start plugin");
    DebugLog("DonJ Kenshi Hack : hooks TitleScreen et game tick installes.");
}
