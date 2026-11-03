#pragma once

#include "UInfo.h"

class UMutator;
class UGameReplicationInfo;
class UStatLog;

class UGameInfo : public UInfo
{
public:
	using UInfo::UInfo;

	ScriptString& AdminPassword() { return Value<ScriptString>(PropOffsets_GameInfo.AdminPassword); }
	float& AutoAim() { return Value<float>(PropOffsets_GameInfo.AutoAim); }
	UMutator*& BaseMutator() { return Value<UMutator*>(PropOffsets_GameInfo.BaseMutator); }
	ScriptString& BeaconName() { return Value<ScriptString>(PropOffsets_GameInfo.BeaconName); }
	ScriptString& BotMenuType() { return Value<ScriptString>(PropOffsets_GameInfo.BotMenuType); }
	int& CurrentID() { return Value<int>(PropOffsets_GameInfo.CurrentID); }
	UClass*& DMMessageClass() { return Value<UClass*>(PropOffsets_GameInfo.DMMessageClass); }
	UMutator*& DamageMutator() { return Value<UMutator*>(PropOffsets_GameInfo.DamageMutator); }
	UClass*& DeathMessageClass() { return Value<UClass*>(PropOffsets_GameInfo.DeathMessageClass); }
	UClass*& DefaultPlayerClass() { return Value<UClass*>(PropOffsets_GameInfo.DefaultPlayerClass); }
	ScriptString& DefaultPlayerName() { return Value<ScriptString>(PropOffsets_GameInfo.DefaultPlayerName); }
	NameString& DefaultPlayerState() { return Value<NameString>(PropOffsets_GameInfo.DefaultPlayerState); }
	UClass*& DefaultWeapon() { return Value<UClass*>(PropOffsets_GameInfo.DefaultWeapon); }
	int& DemoBuild() { return Value<int>(PropOffsets_GameInfo.DemoBuild); }
	int& DemoHasTuts() { return Value<int>(PropOffsets_GameInfo.DemoHasTuts); }
	uint8_t& Difficulty() { return Value<uint8_t>(PropOffsets_GameInfo.Difficulty); }
	ScriptString& EnabledMutators() { return Value<ScriptString>(PropOffsets_GameInfo.EnabledMutators); }
	ScriptString& EnteredMessage() { return Value<ScriptString>(PropOffsets_GameInfo.EnteredMessage); }
	ScriptString& FailedPlaceMessage() { return Value<ScriptString>(PropOffsets_GameInfo.FailedPlaceMessage); }
	ScriptString& FailedSpawnMessage() { return Value<ScriptString>(PropOffsets_GameInfo.FailedSpawnMessage); }
	ScriptString& FailedTeamMessage() { return Value<ScriptString>(PropOffsets_GameInfo.FailedTeamMessage); }
	UClass*& GameMenuType() { return Value<UClass*>(PropOffsets_GameInfo.GameMenuType); }
	ScriptString& GameName() { return Value<ScriptString>(PropOffsets_GameInfo.GameName); }
	ScriptString& GameOptionsMenuType() { return Value<ScriptString>(PropOffsets_GameInfo.GameOptionsMenuType); }
	ScriptString& GamePassword() { return Value<ScriptString>(PropOffsets_GameInfo.GamePassword); }
	UGameReplicationInfo*& GameReplicationInfo() { return Value<UGameReplicationInfo*>(PropOffsets_GameInfo.GameReplicationInfo); }
	UClass*& GameReplicationInfoClass() { return Value<UClass*>(PropOffsets_GameInfo.GameReplicationInfoClass); }
	float& GameSpeed() { return Value<float>(PropOffsets_GameInfo.GameSpeed); }
	ScriptString& GameUMenuType() { return Value<ScriptString>(PropOffsets_GameInfo.GameUMenuType); }
	UClass*& HUDType() { return Value<UClass*>(PropOffsets_GameInfo.HUDType); }
	ScriptString& IPBanned() { return Value<ScriptString>(PropOffsets_GameInfo.IPBanned); }
	ScriptString& IPPolicies() { return Value<ScriptString>(PropOffsets_GameInfo.IPPolicies); }
	int& ItemGoals() { return Value<int>(PropOffsets_GameInfo.ItemGoals); }
	int& KillGoals() { return Value<int>(PropOffsets_GameInfo.KillGoals); }
	ScriptString& LeftMessage() { return Value<ScriptString>(PropOffsets_GameInfo.LeftMessage); }
	UStatLog*& LocalLog() { return Value<UStatLog*>(PropOffsets_GameInfo.LocalLog); }
	ScriptString& LocalLogFileName() { return Value<ScriptString>(PropOffsets_GameInfo.LocalLogFileName); }
	UClass*& MapListType() { return Value<UClass*>(PropOffsets_GameInfo.MapListType); }
	ScriptString& MapPrefix() { return Value<ScriptString>(PropOffsets_GameInfo.MapPrefix); }
	int& MaxPlayers() { return Value<int>(PropOffsets_GameInfo.MaxPlayers); }
	int& MaxSpectators() { return Value<int>(PropOffsets_GameInfo.MaxSpectators); }
	ScriptString& MaxedOutMessage() { return Value<ScriptString>(PropOffsets_GameInfo.MaxedOutMessage); }
	UMutator*& MessageMutator() { return Value<UMutator*>(PropOffsets_GameInfo.MessageMutator); }
	ScriptString& MultiplayerUMenuType() { return Value<ScriptString>(PropOffsets_GameInfo.MultiplayerUMenuType); }
	UClass*& MutatorClass() { return Value<UClass*>(PropOffsets_GameInfo.MutatorClass); }
	ScriptString& NameChangedMessage() { return Value<ScriptString>(PropOffsets_GameInfo.NameChangedMessage); }
	ScriptString& NeedPassword() { return Value<ScriptString>(PropOffsets_GameInfo.NeedPassword); }
	int& NumPlayers() { return Value<int>(PropOffsets_GameInfo.NumPlayers); }
	int& NumSpectators() { return Value<int>(PropOffsets_GameInfo.NumSpectators); }
	ScriptString& RulesMenuType() { return Value<ScriptString>(PropOffsets_GameInfo.RulesMenuType); }
	UClass*& ScoreBoardType() { return Value<UClass*>(PropOffsets_GameInfo.ScoreBoardType); }
	int& SecretGoals() { return Value<int>(PropOffsets_GameInfo.SecretGoals); }
	int& SentText() { return Value<int>(PropOffsets_GameInfo.SentText); }
	ScriptString& ServerLogName() { return Value<ScriptString>(PropOffsets_GameInfo.ServerLogName); }
	ScriptString& SettingsMenuType() { return Value<ScriptString>(PropOffsets_GameInfo.SettingsMenuType); }
	ScriptString& SpecialDamageString() { return Value<ScriptString>(PropOffsets_GameInfo.SpecialDamageString); }
	float& StartTime() { return Value<float>(PropOffsets_GameInfo.StartTime); }
	UClass*& StatLogClass() { return Value<UClass*>(PropOffsets_GameInfo.StatLogClass); }
	ScriptString& SwitchLevelMessage() { return Value<ScriptString>(PropOffsets_GameInfo.SwitchLevelMessage); }
	UClass*& WaterZoneType() { return Value<UClass*>(PropOffsets_GameInfo.WaterZoneType); }
	UStatLog*& WorldLog() { return Value<UStatLog*>(PropOffsets_GameInfo.WorldLog); }
	ScriptString& WorldLogFileName() { return Value<ScriptString>(PropOffsets_GameInfo.WorldLogFileName); }
	ScriptString& WrongPassword() { return Value<ScriptString>(PropOffsets_GameInfo.WrongPassword); }
	BitfieldBool bAllowFOV() { return BoolValue(PropOffsets_GameInfo.bAllowFOV); }
	BitfieldBool bAlternateMode() { return BoolValue(PropOffsets_GameInfo.bAlternateMode); }
	BitfieldBool bBatchLocal() { return BoolValue(PropOffsets_GameInfo.bBatchLocal); }
	BitfieldBool bCanChangeSkin() { return BoolValue(PropOffsets_GameInfo.bCanChangeSkin); }
	BitfieldBool bCanViewOthers() { return BoolValue(PropOffsets_GameInfo.bCanViewOthers); }
	BitfieldBool bClassicDeathMessages() { return BoolValue(PropOffsets_GameInfo.bClassicDeathMessages); }
	BitfieldBool bCoopWeaponMode() { return BoolValue(PropOffsets_GameInfo.bCoopWeaponMode); }
	BitfieldBool bDeathMatch() { return BoolValue(PropOffsets_GameInfo.bDeathMatch); }
	BitfieldBool bExternalBatcher() { return BoolValue(PropOffsets_GameInfo.bExternalBatcher); }
	BitfieldBool bGameEnded() { return BoolValue(PropOffsets_GameInfo.bGameEnded); }
	BitfieldBool bHumansOnly() { return BoolValue(PropOffsets_GameInfo.bHumansOnly); }
	BitfieldBool bLocalLog() { return BoolValue(PropOffsets_GameInfo.bLocalLog); }
	BitfieldBool bLoggingGame() { return BoolValue(PropOffsets_GameInfo.bLoggingGame); }
	BitfieldBool bLowGore() { return BoolValue(PropOffsets_GameInfo.bLowGore); }
	BitfieldBool bMuteSpectators() { return BoolValue(PropOffsets_GameInfo.bMuteSpectators); }
	BitfieldBool bNoCheating() { return BoolValue(PropOffsets_GameInfo.bNoCheating); }
	BitfieldBool bNoMonsters() { return BoolValue(PropOffsets_GameInfo.bNoMonsters); }
	BitfieldBool bOverTime() { return BoolValue(PropOffsets_GameInfo.bOverTime); }
	BitfieldBool bPauseable() { return BoolValue(PropOffsets_GameInfo.bPauseable); }
	BitfieldBool bRestartLevel() { return BoolValue(PropOffsets_GameInfo.bRestartLevel); }
	BitfieldBool bTeamGame() { return BoolValue(PropOffsets_GameInfo.bTeamGame); }
	BitfieldBool bVeryLowGore() { return BoolValue(PropOffsets_GameInfo.bVeryLowGore); }
	BitfieldBool bWorldLog() { return BoolValue(PropOffsets_GameInfo.bWorldLog); }
};
