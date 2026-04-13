#pragma once

#include "ArmyCommandSpec.h"
#include "ArmySession.h"
#include "CommandRegistry.h"

#include <deque>
#include <functional>
#include <string>
#include <vector>

struct GameplayCommandType
{
    enum Type
    {
        SummonArmy,
        DismissArmy
    };
};

struct PendingCommand
{
    std::string rawLine;

    PendingCommand()
    {
    }

    PendingCommand(const std::string& rawLineValue)
        : rawLine(rawLineValue)
    {
    }
};

struct GameplayCommand
{
    GameplayCommandType::Type type;
    int requestedCount;
    float durationSeconds;
    std::vector<std::string> templateNames;

    GameplayCommand()
        : type(GameplayCommandType::SummonArmy)
        , requestedCount(0)
        , durationSeconds(0.0f)
    {
    }
};

struct CommandContext
{
    std::function<void(const std::string&)> writeLine;
    std::function<void(const GameplayCommand&)> enqueueGameplayCommand;
    std::function<ArmySession&()> getArmySession;
    std::function<size_t()> getPendingGameplayCommandCount;
    std::function<bool()> isGameLoaded;
    std::function<bool()> hasResolvableLeader;
    std::function<bool()> areArmyTemplatesAvailable;
    std::function<bool()> isSpawnSystemReady;
};

struct ArmyCommandEnvironment
{
    std::function<bool()> isGameLoaded;
    std::function<bool()> hasResolvableLeader;
    std::function<bool()> areArmyTemplatesAvailable;
    std::function<bool()> isSpawnSystemReady;
    std::function<void(const std::string&)> debugTrace;
    std::function<bool()> isFactoryAvailable;
    std::function<bool()> isReplayHookInstalled;

    ArmyCommandEnvironment()
        : isGameLoaded([]() { return true; })
        , hasResolvableLeader([]() { return true; })
        , areArmyTemplatesAvailable([]() { return true; })
        , isSpawnSystemReady([]() { return true; })
        , debugTrace([](const std::string&) {})
        , isFactoryAvailable([]() { return true; })
        , isReplayHookInstalled([]() { return true; })
    {
    }

    ArmyCommandEnvironment(
        const std::function<bool()>& isGameLoadedValue,
        const std::function<bool()>& hasResolvableLeaderValue,
        const std::function<bool()>& areArmyTemplatesAvailableValue,
        const std::function<bool()>& isSpawnSystemReadyValue,
        const std::function<void(const std::string&)>& debugTraceValue = std::function<void(const std::string&)>(),
        const std::function<bool()>& isFactoryAvailableValue = std::function<bool()>(),
        const std::function<bool()>& isReplayHookInstalledValue = std::function<bool()>())
        : isGameLoaded(isGameLoadedValue ? isGameLoadedValue : std::function<bool()>([]() { return true; }))
        , hasResolvableLeader(hasResolvableLeaderValue ? hasResolvableLeaderValue : std::function<bool()>([]() { return true; }))
        , areArmyTemplatesAvailable(areArmyTemplatesAvailableValue ? areArmyTemplatesAvailableValue : std::function<bool()>([]() { return true; }))
        , isSpawnSystemReady(isSpawnSystemReadyValue ? isSpawnSystemReadyValue : std::function<bool()>([]() { return true; }))
        , debugTrace(debugTraceValue ? debugTraceValue : std::function<void(const std::string&)>([](const std::string&) {}))
        , isFactoryAvailable(isFactoryAvailableValue ? isFactoryAvailableValue : std::function<bool()>([]() { return true; }))
        , isReplayHookInstalled(isReplayHookInstalledValue ? isReplayHookInstalledValue : std::function<bool()>([]() { return true; }))
    {
    }
};

class TerminalBackend
{
public:
    TerminalBackend();

    const CommandRegistry& GetCommandRegistry() const;
    const ArmySession& GetArmySession() const;
    ArmySession& GetArmySession();
    const ArmyCommandSpec& GetArmySpec() const;

    void ActivateInput();
    void CancelInput(bool keepBuffer = false);
    bool IsInputActive() const;

    const std::string& GetInputBuffer() const;
    void SetInputBuffer(const std::string& value);
    void AppendInputCharacter(char value);
    void BackspaceInput();
    void NavigateCommandHistory(int direction);
    bool SubmitCurrentInput();

    void AppendOutputLine(const std::string& line);
    std::string BuildOutputText() const;
    std::string BuildInputText() const;

    bool HasPendingCommands() const;
    size_t GetPendingCommandCount() const;
    size_t ProcessPendingCommands(size_t maxCommandsToProcess = 8);

    bool HasPendingGameplayCommands() const;
    size_t GetPendingGameplayCommandCount() const;
    const std::deque<GameplayCommand>& GetPendingGameplayCommands() const;
    void TickGameplay(float deltaSeconds);

    bool ConsumeOutputDirty();
    bool ConsumeInputDirty();
    void MarkUiDirty();
    void SetArmyCommandEnvironment(const ArmyCommandEnvironment& environment);

private:
    void RegisterBuiltinCommands();
    void ExecuteLine(const std::string& line);
    void QueuePendingCommand(const PendingCommand& command);
    void QueueGameplayCommand(const GameplayCommand& command);
    void ProcessGameplayCommand(const GameplayCommand& command);
    void TickArmySession(float deltaSeconds);
    ArmyPreflightCode::Type EvaluateArmyPreflight() const;
    void QueueArmyInvocation(const CommandContext& context, int requestedCount, float durationSeconds, bool testMode);

    static std::string Trim(const std::string& value);
    static std::vector<std::string> Tokenize(const std::string& line);
    static bool TryParsePositiveInt(const std::string& value, int& outValue);

    std::deque<std::string> outputHistory_;
    std::vector<std::string> commandHistory_;
    std::string historyDraft_;
    std::string inputBuffer_;
    std::size_t maxOutputLines_;
    std::size_t commandHistoryCursor_;
    bool commandHistoryBrowsing_;
    bool inputActive_;
    bool outputDirty_;
    bool inputDirty_;

    CommandRegistry commandRegistry_;
    ArmyCommandEnvironment armyEnvironment_;
    std::deque<PendingCommand> pendingCommands_;
    ArmySession armySession_;
    std::deque<GameplayCommand> gameplayCommandQueue_;
};
