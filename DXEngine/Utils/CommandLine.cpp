
#include "Precomp.h"
#include "Utils/CommandLine.h"

CommandLine* commandline;

CommandLine::CommandLine(const Array<std::string>& args)
{
	for (size_t i = 0; i < args.size(); i++)
	{
		const std::string& arg = args[i];
		size_t dashes = (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') ? 2 : (arg.size() > 1 && arg[0] == '-') ? 1 : 0;
		if (dashes == 0)
		{
			Items.push_back(arg);
			continue;
		}

		size_t pos = arg.find('=', dashes);
		if (pos != std::string::npos)
		{
			Args[arg.substr(0, pos)] = arg.substr(pos + 1);
		}
		else if (TakesValue(arg) && i + 1 < args.size() && !args[i + 1].empty() && args[i + 1][0] != '-')
		{
			// "--url <map>" as "--url=<map>": the next word is the value, not a game folder
			Args[arg] = args[++i];
		}
		else
		{
			Args[arg] = {};
		}
	}
}

bool CommandLine::TakesValue(const std::string& option)
{
	static const char* options[] = { "-u", "--url", "--ini", "--userini", "--userdir", "--cmdline", "--timeline" };
	for (const char* name : options)
	{
		if (option == name)
			return true;
	}
	return false;
}

bool CommandLine::HasArg(const std::string& shortform, const std::string& longform) const
{
	return (!shortform.empty() && Args.find(shortform) != Args.end()) || (!longform.empty() && Args.find(longform) != Args.end());
}

std::string CommandLine::GetArg(const std::string& shortform, const std::string& longform, const std::string& defaultValue) const
{
	if (!shortform.empty())
	{
		auto it = Args.find(shortform);
		if (it != Args.end())
			return it->second;
	}

	if (!longform.empty())
	{
		auto it = Args.find(longform);
		if (it != Args.end())
			return it->second;
	}

	return defaultValue;
}
