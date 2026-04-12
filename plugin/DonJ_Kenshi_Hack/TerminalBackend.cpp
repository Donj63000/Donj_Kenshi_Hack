#include "ArmyDiagnostics.h"
#include "TerminalBackend.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace
{
    std::string BuildArmyStatusLine(const ArmySession& session)
    {
        const ArmyCommandSpec& spec = GetArmyCommandSpec();
        std::ostringstream builder;
        builder
            << "[INFO] /army : etat=" << ToStatusLabel(session.state)
            << ", unites=" << session.spawnedCount << " / " << spec.unitCount
            << ", spawned=" << session.spawnedCount
            << ", pending=" << session.pendingRequestCount
            << ", active_units=" << session.activeUnits.size()
            << ", active_handles=" << session.activeUnitHandleIds.size()
            << ", wave=" << session.currentWaveTarget
            << ", tentatives=" << session.totalSpawnAttempts
            << ", differees=" << session.deferredSpawnAttempts
            << ", echecs=" << session.failedSpawnAttempts
            << ", leader=" << (session.leaderHandleId != 0 ? "verrouille" : "non_lie")
            << ", faction=" << (session.factionBootstrappedFromLeader ? "bootstrap_leader" : "player")
            << ", mode_spawn=" << (session.waitingForReplayOpportunity ? "attente_factory" : "pret")
            << ", temps restant=" << static_cast<int>(session.remainingSeconds) << "s.";
        return builder.str();
    }

    void TraceDebug(const ArmyCommandEnvironment& environment, const std::string& message)
    {
        if (environment.debugTrace)
        {
            environment.debugTrace(message);
        }
    }
}

TerminalBackend::TerminalBackend()
{
    RegisterBuiltinCommands();

    // La j'initialise le terminal avec un message clair pour verifier vite que le backend tourne.
    AppendOutputLine("DonJ Kenshi Hack : terminal pret.");
    AppendOutputLine("Tape /help pour voir les commandes disponibles.");
}

const CommandRegistry& TerminalBackend::GetCommandRegistry() const
{
    return commandRegistry_;
}

const ArmySession& TerminalBackend::GetArmySession() const
{
    return armySession_;
}

ArmySession& TerminalBackend::GetArmySession()
{
    return armySession_;
}

const ArmyCommandSpec& TerminalBackend::GetArmySpec() const
{
    return GetArmyCommandSpec();
}

void TerminalBackend::ActivateInput()
{
    inputActive_ = true;
    inputDirty_ = true;
}

void TerminalBackend::CancelInput(bool keepBuffer)
{
    inputActive_ = false;
    commandHistoryBrowsing_ = false;
    commandHistoryCursor_ = commandHistory_.size();
    historyDraft_.clear();

    if (!keepBuffer)
    {
        inputBuffer_.clear();
    }

    inputDirty_ = true;
}

bool TerminalBackend::IsInputActive() const
{
    return inputActive_;
}

const std::string& TerminalBackend::GetInputBuffer() const
{
    return inputBuffer_;
}

void TerminalBackend::SetInputBuffer(const std::string& value)
{
    inputBuffer_ = value;
    inputDirty_ = true;
}

void TerminalBackend::AppendInputCharacter(char value)
{
    inputBuffer_.push_back(value);
    inputDirty_ = true;
}

void TerminalBackend::BackspaceInput()
{
    if (!inputBuffer_.empty())
    {
        inputBuffer_.pop_back();
        inputDirty_ = true;
    }
}

void TerminalBackend::NavigateCommandHistory(int direction)
{
    if (commandHistory_.empty())
    {
        return;
    }

    if (!commandHistoryBrowsing_)
    {
        historyDraft_ = inputBuffer_;
        commandHistoryBrowsing_ = true;
        commandHistoryCursor_ = commandHistory_.size();
    }

    if (direction < 0)
    {
        if (commandHistoryCursor_ > 0)
        {
            --commandHistoryCursor_;
        }
    }
    else if (direction > 0)
    {
        if (commandHistoryCursor_ < commandHistory_.size())
        {
            ++commandHistoryCursor_;
        }
    }

    if (commandHistoryCursor_ >= commandHistory_.size())
    {
        commandHistoryBrowsing_ = false;
        inputBuffer_ = historyDraft_;
    }
    else
    {
        inputBuffer_ = commandHistory_[commandHistoryCursor_];
    }

    inputDirty_ = true;
}

