#pragma once

#include "Packages/Core/UObject.h"

class UDXExtString;

enum class DeusExTextTags : uint8_t
{
	TT_Text,
	TT_File,
	TT_Email,
	TT_Note,
	TT_EndNote,
	TT_Goal,
	TT_EndGoal,
	TT_Comment,
	TT_EndComment,
	TT_PlayerName,
	TT_PlayerFirstName,
	TT_NewPage,
	TT_CenterText,
	TT_LeftJustify,
	TT_RightJustify,
	TT_DefaultColor,
	TT_TextColor,
	TT_RevertColor,
	TT_NewParagraph,
	TT_Bold,
	TT_EndBold,
	TT_Underline,
	TT_EndUnderline,
	TT_Italics,
	TT_EndItalics,
	TT_Graphic,
	TT_Font,
	TT_Label,
	TT_OpenBracket,
	TT_CloseBracket,
	TT_None
};

class UDXTextParser : public UObject
{
public:
	void Mark(GCMarker& marker) override;
	using UObject::UObject;

	int& Text() { return Value<int>(PropOffsets_DeusExTextParser.Text); }
	int& TextPos() { return Value<int>(PropOffsets_DeusExTextParser.textPos); }
	int& TagEndPos() { return Value<int>(PropOffsets_DeusExTextParser.tagEndPos); }

	ScriptString& LastText() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastText); }
	DeusExTextTags& LastTag() { return Value<DeusExTextTags>(PropOffsets_DeusExTextParser.lastTag); }
	NameString& LastName() { return Value<NameString>(PropOffsets_DeusExTextParser.lastName); }
	Color& LastColor() { return Value<Color>(PropOffsets_DeusExTextParser.lastColor); }
	Color& DefaultColor() { return Value<Color>(PropOffsets_DeusExTextParser.DefaultColor); }
	BitfieldBool bParagraphStarted() { return BoolValue(PropOffsets_DeusExTextParser.bParagraphStarted); }
	ScriptString& PlayerName() { return Value<ScriptString>(PropOffsets_DeusExTextParser.PlayerName); }
	ScriptString& PlayerFirstName() { return Value<ScriptString>(PropOffsets_DeusExTextParser.playerFirstName); }

	ScriptString& LastEmailName() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastEmailName); }
	ScriptString& LastEmailSubject() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastEmailSubject); }
	ScriptString& LastEmailFrom() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastEmailFrom); }
	ScriptString& LastEmailTo() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastEmailTo); }
	ScriptString& LastEmailCC() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastEmailCC); }
	ScriptString& LastFileName() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastFileName); }
	ScriptString& LastFileDescription() { return Value<ScriptString>(PropOffsets_DeusExTextParser.lastFileDescription); }

	bool OpenText(NameString textName, std::string textPackage = "");
	void CloseText();
	bool ProcessText();
	bool IsEOF();
	std::string GetText();
	void GotoLabel(const std::string& label);
	uint8_t GetTag();
	NameString GetName();
	Color GetColor();
	void GetEmailInfo(std::string& name, std::string& subject, std::string& from, std::string& to, std::string& cc);
	void GetFileInfo(std::string& fileName, std::string& fileDescription);
	void SetPlayerName(const std::string& newPlayerName);

private:
	UDXExtString* textObject = nullptr;

	std::string ParseTag(const std::string& text, size_t& pos, DeusExTextTags& tag);
	void FindEndTag(const std::string& text, size_t& pos, DeusExTextTags endTag, DeusExTextTags& tag);
	void ParseFile(const std::string& fields);
	void ParseEmail(const std::string& fields);
	static Color ParseColor(const std::string& fields);
};
