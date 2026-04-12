#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct CommandContext;

using CommandHandler = std::function<void(const CommandContext&, const std::vector<std::string>&)>;

struct RegisteredCommand
{
    std::string name;
    std::string help;
    CommandHandler handler;
};

class CommandRegistry
{
public:
    void Register(const RegisteredCommand& cmd);
    bool Exists(const std::string& name) const;
    const RegisteredCommand* Find(const std::string& name) const;
    std::vector<const RegisteredCommand*> List() const;

private:
    static std::string NormalizeName(const std::string& name);

    std::unordered_map<std::string, RegisteredCommand> commands_;
    std::vector<std::string> order_;
};
