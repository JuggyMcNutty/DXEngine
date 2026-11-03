#pragma once

#include "UActor.h"

class UPlayerPawn;

class UMenu : public UActor
{
public:
	using UActor::UActor;

	ScriptString& CenterString() { return Value<ScriptString>(PropOffsets_Menu.CenterString); }
	ScriptString& DisabledString() { return Value<ScriptString>(PropOffsets_Menu.DisabledString); }
	ScriptString& EnabledString() { return Value<ScriptString>(PropOffsets_Menu.EnabledString); }
	FixedArrayView<std::optional<std::string>, 24> HelpMessage() { return FixedArray<std::optional<std::string>, 24>(PropOffsets_Menu.HelpMessage); }
	ScriptString& LeftString() { return Value<ScriptString>(PropOffsets_Menu.LeftString); }
	int& MenuLength() { return Value<int>(PropOffsets_Menu.MenuLength); }
	FixedArrayView<std::optional<std::string>, 24> MenuList() { return FixedArray<std::optional<std::string>, 24>(PropOffsets_Menu.MenuList); }
	ScriptString& MenuTitle() { return Value<ScriptString>(PropOffsets_Menu.MenuTitle); }
	ScriptString& NoString() { return Value<ScriptString>(PropOffsets_Menu.NoString); }
	UMenu*& ParentMenu() { return Value<UMenu*>(PropOffsets_Menu.ParentMenu); }
	UPlayerPawn*& PlayerOwner() { return Value<UPlayerPawn*>(PropOffsets_Menu.PlayerOwner); }
	ScriptString& RightString() { return Value<ScriptString>(PropOffsets_Menu.RightString); }
	int& Selection() { return Value<int>(PropOffsets_Menu.Selection); }
	ScriptString& YesString() { return Value<ScriptString>(PropOffsets_Menu.YesString); }
	BitfieldBool bConfigChanged() { return BoolValue(PropOffsets_Menu.bConfigChanged); }
	BitfieldBool bExitAllMenus() { return BoolValue(PropOffsets_Menu.bExitAllMenus); }
};
