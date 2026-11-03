#pragma once

#include "UReplicationInfo.h"

class UPlayerReplicationInfo;

class UGameReplicationInfo : public UReplicationInfo
{
public:
	using UReplicationInfo::UReplicationInfo;

	ScriptString& AdminEmail() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.AdminEmail); }
	ScriptString& AdminName() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.AdminName); }
	int& ElapsedTime() { return Value<int>(PropOffsets_GameReplicationInfo.ElapsedTime); }
	ScriptString& GameClass() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.GameClass); }
	ScriptString& GameEndedComments() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.GameEndedComments); }
	ScriptString& GameName() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.GameName); }
	ScriptString& MOTDLine1() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.MOTDLine1); }
	ScriptString& MOTDLine2() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.MOTDLine2); }
	ScriptString& MOTDLine3() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.MOTDLine3); }
	ScriptString& MOTDLine4() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.MOTDLine4); }
	int& NumPlayers() { return Value<int>(PropOffsets_GameReplicationInfo.NumPlayers); }
	UPlayerReplicationInfo*& PRIArray() { return Value<UPlayerReplicationInfo*>(PropOffsets_GameReplicationInfo.PRIArray); }
	int& Region() { return Value<int>(PropOffsets_GameReplicationInfo.Region); }
	int& RemainingMinute() { return Value<int>(PropOffsets_GameReplicationInfo.RemainingMinute); }
	int& RemainingTime() { return Value<int>(PropOffsets_GameReplicationInfo.RemainingTime); }
	float& SecondCount() { return Value<float>(PropOffsets_GameReplicationInfo.SecondCount); }
	ScriptString& ServerName() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.ServerName); }
	ScriptString& ShortName() { return Value<ScriptString>(PropOffsets_GameReplicationInfo.ShortName); }
	int& SumFrags() { return Value<int>(PropOffsets_GameReplicationInfo.SumFrags); }
	float& UpdateTimer() { return Value<float>(PropOffsets_GameReplicationInfo.UpdateTimer); }
	BitfieldBool bClassicDeathMessages() { return BoolValue(PropOffsets_GameReplicationInfo.bClassicDeathMessages); }
	BitfieldBool bStopCountDown() { return BoolValue(PropOffsets_GameReplicationInfo.bStopCountDown); }
	BitfieldBool bTeamGame() { return BoolValue(PropOffsets_GameReplicationInfo.bTeamGame); }
};
