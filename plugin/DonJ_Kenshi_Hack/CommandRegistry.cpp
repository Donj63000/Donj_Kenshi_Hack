#include "CommandRegistry.h"

#include <algorithm>
#include <cctype>

namespace
{
    std::string NormalizeValue(const std::string& value)
    {
        std::string normalized = value;
        if (!normalized.empty() && normalized.front() == '/')
        {
            normalized.erase(normalized.begin());
        }

        std::transform(
            normalized.begin(),
            normalized.end(),
            normalized.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        return normalized;
    }
}

void CommandRegistry::Register(const RegisteredCommand& cmd)
{
    const std::string normalizedName = NormalizeName(cmd.name);

    RegisteredCommand normalizedCommand = cmd;
    normalizedCommand.name = normalizedName;

    const bool alreadyExists = commands_.find(normalizedName) != commands_.end();
    commands_[normalizedName] = normalizedCommand;

    if (!alreadyExists)
    {
        order_.push_back(normalizedName);
    }
}

bool CommandRegistry::Exists(const std::string& name) const
{
    return commands_.find(NormalizeName(name)) != commands_.end();
}

const RegisteredCommand* CommandRegistry::Find(const std::string& name) const
{
    const auto it = commands_.find(NormalizeName(name));
    if (it == commands_.end())
    {
        return nullptr;
    }

    return &it->second;
}

std::vector<const RegisteredCommand*> CommandRegistry::List() const
{
    std::vector<const RegisteredCommand*> commands;
    commands.reserve(order_.size());

    for (std::size_t index = 0; index < order_.size(); ++index)
    {
        const std::string& name = order_[index];
        const auto it = commands_.find(name);
        if (it != commands_.end())
        {
            commands.push_back(&it->second);
        }
    }

    return commands;
}

std::string CommandRegistry::NormalizeName(const std::string& name)
{
    return NormalizeValue(name);
}
