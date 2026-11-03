#pragma once

#include "UInfo.h"

class UStatLog : public UInfo
{
public:
	using UInfo::UInfo;

	int& Context() { return Value<int>(PropOffsets_StatLog.Context); }
	ScriptString& DecoderRingURL() { return Value<ScriptString>(PropOffsets_StatLog.DecoderRingURL); }
	ScriptString& GameCreator() { return Value<ScriptString>(PropOffsets_StatLog.GameCreator); }
	ScriptString& GameCreatorURL() { return Value<ScriptString>(PropOffsets_StatLog.GameCreatorURL); }
	ScriptString& GameName() { return Value<ScriptString>(PropOffsets_StatLog.GameName); }
	ScriptString& LocalBatcherParams() { return Value<ScriptString>(PropOffsets_StatLog.LocalBatcherParams); }
	ScriptString& LocalBatcherURL() { return Value<ScriptString>(PropOffsets_StatLog.LocalBatcherURL); }
	ScriptString& LocalLogDir() { return Value<ScriptString>(PropOffsets_StatLog.LocalLogDir); }
	ScriptString& LocalStandard() { return Value<ScriptString>(PropOffsets_StatLog.LocalStandard); }
	ScriptString& LocalStatsURL() { return Value<ScriptString>(PropOffsets_StatLog.LocalStatsURL); }
	ScriptString& LogInfoURL() { return Value<ScriptString>(PropOffsets_StatLog.LogInfoURL); }
	ScriptString& LogVersion() { return Value<ScriptString>(PropOffsets_StatLog.LogVersion); }
	float& TimeStamp() { return Value<float>(PropOffsets_StatLog.TimeStamp); }
	ScriptString& WorldBatcherParams() { return Value<ScriptString>(PropOffsets_StatLog.WorldBatcherParams); }
	ScriptString& WorldBatcherURL() { return Value<ScriptString>(PropOffsets_StatLog.WorldBatcherURL); }
	ScriptString& WorldLogDir() { return Value<ScriptString>(PropOffsets_StatLog.WorldLogDir); }
	ScriptString& WorldStandard() { return Value<ScriptString>(PropOffsets_StatLog.WorldStandard); }
	ScriptString& WorldStatsURL() { return Value<ScriptString>(PropOffsets_StatLog.WorldStatsURL); }
	BitfieldBool bWorld() { return BoolValue(PropOffsets_StatLog.bWorld); }
	BitfieldBool bWorldBatcherError() { return BoolValue(PropOffsets_StatLog.bWorldBatcherError); }
};
