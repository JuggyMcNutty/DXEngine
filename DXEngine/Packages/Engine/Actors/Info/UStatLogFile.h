#pragma once

#include "UStatLog.h"

class UStatLogFile : public UStatLog
{
public:
	using UStatLog::UStatLog;

	int& LogAr() { return Value<int>(PropOffsets_StatLogFile.LogAr); }
	ScriptString& StatLogFile() { return Value<ScriptString>(PropOffsets_StatLogFile.StatLogFile); }
	ScriptString& StatLogFinal() { return Value<ScriptString>(PropOffsets_StatLogFile.StatLogFinal); }
	BitfieldBool bWatermark() { return BoolValue(PropOffsets_StatLogFile.bWatermark); }
};
