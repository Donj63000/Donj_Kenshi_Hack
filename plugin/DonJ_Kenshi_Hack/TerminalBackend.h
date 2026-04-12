#pragma once

#include "ArmyCommandSpec.h"
#include "ArmySession.h"
#include "CommandRegistry.h"

#include <deque>
#include <functional>
#include <string>
#include <vector>

enum class GameplayCommandType
{
    SummonArmy
};

struct PendingCommand
{
    std::string rawLine;
};

struct GameplayCommand
{
    GameplayCommandType type = GameplayCommandType::SummonArmy;
    int requestedCount = 0;
    float durationSeconds = 0.0f;
    std::vector<std::string> templateNames;
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
    std::function<bool()> isGameLoaded = []() { return true; };
    std::function<bool()> hasResolvableLeader = []() { return true; };
    std::function<bool()> areArmyTemplatesAvailable = []() { return true; };
    std::function<bool()> isSpawnSystemReady = []() { return true; };
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
    ArmyPreflightCode EvaluateArmyPreflight() const;

    static std::string Trim(const std::string& value);
    static std::vector<std::string> Tokenize(const std::string& line);

    std::deque<std::string> outputHistory_;
    std::vector<std::string> commandHistory_;
    std::string historyDraft_;
    std::string inputBuffer_;
    std::size_t maxOutputLines_ = 64;
    std::size_t commandHistoryCursor_ = 0;
    bool commandHistoryBrowsing_ = false;
    bool inputActive_ = false;
    bool outputDirty_ = true;
    bool inputDirty_ = true;

    CommandRegistry commandRegistry_;
    ArmyCommandEnvironment armyEnvironment_;
    std::deque<PendingCommand> pendingCommands_;
    ArmySession armySession_;
    std::deque<GameplayCommand> gameplayCommandQueue_;
};
