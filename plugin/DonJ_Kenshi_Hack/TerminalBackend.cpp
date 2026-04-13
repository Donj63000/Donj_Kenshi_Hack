#include "ArmyDiagnostics.h"
#include "TerminalBackend.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace
{
    const char* ToGameplayCommandLabel(GameplayCommandType::Type type)
    {
        switch (type)
        {
        case GameplayCommandType::SummonArmy:
            return "SummonArmy";
        case GameplayCommandType::DismissArmy:
            return "DismissArmy";
        default:
            return "Unknown";
        }
    }

    std::string BuildArmyStatusLine(const ArmySession& session)
    {
        const ArmyCommandSpec& spec = GetArmyCommandSpec();
        const int targetUnitCount = session.requestedCount > 0 ? session.requestedCount : spec.unitCount;
        std::ostringstream builder;
        builder
            << "[INFO] /army : etat=" << ToStatusLabel(session.state)
            << ", unites=" << session.spawnedCount << " / " << targetUnitCount
            << ", spawned=" << session.spawnedCount
            << ", pending=" << session.pendingRequestCount
            << ", pending_finalize=" << session.pendingFinalizeUnits.size()
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
    : maxOutputLines_(64)
    , commandHistoryCursor_(0)
    , commandHistoryBrowsing_(false)
    , inputActive_(false)
    , outputDirty_(true)
    , inputDirty_(true)
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

    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] input_submit_called | line=") +
            line +
            " | pending_ui_before=" +
            DonjToString(pendingCommands_.size()));

    commandHistoryBrowsing_ = false;
    commandHistoryCursor_ = commandHistory_.size();
    historyDraft_.clear();
    inputBuffer_.clear();
    inputDirty_ = true;

    if (line.empty())
    {
        TraceDebug(armyEnvironment_, "[TRACE] input_submit_ignored_empty");
        return false;
    }

    commandHistory_.push_back(line);
    commandHistoryCursor_ = commandHistory_.size();

    AppendOutputLine("> " + line);
    QueuePendingCommand(PendingCommand(line));
    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] input_submit_success | line=") +
            line +
            " | pending_ui_after=" +
            DonjToString(pendingCommands_.size()));
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
    for (std::size_t index = 0; index < outputHistory_.size(); ++index)
    {
        if (!firstLine)
        {
            builder << '\n';
        }

        builder << outputHistory_[index];
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
        TraceDebug(
            armyEnvironment_,
            std::string("[TRACE] ui_command_dequeued | line=") +
                command.rawLine +
                " | pending_ui_after_pop=" +
                DonjToString(pendingCommands_.size()));
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
    commandRegistry_.Register(RegisteredCommand(
        "help",
        "/help - liste les commandes disponibles.",
        [](const CommandContext& context, const std::vector<std::string>&)
        {
            context.writeLine("Commandes disponibles :");
            context.writeLine(" - /help : affiche cette aide.");
            context.writeLine(" - /status : affiche l'etat du terminal et de /army.");
            context.writeLine(" - /army : prepare 30 allies temporaires pour 180 secondes.");
            context.writeLine(" - /armytest <1..30> : lance une invocation de validation avec un plus petit volume.");
            context.writeLine(" - /dismiss : force la dissolution de l'armee active.");
        }));

    commandRegistry_.Register(RegisteredCommand(
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
        }));

    commandRegistry_.Register(RegisteredCommand(
        "army",
        "/army - prepare 30 allies temporaires pour 180 secondes.",
        [this](const CommandContext& context, const std::vector<std::string>&)
        {
            const ArmyPreflightCode::Type preflightCode = EvaluateArmyPreflight();
            if (preflightCode != ArmyPreflightCode::Ok)
            {
                context.writeLine(ToPreflightMessage(preflightCode));
                context.writeLine(BuildArmyStatusLine(context.getArmySession()));
                return;
            }

            QueueArmyInvocation(context, GetArmyCommandSpec().unitCount, GetArmyCommandSpec().durationSeconds, false);
        }));

    commandRegistry_.Register(RegisteredCommand(
        "armytest",
        "/armytest <1..30> - prepare une invocation de validation.",
        [this](const CommandContext& context, const std::vector<std::string>& args)
        {
            const ArmyPreflightCode::Type preflightCode = EvaluateArmyPreflight();
            if (preflightCode != ArmyPreflightCode::Ok)
            {
                context.writeLine(ToPreflightMessage(preflightCode));
                context.writeLine(BuildArmyStatusLine(context.getArmySession()));
                return;
            }

            if (args.empty())
            {
                context.writeLine("[INFO] Usage : /armytest <1..30>.");
                return;
            }

            int requestedCount = 0;
            if (!TryParsePositiveInt(args.front(), requestedCount))
            {
                context.writeLine("[ERREUR] /armytest attend un entier compris entre 1 et 30.");
                return;
            }

            requestedCount = std::max(1, std::min(30, requestedCount));
            QueueArmyInvocation(context, requestedCount, GetArmyCommandSpec().durationSeconds, true);
        }));

    commandRegistry_.Register(RegisteredCommand(
        "dismiss",
        "/dismiss - force la dissolution de l'armee active.",
        [this](const CommandContext& context, const std::vector<std::string>&)
        {
            ArmySession& session = context.getArmySession();
            if (session.state == ArmyState::Idle)
            {
                context.writeLine("[INFO] /dismiss : aucune invocation active.");
                return;
            }

            GameplayCommand gameplayCommand;
            gameplayCommand.type = GameplayCommandType::DismissArmy;
            context.enqueueGameplayCommand(gameplayCommand);
            context.writeLine("[OK] /dismiss : nettoyage force programme.");
        }));
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

    CommandContext context;
    context.writeLine = [this](const std::string& outputLine)
    {
        AppendOutputLine(outputLine);
    };
    context.enqueueGameplayCommand = [this](const GameplayCommand& commandToQueue)
    {
        QueueGameplayCommand(commandToQueue);
    };
    context.getArmySession = [this]() -> ArmySession&
    {
        return armySession_;
    };
    context.getPendingGameplayCommandCount = [this]() -> size_t
    {
        return gameplayCommandQueue_.size();
    };
    context.isGameLoaded = [this]() -> bool
    {
        return armyEnvironment_.isGameLoaded();
    };
    context.hasResolvableLeader = [this]() -> bool
    {
        return armyEnvironment_.hasResolvableLeader();
    };
    context.areArmyTemplatesAvailable = [this]() -> bool
    {
        return armyEnvironment_.areArmyTemplatesAvailable();
    };
    context.isSpawnSystemReady = [this]() -> bool
    {
        return armyEnvironment_.isSpawnSystemReady();
    };

    command->handler(context, args);
}