bool TerminalBackend::SubmitCurrentInput()
{
    const std::string line = Trim(inputBuffer_);

    commandHistoryBrowsing_ = false;
    commandHistoryCursor_ = commandHistory_.size();
    historyDraft_.clear();
    inputBuffer_.clear();
    inputDirty_ = true;

    if (line.empty())
    {
        return false;
    }

    commandHistory_.push_back(line);
    commandHistoryCursor_ = commandHistory_.size();

    AppendOutputLine("> " + line);
    QueuePendingCommand({ line });
    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] TerminalBackend::SubmitCurrentInput | line=") +
            line +
            " | pending_ui=" +
            std::to_string(pendingCommands_.size()));
    return true;
}

void TerminalBackend::AppendOutputLine(const std::string& line)
{
    if (line.empty())
    {
        return;
    }

    outputHistory_.push_back(line);
    while (outputHistory_.size() > maxOutputLines_)
    {
        outputHistory_.pop_front();
    }

    outputDirty_ = true;
}

std::string TerminalBackend::BuildOutputText() const
{
    std::ostringstream builder;

    bool firstLine = true;
    for (const std::string& line : outputHistory_)
    {
        if (!firstLine)
        {
            builder << '\n';
        }

        builder << line;
        firstLine = false;
    }

    return builder.str();
}

std::string TerminalBackend::BuildInputText() const
{
    if (inputActive_)
    {
        return std::string("> ") + inputBuffer_ + "_";
    }

    if (!inputBuffer_.empty())
    {
        return std::string("> ") + inputBuffer_;
    }

    return "Appuie sur Entree pour ouvrir la saisie.";
}

bool TerminalBackend::HasPendingCommands() const
{
    return !pendingCommands_.empty();
}

size_t TerminalBackend::GetPendingCommandCount() const
{
    return pendingCommands_.size();
}

size_t TerminalBackend::ProcessPendingCommands(size_t maxCommandsToProcess)
{
    size_t processedCount = 0;

    while (!pendingCommands_.empty() && processedCount < maxCommandsToProcess)
    {
        const PendingCommand command = pendingCommands_.front();
        pendingCommands_.pop_front();
        ExecuteLine(command.rawLine);
        ++processedCount;
    }

    return processedCount;
}

bool TerminalBackend::HasPendingGameplayCommands() const
{
    return !gameplayCommandQueue_.empty();
}

size_t TerminalBackend::GetPendingGameplayCommandCount() const
{
    return gameplayCommandQueue_.size();
}

const std::deque<GameplayCommand>& TerminalBackend::GetPendingGameplayCommands() const
{
    return gameplayCommandQueue_;
}

void TerminalBackend::TickGameplay(float deltaSeconds)
{
    const float safeDeltaSeconds = std::max(deltaSeconds, 0.0f);

    if (!gameplayCommandQueue_.empty() || armySession_.state != ArmyState::Idle)
    {
        char traceBuffer[768] = {};
        std::snprintf(
            traceBuffer,
            sizeof(traceBuffer),
            "[TRACE] TerminalBackend::TickGameplay entree | dt=%.3f | pending_gameplay=%zu | %s",
            static_cast<double>(safeDeltaSeconds),
            gameplayCommandQueue_.size(),
            BuildArmySessionDebugLine(armySession_).c_str());
        TraceDebug(armyEnvironment_, traceBuffer);
    }

    while (!gameplayCommandQueue_.empty())
    {
        const GameplayCommand command = gameplayCommandQueue_.front();
        gameplayCommandQueue_.pop_front();
        ProcessGameplayCommand(command);
    }

    TickArmySession(safeDeltaSeconds);

    if (armySession_.state != ArmyState::Idle)
    {
        TraceDebug(
            armyEnvironment_,
            std::string("[TRACE] TerminalBackend::TickGameplay sortie | ") +
                BuildArmySessionDebugLine(armySession_));
    }
}

bool TerminalBackend::ConsumeOutputDirty()
{
    const bool wasDirty = outputDirty_;
    outputDirty_ = false;
    return wasDirty;
}

bool TerminalBackend::ConsumeInputDirty()
{
    const bool wasDirty = inputDirty_;
    inputDirty_ = false;
    return wasDirty;
}

void TerminalBackend::MarkUiDirty()
{
    outputDirty_ = true;
    inputDirty_ = true;
}

