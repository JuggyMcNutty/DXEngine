
#include "Precomp.h"
#include "NConsole.h"
#include "VM/NativeFunc.h"
#include "VM/ScriptCall.h"
#include "Engine.h"

void NConsole::RegisterFunctions()
{
	RegisterVMNativeFunc_2("Console", "ConsoleCommand", &NConsole::ConsoleCommand, 0);
	RegisterVMNativeFunc_1("Console", "SaveTimeDemo", &NConsole::SaveTimeDemo, 0);
}

// The original's (Engine.dll 0x10379900): the viewport runs the line, the
// console the output, each line of it a message (UConsole::Serialize
// 0x10379740), and the answer whether anything took the line, which the
// console's typing says "Unrecognized command" for.
void NConsole::ConsoleCommand(UObject* Self, const std::string& S, BitfieldBool& ReturnValue)
{
	ExecOutput out;
	ReturnValue = engine->ViewportExec(S, out);
	for (const std::string& line : out.Lines)
	{
		if (line.empty())
			continue;
		// XXX: This depends on how Engine.Console.Message is declared
		Array<ExpressionValue> exprs = {};
		exprs.push_back(ExpressionValue::ObjectValue(nullptr));
		exprs.push_back(ExpressionValue::StringValue(line));
		exprs.push_back(ExpressionValue::NameValue("Console"));
		CallEvent(Self, "Message", exprs);
	}
}

void NConsole::SaveTimeDemo(UObject* Self, const std::string& S)
{
	LogUnimplemented("Console.SaveTimeDemo(" + S + ")");
}
