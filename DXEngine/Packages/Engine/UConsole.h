#pragma once

#include "Packages/Core/UObject.h"

class UTexture;
class UPlayerReplicationInfo;
class UFont;
class UViewport;

class UConsole : public UObject
{
public:
	using UObject::UObject;
	bool TakesStateFrame() const override { return true; }

	ScriptString& AvgText() { return Value<ScriptString>(PropOffsets_Console.AvgText); }
	UTexture*& Border() { return Value<UTexture*>(PropOffsets_Console.Border); }
	int& BorderLines() { return Value<int>(PropOffsets_Console.BorderLines); }
	int& BorderPixels() { return Value<int>(PropOffsets_Console.BorderPixels); }
	int& BorderSize() { return Value<int>(PropOffsets_Console.BorderSize); }
	UTexture*& ConBackground() { return Value<UTexture*>(PropOffsets_Console.ConBackground); }
	ScriptString& ConnectingMessage() { return Value<ScriptString>(PropOffsets_Console.ConnectingMessage); }
	float& ConsoleDest() { return Value<float>(PropOffsets_Console.ConsoleDest); }
	int& ConsoleLines() { return Value<int>(PropOffsets_Console.ConsoleLines); }
	float& ConsolePos() { return Value<float>(PropOffsets_Console.ConsolePos); }
	float& ExtraTime() { return Value<float>(PropOffsets_Console.ExtraTime); }
	int& FrameCount() { return Value<int>(PropOffsets_Console.FrameCount); }
	ScriptString& FrameRateText() { return Value<ScriptString>(PropOffsets_Console.FrameRateText); }
	float& FrameX() { return Value<float>(PropOffsets_Console.FrameX); }
	float& FrameY() { return Value<float>(PropOffsets_Console.FrameY); }
	ScriptString& FramesText() { return Value<ScriptString>(PropOffsets_Console.FramesText); }
	ScriptString& History() { return Value<ScriptString>(PropOffsets_Console.History); }
	int& HistoryBot() { return Value<int>(PropOffsets_Console.HistoryBot); }
	int& HistoryCur() { return Value<int>(PropOffsets_Console.HistoryCur); }
	int& HistoryTop() { return Value<int>(PropOffsets_Console.HistoryTop); }
	float& LastFrameTime() { return Value<float>(PropOffsets_Console.LastFrameTime); }
	float& LastSecFPS() { return Value<float>(PropOffsets_Console.LastSecFPS); }
	ScriptString& LastSecText() { return Value<ScriptString>(PropOffsets_Console.LastSecText); }
	int& LastSecondFrameCount() { return Value<int>(PropOffsets_Console.LastSecondFrameCount); }
	float& LastSecondStartTime() { return Value<float>(PropOffsets_Console.LastSecondStartTime); }
	ScriptString& LoadingMessage() { return Value<ScriptString>(PropOffsets_Console.LoadingMessage); }
	float& MaxFPS() { return Value<float>(PropOffsets_Console.MaxFPS); }
	ScriptString& MaxText() { return Value<ScriptString>(PropOffsets_Console.MaxText); }
	float& MinFPS() { return Value<float>(PropOffsets_Console.MinFPS); }
	ScriptString& MinText() { return Value<ScriptString>(PropOffsets_Console.MinText); }
	UPlayerReplicationInfo*& MsgPlayer() { return Value<UPlayerReplicationInfo*>(PropOffsets_Console.MsgPlayer); }
	ScriptString& MsgText() { return Value<ScriptString>(PropOffsets_Console.MsgText); }
	float& MsgTick() { return Value<float>(PropOffsets_Console.MsgTick); }
	float& MsgTickTime() { return Value<float>(PropOffsets_Console.MsgTickTime); }
	float& MsgTime() { return Value<float>(PropOffsets_Console.MsgTime); }
	NameString& MsgType() { return Value<NameString>(PropOffsets_Console.MsgType); }
	ScriptString& PausedMessage() { return Value<ScriptString>(PropOffsets_Console.PausedMessage); }
	ScriptString& PrecachingMessage() { return Value<ScriptString>(PropOffsets_Console.PrecachingMessage); }
	ScriptString& SavingMessage() { return Value<ScriptString>(PropOffsets_Console.SavingMessage); }
	int& Scrollback() { return Value<int>(PropOffsets_Console.Scrollback); }
	ScriptString& SecondsText() { return Value<ScriptString>(PropOffsets_Console.SecondsText); }
	float& StartTime() { return Value<float>(PropOffsets_Console.StartTime); }
	int& TextLines() { return Value<int>(PropOffsets_Console.TextLines); }
	UFont*& TimeDemoFont() { return Value<UFont*>(PropOffsets_Console.TimeDemoFont); }
	int& TopLine() { return Value<int>(PropOffsets_Console.TopLine); }
	ScriptString& TypedStr() { return Value<ScriptString>(PropOffsets_Console.TypedStr); }
	UViewport*& Viewport() { return Value<UViewport*>(PropOffsets_Console.Viewport); }
	BitfieldBool bNoDrawWorld() { return BoolValue(PropOffsets_Console.bNoDrawWorld); }
	BitfieldBool bNoStuff() { return BoolValue(PropOffsets_Console.bNoStuff); }
	BitfieldBool bRestartTimeDemo() { return BoolValue(PropOffsets_Console.bRestartTimeDemo); }
	BitfieldBool bSaveTimeDemoToFile() { return BoolValue(PropOffsets_Console.bSaveTimeDemoToFile); }
	BitfieldBool bStartTimeDemo() { return BoolValue(PropOffsets_Console.bStartTimeDemo); }
	BitfieldBool bTimeDemo() { return BoolValue(PropOffsets_Console.bTimeDemo); }
	BitfieldBool bTyping() { return BoolValue(PropOffsets_Console.bTyping); }
	ScriptString& fpsText() { return Value<ScriptString>(PropOffsets_Console.fpsText); }
	int& numLines() { return Value<int>(PropOffsets_Console.numLines); }
	int& vtblOut() { return Value<int>(PropOffsets_Console.vtblOut); }
};