void TerminalBackend::SetArmyCommandEnvironment(const ArmyCommandEnvironment& environment)
{
    armyEnvironment_ = environment;
}

void TerminalBackend::RegisterBuiltinCommands()
{
    commandRegistry_.Register({
        "help",
        "/help - liste les commandes disponibles.",
        [](const CommandContext& context, const std::vector<std::string>&)
        {
            context.writeLine("Commandes disponibles :");
            context.writeLine(" - /help : affiche cette aide.");
            context.writeLine(" - /status : affiche l'etat du terminal et de /army.");
            context.writeLine(" - /army : prepare l'invocation de l'armee des morts.");
        } });

    commandRegistry_.Register({
        "status",
        "/status - affiche l'etat courant du terminal.",
        [](const CommandContext& context, const std::vector<std::string>&)
        {
            const ArmySession& session = context.getArmySession();
            context.writeLine("[INFO] Etat du terminal :");
            std::ostringstream gameplayQueueLine;
            gameplayQueueLine << "[INFO] Commandes gameplay en attente : " << context.getPendingGameplayCommandCount() << '.';
            context.writeLine(gameplayQueueLine.str());
            context.writeLine(BuildArmyStatusLine(session));
        } });

    commandRegistry_.Register({
        "army",
        "/army - prepare 30 allies temporaires pour 180 secondes.",
        [this](const CommandContext& context, const std::vector<std::string>&)
        {
            const ArmyCommandSpec& spec = GetArmyCommandSpec();
            ArmySession& session = context.getArmySession();

            const ArmyPreflightCode preflightCode = EvaluateArmyPreflight();
            if (preflightCode != ArmyPreflightCode::Ok)
            {
                context.writeLine(ToPreflightMessage(preflightCode));
                context.writeLine(BuildArmyStatusLine(session));
                return;
            }

            ResetArmySession(session);
            session.state = ArmyState::Preparing;
            session.requestedCount = spec.unitCount;
            session.durationSeconds = spec.durationSeconds;
            session.remainingSeconds = spec.durationSeconds;
            session.lockOneArmyAtATime = spec.singleArmyAtATime;

            GameplayCommand gameplayCommand;
            gameplayCommand.type = GameplayCommandType::SummonArmy;
            gameplayCommand.requestedCount = session.requestedCount;
            gameplayCommand.durationSeconds = session.durationSeconds;
            gameplayCommand.templateNames.reserve(static_cast<size_t>(session.requestedCount));

            for (int index = 0; index < session.requestedCount; ++index)
            {
                const std::string templateName = spec.templateNames[static_cast<size_t>(index) % spec.templateNames.size()];
                session.pendingRequests.push_back({ templateName, index });
                gameplayCommand.templateNames.push_back(templateName);
            }
            session.pendingRequestCount = static_cast<int>(session.pendingRequests.size());
            session.currentWaveTarget = 1;

            context.enqueueGameplayCommand(gameplayCommand);

            std::ostringstream acceptedMessage;
            acceptedMessage
                << "[OK] " << spec.displayName
                << " : preparation de " << spec.unitCount << " invocations.";
            context.writeLine(acceptedMessage.str());
            context.writeLine("[INFO] Plan de validation du spawn : 1 -> 3 -> 10 -> 30.");
            context.writeLine("[INFO] Le spawn passera uniquement par une voie de factory Kenshi.");
            context.writeLine("[INFO] Les messages de progression et de fin seront publies par le game tick.");
            context.writeLine(BuildArmyStatusLine(session));
            TraceDebug(
                armyEnvironment_,
                std::string("[TRACE] Commande /army preparee | ") +
                    BuildArmySessionDebugLine(session));
        } });
}