void TerminalBackend::QueueGameplayCommand(const GameplayCommand& command)
{
    gameplayCommandQueue_.push_back(command);
    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] gameplay_command_enqueued | type=") +
            ToGameplayCommandLabel(command.type) +
            " | pending_gameplay=" +
            DonjToString(gameplayCommandQueue_.size()));
}

void TerminalBackend::QueuePendingCommand(const PendingCommand& command)
{
    pendingCommands_.push_back(command);
}


void TerminalBackend::ProcessGameplayCommand(const GameplayCommand& command)
{
    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] gameplay_command_processing | type=") +
            ToGameplayCommandLabel(command.type) +
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

            std::ostringstream statusLine;
            statusLine << "[INFO] Spawn en cours : 0 / " << armySession_.requestedCount << " unites creees.";
            AppendOutputLine(statusLine.str());

            TraceDebug(
                armyEnvironment_,
                std::string("[TRACE] TerminalBackend::ProcessGameplayCommand /army -> Spawning | ") +
                    BuildArmySessionDebugLine(armySession_));
        }
        break;

    case GameplayCommandType::DismissArmy:
        if (armySession_.state != ArmyState::Idle)
        {
            armySession_.pendingRequests.clear();
            armySession_.pendingRequestCount = 0;
            armySession_.waitingForReplayOpportunity = false;
            armySession_.state = ArmyState::Dismissing;
            armySession_.active = false;
            armySession_.remainingSeconds = 0.0f;
            AppendOutputLine("[OK] Dissolution forcee : nettoyage demande au runtime.");
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

    if (!armySession_.pendingFinalizeUnits.empty())
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


void TerminalBackend::QueueArmyInvocation(const CommandContext& context, int requestedCount, float durationSeconds, bool testMode)
{
    const ArmyCommandSpec& spec = GetArmyCommandSpec();
    ArmySession& session = context.getArmySession();

    ResetArmySession(session);
    session.state = ArmyState::Preparing;
    session.requestedCount = std::max(1, requestedCount);
    session.durationSeconds = durationSeconds;
    session.remainingSeconds = durationSeconds;
    session.lockOneArmyAtATime = spec.singleArmyAtATime;

    GameplayCommand gameplayCommand;
    gameplayCommand.type = GameplayCommandType::SummonArmy;
    gameplayCommand.requestedCount = session.requestedCount;
    gameplayCommand.durationSeconds = session.durationSeconds;
    gameplayCommand.templateNames.reserve(static_cast<size_t>(session.requestedCount));

    for (int index = 0; index < session.requestedCount; ++index)
    {
        const std::string templateName = spec.templateNames[static_cast<size_t>(index) % spec.templateNames.size()];
        session.pendingRequests.push_back(SpawnRequest(templateName, index));
        gameplayCommand.templateNames.push_back(templateName);
    }

    session.pendingRequestCount = static_cast<int>(session.pendingRequests.size());
    session.currentWaveTarget = 1;

    context.enqueueGameplayCommand(gameplayCommand);

    std::ostringstream acceptedMessage;
    acceptedMessage
        << "[OK] " << spec.displayName
        << (testMode ? " (mode test)" : "")
        << " : preparation de " << session.requestedCount << " invocation(s).";
    context.writeLine(acceptedMessage.str());
    context.writeLine("[INFO] Plan de validation recommande : 1 -> 3 -> 10 -> 30.");
    context.writeLine("[INFO] Le spawn passera uniquement par une voie de factory Kenshi.");
    context.writeLine("[INFO] Utilise /dismiss pour nettoyer une session bloquee.");
    context.writeLine("[INFO] Les messages de progression et de fin seront publies par le game tick.");
    context.writeLine(BuildArmyStatusLine(session));

    TraceDebug(
        armyEnvironment_,
        std::string("[TRACE] QueueArmyInvocation | ") +
            BuildArmySessionDebugLine(session));
}

ArmyPreflightCode::Type TerminalBackend::EvaluateArmyPreflight() const
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

    if (!armyEnvironment_.isFactoryAvailable())
    {
        return ArmyPreflightCode::FactoryUnavailable;
    }

    if (!armyEnvironment_.isReplayHookInstalled())
    {
        return ArmyPreflightCode::ReplayHookUnavailable;
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


bool TerminalBackend::TryParsePositiveInt(const std::string& value, int& outValue)
{
    std::istringstream stream(value);
    int parsedValue = 0;
    char trailingCharacter = '\0';
    if (!(stream >> parsedValue))
    {
        return false;
    }

    if (stream >> trailingCharacter)
    {
        return false;
    }

    if (parsedValue <= 0)
    {
        return false;
    }

    outValue = parsedValue;
    return true;
}
