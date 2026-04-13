#include "ArmyDiagnostics.h"
#include "CommandRegistry.h"
#include "HookAddressResolver.h"
#include "ArmyRuntimeManager.h"
#include "RuntimeIdentity.h"
#include "SpawnManager.h"
#include "TerminalUiBootstrap.h"
#include "TerminalBackend.h"

#include <cassert>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    void TestBuiltinCommandsExist()
    {
        TerminalBackend backend;

        assert(backend.GetCommandRegistry().Exists("help"));
        assert(backend.GetCommandRegistry().Exists("/status"));
        assert(backend.GetCommandRegistry().Exists("army"));
    }

    void TestArmySpecValues()
    {
        TerminalBackend backend;
        const ArmyCommandSpec& spec = backend.GetArmySpec();

        assert(std::string(spec.commandName) == "/army");
        assert(std::string(spec.displayName) == "Invocation de l'armee des morts");
        assert(spec.unitCount == 30);
        assert(static_cast<int>(spec.durationSeconds) == 180);
        assert(spec.singleArmyAtATime);
        assert(std::string(spec.templateNames[0]) == "DonJ_ArmyOfDead_Warrior_A");
    }

    void TestHelpCommandWritesHistory()
    {
        TerminalBackend backend;

        backend.ActivateInput();
        backend.SetInputBuffer("/help");
        const bool submitted = backend.SubmitCurrentInput();
        assert(backend.HasPendingCommands());
        backend.ProcessPendingCommands();
        const std::string output = backend.BuildOutputText();

        assert(submitted);
        assert(output.find("Commandes disponibles") != std::string::npos);
        assert(output.find("/status") != std::string::npos);
        assert(output.find("/army") != std::string::npos);
    }

    void TestUnknownCommandIsRejected()
    {
        TerminalBackend backend;

        backend.SetInputBuffer("/introuvable");
        backend.SubmitCurrentInput();
        backend.ProcessPendingCommands();

        const std::string output = backend.BuildOutputText();
        assert(output.find("Commande inconnue") != std::string::npos);
    }

    void TestHookAddressResolverBuildsAbsoluteAddressFromRva()
    {
        const std::uintptr_t moduleBase = 0x140000000ull;
        const std::uintptr_t resolved = DonJHookAddressResolver::ResolveModuleRva(
            moduleBase,
            DonJHookAddressResolver::kTitleScreenConstructorRva);

        assert(resolved == (moduleBase + DonJHookAddressResolver::kTitleScreenConstructorRva));
        assert(DonJHookAddressResolver::ResolveModuleRva(0, DonJHookAddressResolver::kTitleScreenUpdateRva) == 0);
        assert(DonJHookAddressResolver::ResolveModuleRva(moduleBase, 0) == 0);
    }

    void TestTerminalUiBootstrapAttemptsCreationFromTitleScreenMenu()
    {
        const DonJTerminalUiBootstrap::Decision decision = DonJTerminalUiBootstrap::Evaluate(
            DonJTerminalUiBootstrap::Context(true, true, false, false),
            DonJTerminalUiBootstrap::NotCreated);

        assert(decision == DonJTerminalUiBootstrap::AttemptCreate);
    }

    void TestTerminalUiBootstrapSkipsCreationWhenWindowAlreadyExists()
    {
        const DonJTerminalUiBootstrap::Decision decision = DonJTerminalUiBootstrap::Evaluate(
            DonJTerminalUiBootstrap::Context(true, true, true, false),
            DonJTerminalUiBootstrap::NotCreated);

        assert(decision == DonJTerminalUiBootstrap::WindowAlreadyPresent);
    }

    void TestTerminalUiBootstrapSkipsCreationDuringGameplay()
    {
        const DonJTerminalUiBootstrap::Decision decision = DonJTerminalUiBootstrap::Evaluate(
            DonJTerminalUiBootstrap::Context(true, true, false, true),
            DonJTerminalUiBootstrap::NotCreated);

        assert(decision == DonJTerminalUiBootstrap::Skip);
    }

    void TestTerminalUiBootstrapDetectsGuiUnavailable()
    {
        const DonJTerminalUiBootstrap::Decision decision = DonJTerminalUiBootstrap::Evaluate(
            DonJTerminalUiBootstrap::Context(true, false, false, false),
            DonJTerminalUiBootstrap::NotCreated);

        assert(decision == DonJTerminalUiBootstrap::GuiUnavailable);
    }

    void TestTerminalUiBootstrapSkipsAfterFailedAttempt()
    {
        const DonJTerminalUiBootstrap::Decision decision = DonJTerminalUiBootstrap::Evaluate(
            DonJTerminalUiBootstrap::Context(true, true, false, false),
            DonJTerminalUiBootstrap::Failed);

        assert(decision == DonJTerminalUiBootstrap::Skip);
    }

    void TestTerminalUiToggleCreatesAndShowsWindowOnF10InGameplay()
    {
        const DonJTerminalUiBootstrap::ToggleDecision decision =
            DonJTerminalUiBootstrap::EvaluateToggle(
                DonJTerminalUiBootstrap::ToggleContext(true, true, false, false, true));

        assert(decision == DonJTerminalUiBootstrap::CreateAndShowWindow);
    }

    void TestTerminalUiToggleShowsExistingWindowOnF10InGameplay()
    {
        const DonJTerminalUiBootstrap::ToggleDecision decision =
            DonJTerminalUiBootstrap::EvaluateToggle(
                DonJTerminalUiBootstrap::ToggleContext(true, true, true, false, true));

        assert(decision == DonJTerminalUiBootstrap::ShowExistingWindow);
    }

    void TestTerminalUiToggleHidesVisibleWindowOnF10InGameplay()
    {
        const DonJTerminalUiBootstrap::ToggleDecision decision =
            DonJTerminalUiBootstrap::EvaluateToggle(
                DonJTerminalUiBootstrap::ToggleContext(true, true, true, true, true));

        assert(decision == DonJTerminalUiBootstrap::HideWindow);
    }

    void TestTerminalUiToggleAlsoWorksFromMenuWhenGuiIsReady()
    {
        const DonJTerminalUiBootstrap::ToggleDecision decision =
            DonJTerminalUiBootstrap::EvaluateToggle(
                DonJTerminalUiBootstrap::ToggleContext(true, true, true, false, false));

        assert(decision == DonJTerminalUiBootstrap::ShowExistingWindow);
    }

    void TestTerminalUiToggleIgnoresMenuWhenGuiIsUnavailable()
    {
        const DonJTerminalUiBootstrap::ToggleDecision decision =
            DonJTerminalUiBootstrap::EvaluateToggle(
                DonJTerminalUiBootstrap::ToggleContext(true, false, false, false, false));

        assert(decision == DonJTerminalUiBootstrap::IgnoreToggle);
    }

    void TestTerminalUiHiddenWindowDoesNotCaptureKeyboard()
    {
        assert(!DonJTerminalUiBootstrap::ShouldCaptureKeyboard(true, false));
        assert(!DonJTerminalUiBootstrap::ShouldCaptureKeyboard(false, false));
        assert(DonJTerminalUiBootstrap::ShouldCaptureKeyboard(true, true));
    }

    void TestClosingConsoleCancelsInputAndClearsBuffer()
    {
        TerminalBackend backend;

        backend.ActivateInput();
        backend.SetInputBuffer("/help");
        backend.CancelInput();

        assert(!backend.IsInputActive());
        assert(backend.GetInputBuffer().empty());
    }


    void TestArmyCommandPreparesSessionAndQueue()
    {
        TerminalBackend backend;

        backend.SetInputBuffer("/army");
        const bool submitted = backend.SubmitCurrentInput();
        assert(backend.HasPendingCommands());
        backend.ProcessPendingCommands();

        assert(submitted);
        assert(backend.HasPendingGameplayCommands());
        assert(backend.GetPendingGameplayCommands().size() == 1);
        assert(backend.GetArmySession().state == ArmyState::Preparing);
        assert(backend.GetArmySession().pendingRequests.size() == 30);
        assert(backend.GetArmySession().pendingRequestCount == 30);
        assert(backend.GetPendingGameplayCommands().front().templateNames.size() == 30);

        backend.TickGameplay(0.016f);
        assert(backend.GetArmySession().state == ArmyState::Spawning);
        assert(backend.GetArmySession().currentWaveTarget == 1);
    }

    void TestArmyTestCommandAcceptsReducedVolumes()
    {
        TerminalBackend backend;

        backend.SetInputBuffer("/armytest 3");
        const bool submitted = backend.SubmitCurrentInput();
        backend.ProcessPendingCommands();

        assert(submitted);
        assert(backend.HasPendingGameplayCommands());
        assert(backend.GetArmySession().state == ArmyState::Preparing);
        assert(backend.GetArmySession().requestedCount == 3);
        assert(backend.GetArmySession().pendingRequests.size() == 3);

        backend.TickGameplay(0.016f);
        assert(backend.GetArmySession().state == ArmyState::Spawning);
        assert(backend.GetArmySession().currentWaveTarget == 1);

        const std::string output = backend.BuildOutputText();
        assert(output.find("mode test") != std::string::npos);
    }

    void TestArmyRefusesWhenGameIsNotLoaded()
    {
        TerminalBackend backend;
        backend.SetArmyCommandEnvironment({
            []() { return false; },
            []() { return false; },
            []() { return true; },
            []() { return true; }
        });

        backend.SetInputBuffer("/army");
        backend.SubmitCurrentInput();
        backend.ProcessPendingCommands();

        const std::string output = backend.BuildOutputText();
        assert(output.find("[INFO] /army refusee : aucune partie chargee.") != std::string::npos);
    }

    void TestArmyRefusesWhenFactoryIsUnavailable()
    {
        TerminalBackend backend;
        backend.SetArmyCommandEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return true; },
            [](const std::string&) {},
            []() { return false; },
            []() { return true; }
        });

        backend.SetInputBuffer("/army");
        backend.SubmitCurrentInput();
        backend.ProcessPendingCommands();

        const std::string output = backend.BuildOutputText();
        assert(output.find("[INFO] /army refusee : factory Kenshi indisponible.") != std::string::npos);
    }

    void TestArmyRefusesWhenReplayHookIsUnavailable()
    {
        TerminalBackend backend;
        backend.SetArmyCommandEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return true; },
            [](const std::string&) {},
            []() { return true; },
            []() { return false; }
        });

        backend.SetInputBuffer("/army");
        backend.SubmitCurrentInput();
        backend.ProcessPendingCommands();

        const std::string output = backend.BuildOutputText();
        assert(output.find("[INFO] /army refusee : hook de replay Kenshi indisponible.") != std::string::npos);
    }

    void TestCommandHistoryNavigation()
    {
        TerminalBackend backend;

        backend.SetInputBuffer("/help");
        backend.SubmitCurrentInput();
        backend.SetInputBuffer("/status");
        backend.SubmitCurrentInput();

        backend.NavigateCommandHistory(-1);
        assert(backend.GetInputBuffer() == "/status");

        backend.NavigateCommandHistory(-1);
        assert(backend.GetInputBuffer() == "/help");

        backend.NavigateCommandHistory(1);
        assert(backend.GetInputBuffer() == "/status");

        backend.NavigateCommandHistory(1);
        assert(backend.GetInputBuffer().empty());
    }

    void TestSubmitQueuesWithoutImmediateExecution()
    {
        TerminalBackend backend;

        backend.SetInputBuffer("/status");
        const bool submitted = backend.SubmitCurrentInput();
        const std::string outputBeforeTick = backend.BuildOutputText();

        assert(submitted);
        assert(backend.GetPendingCommandCount() == 1);
        assert(outputBeforeTick.find("[INFO] /army : etat=") == std::string::npos);

        backend.ProcessPendingCommands();
        const std::string outputAfterTick = backend.BuildOutputText();
        assert(outputAfterTick.find("[INFO] /army : etat=") != std::string::npos);
    }

    void TestTerminalTraceMilestonesRemainVisible()
    {
        TerminalBackend backend;
        std::vector<std::string> traces;

        backend.SetArmyCommandEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return true; },
            [&traces](const std::string& line)
            {
                traces.push_back(line);
            }
        });

        backend.SetInputBuffer("/armytest 1");
        const bool submitted = backend.SubmitCurrentInput();
        assert(submitted);

        backend.ProcessPendingCommands();
        backend.TickGameplay(0.016f);

        auto containsTrace = [&traces](const char* needle)
        {
            for (const std::string& line : traces)
            {
                if (line.find(needle) != std::string::npos)
                {
                    return true;
                }
            }

            return false;
        };

        assert(containsTrace("input_submit_called"));
        assert(containsTrace("input_submit_success"));
        assert(containsTrace("ui_command_dequeued"));
        assert(containsTrace("gameplay_command_enqueued"));
        assert(containsTrace("gameplay_command_processing"));
    }

    void TestSpawnManagerWaitsForReplayHook()
    {
        SpawnManager manager;
        std::vector<std::string> infoLines;
        std::vector<std::string> errorLines;

        manager.SetEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return false; },
            []() { return false; },
            [](const std::string&) -> void* { return reinterpret_cast<void*>(0x1010); },
            []() -> Faction* { return reinterpret_cast<Faction*>(0x2020); },
            [](SpawnPosition& position) -> bool
            {
                position = { 1.0f, 2.0f, 3.0f };
                return true;
            },
            {},
            {},
            [&infoLines](const std::string& line) { infoLines.push_back(line); },
            [&errorLines](const std::string& line) { errorLines.push_back(line); }
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.requestedCount = 30;
        for (int index = 0; index < 30; ++index)
        {
            session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", index });
        }

        manager.Tick(session, 0.016f);
        manager.Tick(session, 5.1f);

        assert(manager.HasPendingRequests());
        assert(session.pendingRequestCount == 30);
        assert(session.currentWaveTarget == 1);
        assert(session.waitingForReplayOpportunity);
        assert(session.spawnedCount == 0);
        assert(errorLines.empty());

        bool foundHint = false;
        for (const std::string& line : infoLines)
        {
            if (line.find("zone peuplee") != std::string::npos)
            {
                foundHint = true;
                break;
            }
        }
        assert(foundHint);
    }


    void TestSpawnManagerWaitsForObservedFactoryOpportunity()
    {
        SpawnManager manager;
        std::vector<std::string> infoLines;
        int templateResolveCalls = 0;

        manager.SetEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return false; },
            [&templateResolveCalls](const std::string&) -> void*
            {
                ++templateResolveCalls;
                return reinterpret_cast<void*>(0x1010);
            },
            []() -> Faction* { return reinterpret_cast<Faction*>(0x2020); },
            [](SpawnPosition& position) -> bool
            {
                position = { 2.0f, 3.0f, 4.0f };
                return true;
            },
            {},
            {},
            [&infoLines](const std::string& line) { infoLines.push_back(line); },
            [](const std::string&) {}
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.requestedCount = 30;
        session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", 0 });

        manager.Tick(session, 0.016f);
        manager.Tick(session, 5.1f);

        assert(session.waitingForReplayOpportunity);
        assert(session.spawnedCount == 0);
        assert(session.pendingRequestCount == 1);
        assert(templateResolveCalls == 0);

        bool foundHint = false;
        for (const std::string& line : infoLines)
        {
            if (line.find("replay Kenshi") != std::string::npos)
            {
                foundHint = true;
                break;
            }
        }
        assert(foundHint);
    }

    void TestSpawnManagerAllowsLeaderBootstrapWhenPlayerFactionIsUnavailable()
    {
        SpawnManager manager;

        manager.SetEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return true; },
            [](const std::string&) -> void* { return reinterpret_cast<void*>(0x1010); },
            []() -> Faction* { return static_cast<Faction*>(nullptr); },
            [](SpawnPosition& position) -> bool
            {
                position = { 2.0f, 3.0f, 4.0f };
                return true;
            },
            [](const SpawnRequest&, void*, Faction*, const SpawnPosition&) -> SpawnAttemptResult
            {
                SpawnAttemptResult result;
                result.outcome = SpawnAttemptOutcome::Spawned;
                result.character = reinterpret_cast<Character*>(0x3030);
                return result;
            },
            {},
            [](const std::string&) {},
            [](const std::string&) {}
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.requestedCount = 1;
        session.durationSeconds = 180.0f;
        session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", 0 });

        manager.Tick(session, 0.016f);

        assert(session.spawnedCount == 1);
        assert(session.pendingRequestCount == 0);
        assert(session.state == ArmyState::Active);
        assert(session.active);
    }

    void TestSpawnManagerPurgesInternalQueueWhenSessionReturnsIdle()
    {
        SpawnManager manager;

        manager.SetEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return false; },
            [](const std::string&) -> void* { return reinterpret_cast<void*>(0x1010); },
            []() -> Faction* { return reinterpret_cast<Faction*>(0x2020); },
            [](SpawnPosition& position) -> bool
            {
                position = { 0.0f, 0.0f, 0.0f };
                return true;
            },
            {},
            {},
            [](const std::string&) {},
            [](const std::string&) {}
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", 0 });

        manager.Tick(session, 0.016f);
        assert(manager.HasPendingRequests());

        session.state = ArmyState::Idle;
        manager.Tick(session, 0.016f);

        assert(!manager.HasPendingRequests());
        assert(session.pendingRequestCount == 0);
    }

    void TestSpawnManagerPurgesInternalQueueWhenSessionStartsDismissing()
    {
        SpawnManager manager;

        manager.SetEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return false; },
            [](const std::string&) -> void* { return reinterpret_cast<void*>(0x1010); },
            []() -> Faction* { return reinterpret_cast<Faction*>(0x2020); },
            [](SpawnPosition& position) -> bool
            {
                position = { 0.0f, 0.0f, 0.0f };
                return true;
            },
            {},
            {},
            [](const std::string&) {},
            [](const std::string&) {}
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", 0 });

        manager.Tick(session, 0.016f);
        assert(manager.HasPendingRequests());

        session.state = ArmyState::Dismissing;
        manager.Tick(session, 0.016f);

        assert(!manager.HasPendingRequests());
        assert(session.pendingRequestCount == 0);
    }

    void TestSpawnManagerAbandonsSessionWhenAttemptBudgetIsExhausted()
    {
        SpawnManager manager;
        SpawnManagerConfig config;
        config.maxTotalAttempts = 3;
        manager.SetConfig(config);

        std::vector<std::string> errorLines;
        manager.SetEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return false; },
            [](const std::string&) -> void* { return reinterpret_cast<void*>(0x1010); },
            []() -> Faction* { return reinterpret_cast<Faction*>(0x2020); },
            [](SpawnPosition& position) -> bool
            {
                position = { 0.0f, 0.0f, 0.0f };
                return true;
            },
            {},
            {},
            [](const std::string&) {},
            [&errorLines](const std::string& line)
            {
                errorLines.push_back(line);
            }
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.requestedCount = 10;
        session.totalSpawnAttempts = 3;
        session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", 0 });

        manager.Tick(session, 0.016f);

        assert(session.state == ArmyState::Dismissing);
        assert(!session.active);
        assert(session.pendingRequestCount == 0);
        assert(session.pendingRequests.empty());

        bool foundBudgetError = false;
        for (const std::string& line : errorLines)
        {
            if (line.find("budget maximal de tentatives") != std::string::npos)
            {
                foundBudgetError = true;
                break;
            }
        }
        assert(foundBudgetError);
    }

    void TestSpawnManagerAdvancesValidationWaves()
    {
        SpawnManager manager;
        SpawnManagerConfig config;
        config.maxAttemptsPerTick = 64;
        manager.SetConfig(config);

        manager.SetEnvironment({
            []() { return true; },
            []() { return true; },
            []() { return true; },
            []() { return true; },
            [](const std::string&) -> void* { return reinterpret_cast<void*>(0x1010); },
            []() -> Faction* { return reinterpret_cast<Faction*>(0x2020); },
            [](SpawnPosition& position) -> bool
            {
                position = { 4.0f, 5.0f, 6.0f };
                return true;
            },
            [](const SpawnRequest&, void*, Faction*, const SpawnPosition&) -> SpawnAttemptResult
            {
                SpawnAttemptResult result;
                result.outcome = SpawnAttemptOutcome::Spawned;
                result.character = reinterpret_cast<Character*>(0x3030);
                return result;
            },
            {},
            [](const std::string&) {},
            [](const std::string&) {}
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.requestedCount = 30;
        session.durationSeconds = 180.0f;
        session.remainingSeconds = 180.0f;
        for (int index = 0; index < 30; ++index)
        {
            session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", index });
        }

        manager.Tick(session, 0.016f);
        assert(session.spawnedCount == 1);
        assert(session.currentWaveTarget == 3);

        manager.Tick(session, 0.016f);
        assert(session.spawnedCount == 3);
        assert(session.currentWaveTarget == 10);

        manager.Tick(session, 0.016f);
        assert(session.spawnedCount == 10);
        assert(session.currentWaveTarget == 30);

        manager.Tick(session, 0.016f);
        assert(session.spawnedCount == 30);
        assert(session.pendingRequestCount == 0);
        assert(session.state == ArmyState::Active);
        assert(session.active);
        assert(session.activeUnits.empty());
        assert(session.pendingFinalizeUnits.empty());
        assert(session.totalSpawnAttempts == 30);
        assert(session.failedSpawnAttempts == 0);
    }


    void TestArmyRuntimeManagerBootstrapsFactionAndTracksEscort()
    {
        ArmyRuntimeManager manager;

        Character* leader = reinterpret_cast<Character*>(0x1010);
        Character* minion = reinterpret_cast<Character*>(0x2020);
        Platoon* playerPlatoon = reinterpret_cast<Platoon*>(0x3030);
        ActivePlatoon* activePlatoon = reinterpret_cast<ActivePlatoon*>(0x4040);
        Faction* leaderFaction = reinterpret_cast<Faction*>(0x5050);

        std::unordered_map<ArmyHandleId, Character*> charactersById = {
            { 11, leader },
            { 22, minion }
        };

        Faction* appliedFaction = nullptr;
        ActivePlatoon* appliedPlatoon = nullptr;
        SpawnPosition teleportedPosition;
        Character* followLeader = nullptr;
        std::string renamedCharacter;
        std::vector<ArmyEscortOrder> orders;
        int roleAssignments = 0;
        int rethinkCalls = 0;

        manager.SetEnvironment({
            []() { return true; },
            [leader]() { return leader; },
            [playerPlatoon]() { return playerPlatoon; },
            [activePlatoon](Platoon*) { return activePlatoon; },
            []() { return static_cast<Faction*>(nullptr); },
            [leaderFaction](Character*) { return leaderFaction; },
            [leader, minion](Character* character) -> ArmyHandleId
            {
                if (character == leader)
                {
                    return 11;
                }

                if (character == minion)
                {
                    return 22;
                }

                return 0;
            },
            [playerPlatoon](Platoon* platoon) -> ArmyHandleId
            {
                return platoon == playerPlatoon ? 101 : 0;
            },
            [&charactersById](ArmyHandleId handleId) -> Character*
            {
                const auto it = charactersById.find(handleId);
                return it != charactersById.end() ? it->second : nullptr;
            },
            [](Character*) { return false; },
            [](Character*) { return false; },
            [leader, minion, &teleportedPosition](Character* character) -> SpawnPosition
            {
                if (character == leader)
                {
                    return { 10.0f, 0.0f, 20.0f };
                }

                if (character == minion)
                {
                    return teleportedPosition;
                }

                return { 0.0f, 0.0f, 0.0f };
            },
            [&appliedFaction, &appliedPlatoon](Character*, Faction* faction, ActivePlatoon* platoon)
            {
                appliedFaction = faction;
                appliedPlatoon = platoon;
            },
            [&renamedCharacter](Character*, const std::string& name)
            {
                renamedCharacter = name;
            },
            [&teleportedPosition](Character*, const SpawnPosition& position)
            {
                teleportedPosition = position;
            },
            [&followLeader](Character*, Character* leaderCharacter)
            {
                followLeader = leaderCharacter;
            },
            [&orders](Character*, ArmyEscortOrder order)
            {
                orders.push_back(order);
            },
            [&roleAssignments](Character*)
            {
                ++roleAssignments;
            },
            [&rethinkCalls](Character*)
            {
                ++rethinkCalls;
            },
            [](const std::string&) {},
            [](const std::string&) {}
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        SpawnRequest request;
        request.templateName = "DonJ_ArmyOfDead_Warrior_A";
        request.index = 0;

        const bool configured = manager.ConfigureSpawnedUnit(session, request, minion);
        assert(configured);
        assert(session.leaderHandleId == 11);
        assert(session.leaderPlatoonHandleId == 101);
        assert(!session.factionBootstrappedFromLeader);
        assert(session.pendingFinalizeUnits.size() == 1);
        assert(session.pendingFinalizeUnits.front().handleId == 22);
        assert(session.activeUnitHandleIds.empty());
        assert(appliedFaction == nullptr);
        assert(appliedPlatoon == nullptr);
        assert(followLeader == nullptr);
        assert(orders.empty());
        assert(roleAssignments == 0);
        assert(rethinkCalls == 0);
        assert(renamedCharacter.empty());

        manager.Tick(session, 0.10f);
        assert(session.pendingFinalizeUnits.empty());
        assert(session.factionBootstrappedFromLeader);
        assert(session.activeUnitHandleIds.size() == 1);
        assert(session.activeUnitHandleIds.front() == 22);
        assert(appliedFaction == leaderFaction);
        assert(appliedPlatoon == activePlatoon);
        assert(!renamedCharacter.empty());

        const float dx = teleportedPosition.x - 10.0f;
        const float dz = teleportedPosition.z - 20.0f;
        const float distance = std::sqrt((dx * dx) + (dz * dz));
        assert(distance >= 1.9f);
        assert(distance <= 8.1f);

        manager.Tick(session, 0.60f);
        assert(followLeader == leader);
        assert(orders.size() == 2);
        assert(roleAssignments == 1);
        assert(rethinkCalls == 1);
    }

    void TestArmyRuntimeManagerDismissesWhenLeaderContextChanges()
    {
        ArmyRuntimeManager manager;

        Character* leader = reinterpret_cast<Character*>(0x1110);
        Character* minion = reinterpret_cast<Character*>(0x2220);
        Platoon* platoonA = reinterpret_cast<Platoon*>(0x3330);
        Platoon* platoonB = reinterpret_cast<Platoon*>(0x4440);

        std::unordered_map<ArmyHandleId, Character*> charactersById = {
            { 77, leader },
            { 88, minion }
        };

        Platoon* currentPlatoon = platoonA;

        manager.SetEnvironment({
            []() { return true; },
            [leader]() { return leader; },
            [&currentPlatoon]() { return currentPlatoon; },
            [](Platoon*) { return static_cast<ActivePlatoon*>(nullptr); },
            []() { return static_cast<Faction*>(nullptr); },
            [](Character*) { return static_cast<Faction*>(nullptr); },
            [leader, minion](Character* character) -> ArmyHandleId
            {
                if (character == leader)
                {
                    return 77;
                }

                if (character == minion)
                {
                    return 88;
                }

                return 0;
            },
            [platoonA, platoonB](Platoon* platoon) -> ArmyHandleId
            {
                if (platoon == platoonA)
                {
                    return 701;
                }

                if (platoon == platoonB)
                {
                    return 702;
                }

                return 0;
            },
            [&charactersById](ArmyHandleId handleId) -> Character*
            {
                const auto it = charactersById.find(handleId);
                return it != charactersById.end() ? it->second : nullptr;
            },
            [](Character*) { return false; },
            [](Character*) { return false; },
            [](Character*) -> SpawnPosition
            {
                return { 0.0f, 0.0f, 0.0f };
            },
            [](Character*, Faction*, ActivePlatoon*) {},
            {},
            [](Character*, const SpawnPosition&) {},
            [](Character*, Character*) {},
            [](Character*, ArmyEscortOrder) {},
            {},
            {},
            [](const std::string&) {},
            [](const std::string&) {}
        });

        ArmySession session;
        session.state = ArmyState::Active;
        session.active = true;
        session.spawnedCount = 2;
        session.pendingRequestCount = 0;
        session.activeUnitHandleIds.push_back(88);

        const bool captured = manager.CaptureLeaderContext(session);
        assert(captured);
        assert(session.leaderHandleId == 77);
        assert(session.leaderPlatoonHandleId == 701);

        manager.Tick(session, 0.6f);
        assert(session.state == ArmyState::Active);

        currentPlatoon = platoonB;
        manager.Tick(session, 0.1f);
        assert(session.state == ArmyState::Idle);
        assert(!session.active);
        assert(session.leaderHandleId == 0);
        assert(session.activeUnitHandleIds.empty());
    }

    void TestArmyRuntimeManagerComputesFormationRings()
    {
        ArmyRuntimeManager manager;

        const SpawnPosition leaderPosition = { 100.0f, 5.0f, 200.0f };
        const SpawnPosition slot0 = manager.ComputeFormationPosition(leaderPosition, 0);
        const SpawnPosition slot5 = manager.ComputeFormationPosition(leaderPosition, 5);
        const SpawnPosition slot6 = manager.ComputeFormationPosition(leaderPosition, 6);
        const SpawnPosition slot29 = manager.ComputeFormationPosition(leaderPosition, 29);

        const auto distanceFromLeader = [&leaderPosition](const SpawnPosition& position)
        {
            const float dx = position.x - leaderPosition.x;
            const float dz = position.z - leaderPosition.z;
            return std::sqrt((dx * dx) + (dz * dz));
        };

        assert(distanceFromLeader(slot0) >= 1.9f);
        assert(distanceFromLeader(slot0) <= 2.1f);
        assert(distanceFromLeader(slot5) >= 1.9f);
        assert(distanceFromLeader(slot5) <= 2.1f);
        assert(distanceFromLeader(slot6) >= 3.9f);
        assert(distanceFromLeader(slot6) <= 4.1f);
        assert(distanceFromLeader(slot29) >= 5.9f);
        assert(distanceFromLeader(slot29) <= 6.1f);
    }

    void TestArmyRuntimeManagerWaitsPassivelyWhileNoUnitIsMaterialised()
    {
        ArmyRuntimeManager manager;

        int resolveLeaderCalls = 0;
        int resolveLeaderHandleCalls = 0;

        manager.SetEnvironment({
            []() { return true; },
            [&resolveLeaderCalls]() -> Character*
            {
                ++resolveLeaderCalls;
                return reinterpret_cast<Character*>(0x1110);
            },
            []() { return static_cast<Platoon*>(nullptr); },
            [](Platoon*) { return static_cast<ActivePlatoon*>(nullptr); },
            []() { return static_cast<Faction*>(nullptr); },
            [](Character*) { return static_cast<Faction*>(nullptr); },
            [](Character*) -> ArmyHandleId { return 0; },
            [](Platoon*) -> ArmyHandleId { return 0; },
            [](ArmyHandleId) { return static_cast<Character*>(nullptr); },
            [](Character*) { return false; },
            [](Character*) { return false; },
            [](Character*) { return SpawnPosition(); },
            [](Character*, Faction*, ActivePlatoon*) {},
            {},
            [](Character*, const SpawnPosition&) {},
            [](Character*, Character*) {},
            [](Character*, ArmyEscortOrder) {},
            {},
            {},
            [](const std::string&) {},
            [](const std::string&) {},
            [](const std::string&) {},
            [&resolveLeaderHandleCalls]() -> ArmyHandleId
            {
                ++resolveLeaderHandleCalls;
                return 77;
            }
        });

        ArmySession session;
        session.state = ArmyState::Spawning;
        session.requestedCount = 30;
        session.pendingRequestCount = 30;
        session.currentWaveTarget = 1;
        session.spawnedCount = 0;

        manager.Tick(session, 0.016f);

        assert(session.state == ArmyState::Spawning);
        assert(resolveLeaderCalls == 0);
        assert(resolveLeaderHandleCalls == 0);
    }

    void TestArmyRuntimeManagerPrefersDedicatedLeaderHandleResolver()
    {
        ArmyRuntimeManager manager;

        Character* leader = reinterpret_cast<Character*>(0x1010);
        Platoon* playerPlatoon = reinterpret_cast<Platoon*>(0x3030);
        int characterHandleCalls = 0;

        manager.SetEnvironment({
            []() { return true; },
            [leader]() { return leader; },
            [playerPlatoon]() { return playerPlatoon; },
            [](Platoon*) { return static_cast<ActivePlatoon*>(nullptr); },
            []() { return static_cast<Faction*>(nullptr); },
            [](Character*) { return static_cast<Faction*>(nullptr); },
            [&characterHandleCalls](Character*) -> ArmyHandleId
            {
                ++characterHandleCalls;
                return 9999;
            },
            [playerPlatoon](Platoon* platoon) -> ArmyHandleId
            {
                return platoon == playerPlatoon ? 5678 : 0;
            },
            [](ArmyHandleId) { return static_cast<Character*>(nullptr); },
            [](Character*) { return false; },
            [](Character*) { return false; },
            [](Character*) { return SpawnPosition(); },
            [](Character*, Faction*, ActivePlatoon*) {},
            {},
            [](Character*, const SpawnPosition&) {},
            [](Character*, Character*) {},
            [](Character*, ArmyEscortOrder) {},
            {},
            {},
            [](const std::string&) {},
            [](const std::string&) {},
            [](const std::string&) {},
            []() -> ArmyHandleId { return 1234; }
        });

        ArmySession session;
        const bool captured = manager.CaptureLeaderContext(session);

        assert(captured);
        assert(session.leaderHandleId == 1234);
        assert(session.leaderPlatoonHandleId == 5678);
        assert(characterHandleCalls == 0);
    }

    void TestArmyTimerTransitionsToDismissingOnGameplayTick()
    {
        TerminalBackend backend;
        ArmySession& session = backend.GetArmySession();

        session.state = ArmyState::Active;
        session.active = true;
        session.requestedCount = 30;
        session.spawnedCount = 30;
        session.durationSeconds = 180.0f;
        session.remainingSeconds = 0.25f;
        session.pendingRequestCount = 0;

        backend.TickGameplay(0.30f);

        assert(session.state == ArmyState::Dismissing);
        assert(!session.active);
        assert(session.remainingSeconds == 0.0f);

        const std::string output = backend.BuildOutputText();
        assert(output.find("[OK] Fin d'invocation : nettoyage effectue.") != std::string::npos);
    }


    void TestArmyRuntimeFinalizeDismissResetsSessionState()
    {
        ArmyRuntimeManager manager;
        int dismissCalls = 0;
        Character* dismissedCharacter = reinterpret_cast<Character*>(0x4040);

        manager.SetEnvironment({
            []() { return true; },
            []() { return static_cast<Character*>(nullptr); },
            []() { return static_cast<Platoon*>(nullptr); },
            [](Platoon*) { return static_cast<ActivePlatoon*>(nullptr); },
            []() { return static_cast<Faction*>(nullptr); },
            [](Character*) { return static_cast<Faction*>(nullptr); },
            [](Character*) -> ArmyHandleId { return 0; },
            [](Platoon*) -> ArmyHandleId { return 0; },
            [dismissedCharacter](ArmyHandleId handleId)
            {
                return handleId == 303 ? dismissedCharacter : static_cast<Character*>(nullptr);
            },
            [](Character*) { return false; },
            [](Character*) { return false; },
            [](Character*) { return SpawnPosition(); },
            [](Character*, Faction*, ActivePlatoon*) {},
            {},
            [](Character*, const SpawnPosition&) {},
            [](Character*, Character*) {},
            [](Character*, ArmyEscortOrder) {},
            {},
            {},
            [](const std::string&) {},
            [](const std::string&) {},
            [](const std::string&) {},
            []() -> ArmyHandleId { return 0; },
            [&dismissCalls](Character*)
            {
                ++dismissCalls;
            }
        });

        ArmySession session;
        session.state = ArmyState::Dismissing;
        session.requestedCount = 30;
        session.spawnedCount = 12;
        session.pendingRequestCount = 4;
        session.totalSpawnAttempts = 19;
        session.failedSpawnAttempts = 2;
        session.deferredSpawnAttempts = 7;
        session.currentWaveTarget = 10;
        session.durationSeconds = 180.0f;
        session.remainingSeconds = 0.0f;
        session.escortRefreshAccumulator = 0.42f;
        session.active = false;
        session.lockOneArmyAtATime = true;
        session.waitingForReplayOpportunity = true;
        session.factionBootstrappedFromLeader = true;
        session.leaderHandleId = 101;
        session.leaderPlatoonHandleId = 202;
        session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", 12 });
        session.pendingFinalizeUnits.push_back({ { "DonJ_ArmyOfDead_Warrior_A", 13 }, 304, 2 });
        session.activeUnitHandleIds.push_back(303);
        session.activeUnits.push_back(dismissedCharacter);

        manager.Tick(session, 0.016f);

        assert(dismissCalls == 1);
        assert(session.state == ArmyState::Idle);
        assert(session.requestedCount == 30);
        assert(session.spawnedCount == 0);
        assert(session.pendingRequestCount == 0);
        assert(session.totalSpawnAttempts == 0);
        assert(session.failedSpawnAttempts == 0);
        assert(session.deferredSpawnAttempts == 0);
        assert(session.currentWaveTarget == 0);
        assert(session.remainingSeconds == 0.0f);
        assert(session.escortRefreshAccumulator == 0.0f);
        assert(!session.active);
        assert(session.lockOneArmyAtATime);
        assert(!session.waitingForReplayOpportunity);
        assert(!session.factionBootstrappedFromLeader);
        assert(session.leaderHandleId == 0);
        assert(session.leaderPlatoonHandleId == 0);
        assert(session.pendingRequests.empty());
        assert(session.pendingFinalizeUnits.empty());
        assert(session.activeUnitHandleIds.empty());
        assert(session.activeUnits.empty());
    }


    void TestArmyCanBeReusedAfterCleanup()
    {
        TerminalBackend backend;
        ArmyRuntimeManager runtimeManager;

        runtimeManager.SetEnvironment({
            []() { return true; },
            []() { return static_cast<Character*>(nullptr); },
            []() { return static_cast<Platoon*>(nullptr); },
            [](Platoon*) { return static_cast<ActivePlatoon*>(nullptr); },
            []() { return static_cast<Faction*>(nullptr); },
            [](Character*) { return static_cast<Faction*>(nullptr); },
            [](Character*) -> ArmyHandleId { return 0; },
            [](Platoon*) -> ArmyHandleId { return 0; },
            [](ArmyHandleId) { return static_cast<Character*>(nullptr); },
            [](Character*) { return false; },
            [](Character*) { return false; },
            [](Character*) { return SpawnPosition(); },
            [](Character*, Faction*, ActivePlatoon*) {},
            {},
            [](Character*, const SpawnPosition&) {},
            [](Character*, Character*) {},
            [](Character*, ArmyEscortOrder) {},
            {},
            {},
            [](const std::string&) {},
            [](const std::string&) {},
            [](const std::string&) {},
            []() -> ArmyHandleId { return 0; },
            [](Character*) {}
        });

        ArmySession& session = backend.GetArmySession();
        session.state = ArmyState::Active;
        session.active = true;
        session.requestedCount = 30;
        session.spawnedCount = 30;
        session.pendingRequestCount = 0;
        session.durationSeconds = 180.0f;
        session.remainingSeconds = 0.10f;
        session.activeUnitHandleIds.push_back(9001);
        session.activeUnits.push_back(reinterpret_cast<Character*>(0x9002));

        backend.TickGameplay(0.20f);
        assert(session.state == ArmyState::Dismissing);

        runtimeManager.Tick(session, 0.016f);
        assert(session.state == ArmyState::Idle);
        assert(session.pendingRequests.empty());
        assert(session.activeUnitHandleIds.empty());
        assert(session.activeUnits.empty());

        backend.SetInputBuffer("/army");
        const bool submitted = backend.SubmitCurrentInput();
        assert(submitted);
        backend.ProcessPendingCommands();

        assert(backend.HasPendingGameplayCommands());
        assert(session.state == ArmyState::Preparing);
        assert(session.pendingRequests.size() == 30);
        assert(session.pendingRequestCount == 30);
    }

    void TestDismissCommandQueuesCleanup()
    {
        TerminalBackend backend;
        ArmySession& session = backend.GetArmySession();
        session.state = ArmyState::Active;
        session.active = true;
        session.requestedCount = 3;
        session.spawnedCount = 3;

        backend.SetInputBuffer("/dismiss");
        const bool submitted = backend.SubmitCurrentInput();
        assert(submitted);
        backend.ProcessPendingCommands();
        assert(backend.HasPendingGameplayCommands());

        backend.TickGameplay(0.016f);
        assert(session.state == ArmyState::Dismissing);
        assert(!session.active);
        assert(session.pendingRequestCount == 0);

        const std::string output = backend.BuildOutputText();
        assert(output.find("Dissolution forcee") != std::string::npos);
    }

    void TestRuntimePointerIdentityUsesPointerValueSafely()
    {
        assert(MakeRuntimePointerIdentity(nullptr) == 0);

        const int sentinel = 42;
        const ArmyHandleId pointerIdentity = MakeRuntimePointerIdentity(&sentinel);
        assert(pointerIdentity != 0);
        assert(pointerIdentity == static_cast<ArmyHandleId>(reinterpret_cast<std::uintptr_t>(&sentinel)));
    }

    void TestArmyDiagnosticsBuildReadableSnapshots()
    {
        ArmySession session;
        session.state = ArmyState::Spawning;
        session.requestedCount = 30;
        session.spawnedCount = 3;
        session.pendingRequestCount = 27;
        session.pendingRequests.push_back({ "DonJ_ArmyOfDead_Warrior_A", 3 });
        session.currentWaveTarget = 10;
        session.totalSpawnAttempts = 4;
        session.deferredSpawnAttempts = 1;
        session.failedSpawnAttempts = 0;
        session.waitingForReplayOpportunity = true;
        session.leaderHandleId = 77;
        session.leaderPlatoonHandleId = 701;

        const std::string sessionLine = BuildArmySessionDebugLine(session);
        const std::string requestLine = BuildSpawnRequestDebugLine(session.pendingRequests.front());
        const std::string positionLine = BuildSpawnPositionDebugLine({ 1.0f, 2.5f, 3.0f });

        assert(sessionLine.find("state=Spawning") != std::string::npos);
        assert(sessionLine.find("spawned=3") != std::string::npos);
        assert(sessionLine.find("waiting_replay=yes") != std::string::npos);
        assert(requestLine.find("DonJ_ArmyOfDead_Warrior_A") != std::string::npos);
        assert(requestLine.find("index=3") != std::string::npos);
        assert(positionLine.find("x=1.00") != std::string::npos);
        assert(std::string(ToString(SpawnAttemptOutcome::DeferredAwaitingReplayHook)) == "DeferredAwaitingReplayHook");
    }
}

int main()
{
    TestBuiltinCommandsExist();
    TestArmySpecValues();
    TestHelpCommandWritesHistory();
    TestUnknownCommandIsRejected();
    TestHookAddressResolverBuildsAbsoluteAddressFromRva();
    TestTerminalUiBootstrapAttemptsCreationFromTitleScreenMenu();
    TestTerminalUiBootstrapSkipsCreationWhenWindowAlreadyExists();
    TestTerminalUiBootstrapSkipsCreationDuringGameplay();
    TestTerminalUiBootstrapDetectsGuiUnavailable();
    TestTerminalUiBootstrapSkipsAfterFailedAttempt();
    TestTerminalUiToggleCreatesAndShowsWindowOnF10InGameplay();
    TestTerminalUiToggleShowsExistingWindowOnF10InGameplay();
    TestTerminalUiToggleHidesVisibleWindowOnF10InGameplay();
    TestTerminalUiToggleAlsoWorksFromMenuWhenGuiIsReady();
    TestTerminalUiToggleIgnoresMenuWhenGuiIsUnavailable();
    TestTerminalUiHiddenWindowDoesNotCaptureKeyboard();
    TestClosingConsoleCancelsInputAndClearsBuffer();
    TestArmyCommandPreparesSessionAndQueue();
    TestArmyTestCommandAcceptsReducedVolumes();
    TestArmyRefusesWhenGameIsNotLoaded();
    TestArmyRefusesWhenFactoryIsUnavailable();
    TestArmyRefusesWhenReplayHookIsUnavailable();
    TestCommandHistoryNavigation();
    TestSubmitQueuesWithoutImmediateExecution();
    TestTerminalTraceMilestonesRemainVisible();
    TestSpawnManagerWaitsForReplayHook();
    TestSpawnManagerWaitsForObservedFactoryOpportunity();
    TestSpawnManagerAllowsLeaderBootstrapWhenPlayerFactionIsUnavailable();
    TestSpawnManagerPurgesInternalQueueWhenSessionReturnsIdle();
    TestSpawnManagerPurgesInternalQueueWhenSessionStartsDismissing();
    TestSpawnManagerAbandonsSessionWhenAttemptBudgetIsExhausted();
    TestSpawnManagerAdvancesValidationWaves();
    TestArmyRuntimeManagerBootstrapsFactionAndTracksEscort();
    TestArmyRuntimeManagerDismissesWhenLeaderContextChanges();
    TestArmyRuntimeManagerComputesFormationRings();
    TestArmyRuntimeManagerWaitsPassivelyWhileNoUnitIsMaterialised();
    TestArmyRuntimeManagerPrefersDedicatedLeaderHandleResolver();
    TestArmyTimerTransitionsToDismissingOnGameplayTick();
    TestArmyRuntimeFinalizeDismissResetsSessionState();
    TestArmyCanBeReusedAfterCleanup();
    TestDismissCommandQueuesCleanup();
    TestRuntimePointerIdentityUsesPointerValueSafely();
    TestArmyDiagnosticsBuildReadableSnapshots();
    return 0;
}