void TerminalBackend::ExecuteLine(const std::string& line)
{
    if (line.empty())
    {
        return;
    }

    if (line.front() != '/')
    {
        AppendOutputLine("Seules les commandes slash sont prises en charge pour le moment.");
        return;
    }

    const std::vector<std::string> tokens = Tokenize(line);
    if (tokens.empty())
    {
        return;
    }

    const RegisteredCommand* command = commandRegistry_.Find(tokens.front());
    if (command == nullptr)
    {
        AppendOutputLine("Commande inconnue. Tape /help pour la liste.");
        return;
    }

    std::vector<std::string> args;
    if (tokens.size() > 1)
    {
        args.assign(tokens.begin() + 1, tokens.end());
    }

    const CommandContext context = {
        [this](const std::string& outputLine)
        {
            AppendOutputLine(outputLine);
        },
        [this](const GameplayCommand& commandToQueue)
        {
            QueueGameplayCommand(commandToQueue);
        },
        [this]() -> ArmySession&
        {
            return armySession_;
        },
        [this]() -> size_t
        {
            return gameplayCommandQueue_.size();
        },
        [this]() -> bool
        {
            return armyEnvironment_.isGameLoaded();
        },
        [this]() -> bool
        {
            return armyEnvironment_.hasResolvableLeader();
        },
        [this]() -> bool
        {
            return armyEnvironment_.areArmyTemplatesAvailable();
        },
        [this]() -> bool
        {
            return armyEnvironment_.isSpawnSystemReady();
        }
    };

    command->handler(context, args);
}

void TerminalBackend::QueueGameplayCommand(const GameplayCommand& command)
{
    gameplayCommandQueue_.push_back(command);
    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] TerminalBackend::QueueGameplayCommand | type=") +
            (command.type == GameplayCommandType::SummonArmy ? "SummonArmy" : "Unknown") +
            " | pending_gameplay=" +
            std::to_string(gameplayCommandQueue_.size()));
}

void TerminalBackend::QueuePendingCommand(const PendingCommand& command)
{
    pendingCommands_.push_back(command);
}

void TerminalBackend::ProcessGameplayCommand(const GameplayCommand& command)
{
    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] TerminalBackend::ProcessGameplayCommand entree | type=") +
            (command.type == GameplayCommandType::SummonArmy ? "SummonArmy" : "Unknown") +
            " | " +
            BuildArmySessionDebugLine(armySession_));

    switch (command.type)
    {
    case GameplayCommandType::SummonArmy:
        if (armySession_.state == ArmyState::Preparing)
        {
            // La je depile la demande /army sur le tick gameplay et je laisse le SpawnManager consommer la file progressivement.
            armySession_.state = ArmyState::Spawning;
            armySession_.pendingRequestCount = static_cast<int>(armySession_.pendingRequests.size());
            armySession_.currentWaveTarget = std::max(1, std::min(armySession_.requestedCount, 1));
            AppendOutputLine("[INFO] Spawn en cours : 0 / 30 unites creees.");
            TraceDebug(
                armyEnvironment_,
                std::string("[TRACE] TerminalBackend::ProcessGameplayCommand /army -> Spawning | ") +
                    BuildArmySessionDebugLine(armySession_));
        }
        break;
    default:
        break;
    }
}

void TerminalBackend::TickArmySession(float deltaSeconds)
{
    if (!armySession_.active || armySession_.state != ArmyState::Active)
    {
        return;
    }

    if (armySession_.remainingSeconds > 0.0f)
    {
        armySession_.remainingSeconds = std::max(0.0f, armySession_.remainingSeconds - deltaSeconds);
        outputDirty_ = true;
    }

    if (armySession_.remainingSeconds <= 0.0f)
    {
        armySession_.state = ArmyState::Dismissing;
        armySession_.active = false;
        AppendOutputLine("[OK] Fin d'invocation : nettoyage effectue.");
    }
}

ArmyPreflightCode TerminalBackend::EvaluateArmyPreflight() const
{
    if (!armyEnvironment_.isGameLoaded())
    {
        return ArmyPreflightCode::GameNotLoaded;
    }

    if (!armyEnvironment_.hasResolvableLeader())
    {
        return ArmyPreflightCode::LeaderUnavailable;
    }

    if (armySession_.lockOneArmyAtATime && armySession_.state != ArmyState::Idle)
    {
        return ArmyPreflightCode::ArmyAlreadyActive;
    }

    if (!armyEnvironment_.areArmyTemplatesAvailable())
    {
        return ArmyPreflightCode::MissingTemplates;
    }

    if (!armyEnvironment_.isSpawnSystemReady())
    {
        return ArmyPreflightCode::SpawnSystemUnavailable;
    }

    return ArmyPreflightCode::Ok;
}

std::string TerminalBackend::Trim(const std::string& value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(start, end - start);
}

std::vector<std::string> TerminalBackend::Tokenize(const std::string& line)
{
    std::istringstream builder(line);
    std::vector<std::string> tokens;
    std::string token;

    while (builder >> token)
    {
        tokens.push_back(token);
    }

    return tokens;
}
