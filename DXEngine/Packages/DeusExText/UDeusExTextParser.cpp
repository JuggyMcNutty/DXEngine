
#include "Precomp.h"
#include "UDeusExTextParser.h"
#include "Packages/Extension/UExtString.h"
#include "Package/Package.h"
#include "Package/PackageManager.h"
#include "Engine.h"
#include "Utils/Logger.h"

// The 30 tag names, in the order of EDeusExTextTags; a tag is the first one
// its content starts with, case sensitive, so <LOG ERROR> is an L. Content
// that starts with none of them has no tag (TT_None).
static const char* const DeusExTextTagNames[] =
{
	"TEXT", "FILE", "EMAIL", "NOTE", "/NOTE", "GOAL", "/GOAL", "COMMENT",
	"/COMMENT", "PLAYERNAME", "PLAYERFIRSTNAME", "NP", "JC", "JL", "JR",
	"DC", "C", "/C", "P", "B", "/B", "U", "/U", "I", "/I", "G", "F", "L",
	"/<", "/>"
};

// A tag's fields: the text after its '=', split at commas and trimmed of
// spaces. No '=' means no fields, and a missing field is empty.
static Array<std::string> ParseFields(const std::string& content)
{
	Array<std::string> fields;
	if (content.empty() || content[0] != '=')
		return fields;

	size_t pos = 1;
	while (true)
	{
		size_t comma = content.find(',', pos);
		std::string field = content.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
		size_t begin = field.find_first_not_of(' ');
		if (begin == std::string::npos)
			field.clear();
		else
			field = field.substr(begin, field.find_last_not_of(' ') - begin + 1);
		fields.push_back(std::move(field));
		if (comma == std::string::npos)
			break;
		pos = comma + 1;
	}
	return fields;
}

bool UDXTextParser::OpenText(NameString textName, std::string textPackage)
{
	CloseText();
	if (textPackage.empty())
		textPackage = "DeusExText";
	// A package that is not there opens no text (false), not the game's end;
	// a None name opens none quietly, as the original's (DeusExText.dll
	// OpenText 0x10001287: DeusExText.None, found nothing), not the quotes
	if (textName.IsNone())
		textObject = nullptr;
	else try
	{
		textObject = UObject::Cast<UDXExtString>(engine->packages->GetPackage(textPackage)->GetUObject("ExtString", textName));
	}
	catch (const std::exception&)
	{
		LogMessage("Could not load package " + textPackage + " in DeusExTextParser.OpenText");
		textObject = nullptr;
	}
	Text() = textObject ? 1 : 0;
	// The original starts a text with no paragraph yet; the last tag, name,
	// colour, file and email stay from before.
	bParagraphStarted() = false;
	return textObject;
}

void UDXTextParser::CloseText()
{
	textObject = nullptr;
	Text() = 0;
	TextPos() = 0;
	TagEndPos() = 0;
}

bool UDXTextParser::ProcessText()
{
	// One token, as the original's ParseTextBlock (DeusExText.dll
	// 0x10001a20): a tag, or the text up to the next tag with each CR and LF
	// made a space and nothing trimmed, so a line break alone between two
	// tags is a token of two spaces. The text's first <P> is swallowed and
	// text before it dropped. Each call starts with no tag, and the text's
	// end ends a token as a tag does, so a text whose first paragraph never
	// starts gives a last token after its last tag, empty and with no tag
	// (TT_None). The call after the last token finds the text used up: an
	// empty token with no tag, false, and IsEOF true from then on (a
	// TextPos of -1, the original's textPos none).
	LastTag() = DeusExTextTags::TT_None;
	LastText().clear();
	if (!textObject || TextPos() < 0)
		return false;

	const std::string text = ScriptToLatin1(textObject->Text());
	size_t len = text.size();
	size_t pos = (size_t)TextPos();
	if (pos >= len)
	{
		TextPos() = -1;
		return false;
	}

	std::string value;
	bool gotText = false;
	while (pos < len)
	{
		if (text[pos] == '<')
		{
			if (gotText)
				break;
			// The token's tag is the last one a tag's name matched (a comment's
			// end tag, or the last tag a comment with none holds), and its text
			// the tag's own: a player's name, a bracket, else none.
			value += ParseTag(text, pos, LastTag());
			if (LastTag() != DeusExTextTags::TT_NewParagraph || bParagraphStarted())
				break;
			bParagraphStarted() = true;
		}
		else
		{
			if (bParagraphStarted())
			{
				char c = text[pos];
				value += (c == '\r' || c == '\n') ? ' ' : c;
				LastTag() = DeusExTextTags::TT_Text;
				gotText = true;
			}
			pos++;
		}
	}

	LastText() = Latin1ToScript(value);
	TextPos() = (int)pos;
	return true;
}

std::string UDXTextParser::ParseTag(const std::string& text, size_t& pos, DeusExTextTags& tag)
{
	// A tag runs from '<' to the first '>' (the original's ParseTag), and
	// its effects are the original's ParseToken. It sets the tag only when a
	// name matches: content that starts with none leaves it as it was, as
	// the original leaves its last tag. Gives the token's text.
	size_t start = pos + 1;
	size_t close = text.find('>', start);
	std::string content = close == std::string::npos ? text.substr(start) : text.substr(start, close - start);
	pos = close == std::string::npos ? text.size() : close + 1;

	int match = -1;
	for (int i = 0; i < 30; i++)
	{
		if (content.compare(0, strlen(DeusExTextTagNames[i]), DeusExTextTagNames[i]) == 0)
		{
			match = i;
			break;
		}
	}
	if (match < 0)
		return {};
	tag = (DeusExTextTags)match;
	std::string fields = content.substr(strlen(DeusExTextTagNames[match]));

	switch (tag)
	{
	case DeusExTextTags::TT_File:
		ParseFile(fields);
		break;
	case DeusExTextTags::TT_Email:
		ParseEmail(fields);
		break;
	case DeusExTextTags::TT_Note:
		FindEndTag(text, pos, DeusExTextTags::TT_EndNote, tag);
		break;
	case DeusExTextTags::TT_Goal:
		FindEndTag(text, pos, DeusExTextTags::TT_EndGoal, tag);
		break;
	case DeusExTextTags::TT_Comment:
		FindEndTag(text, pos, DeusExTextTags::TT_EndComment, tag);
		break;
	case DeusExTextTags::TT_PlayerName:
		return ScriptToLatin1(PlayerName());
	case DeusExTextTags::TT_PlayerFirstName:
		return ScriptToLatin1(PlayerFirstName());
	case DeusExTextTags::TT_DefaultColor:
		DefaultColor() = ParseColor(fields);
		LastColor() = DefaultColor();
		break;
	case DeusExTextTags::TT_TextColor:
		LastColor() = ParseColor(fields);
		break;
	case DeusExTextTags::TT_Graphic:
	case DeusExTextTags::TT_Font:
		if (!fields.empty() && fields[0] == '=')
			LastName() = NameString(fields.substr(1));
		break;
	case DeusExTextTags::TT_OpenBracket:
		return "<";
	case DeusExTextTags::TT_CloseBracket:
		return ">";
	default:
		break;
	}
	return {};
}

void UDXTextParser::FindEndTag(const std::string& text, size_t& pos, DeusExTextTags endTag, DeusExTextTags& tag)
{
	// The original's FindEndTag: NOTE, GOAL and COMMENT hide what they hold,
	// and the token's tag is the end tag's. The tags inside are parsed as
	// any, with their effects (a NOTE, GOAL or COMMENT among them reads on
	// to its own end tag), and the character after each is skipped, so an
	// end tag straight after another tag is missed. With no end tag the
	// token's tag is the last one a tag inside matched, the opening tag's
	// with none, and the rest of the text is hidden: the original's reading
	// steps past its copy's NUL there, a step for each comment open, and
	// stops at the first NUL it meets, a tag in that memory read as any.
	// This takes what lies past the end for NULs, by choice, which gives
	// 09_EmailMenu_ShipOps, the game's one comment left open, the listing the
	// original gives in play: one email, its fields empty (docs/NATIVES.md,
	// what the player reads).
	size_t len = text.size();
	if (pos >= len)
		return;
	while (true)
	{
		if (text[pos] == '<')
		{
			ParseTag(text, pos, tag);
			if (tag == endTag)
				return;
		}
		pos++;
		if (pos >= len)
			return;
	}
}

void UDXTextParser::ParseFile(const std::string& fields)
{
	// FILE=name,description; the last ones read stay, whatever the token.
	Array<std::string> f = ParseFields(fields);
	LastFileName() = f.size() > 0 ? Latin1ToScript(f[0]) : ScriptString();
	LastFileDescription() = f.size() > 1 ? Latin1ToScript(f[1]) : ScriptString();
}

void UDXTextParser::ParseEmail(const std::string& fields)
{
	// EMAIL=name,subject,from,to,cc; the last ones read stay, whatever the
	// token.
	Array<std::string> f = ParseFields(fields);
	LastEmailName() = f.size() > 0 ? Latin1ToScript(f[0]) : ScriptString();
	LastEmailSubject() = f.size() > 1 ? Latin1ToScript(f[1]) : ScriptString();
	LastEmailFrom() = f.size() > 2 ? Latin1ToScript(f[2]) : ScriptString();
	LastEmailTo() = f.size() > 3 ? Latin1ToScript(f[3]) : ScriptString();
	LastEmailCC() = f.size() > 4 ? Latin1ToScript(f[4]) : ScriptString();
}

Color UDXTextParser::ParseColor(const std::string& fields)
{
	// The three numbers after '=', or black without one. The original's
	// alpha is whatever byte was left there; ours is opaque.
	Array<std::string> f = ParseFields(fields);
	Color color;
	color.R = f.size() > 0 ? (uint8_t)std::atoi(f[0].c_str()) : 0;
	color.G = f.size() > 1 ? (uint8_t)std::atoi(f[1].c_str()) : 0;
	color.B = f.size() > 2 ? (uint8_t)std::atoi(f[2].c_str()) : 0;
	color.A = 255;
	return color;
}

bool UDXTextParser::IsEOF()
{
	// The original's (0x100018c0): no text, or a call has found it used up
	return !textObject || TextPos() < 0;
}

std::string UDXTextParser::GetText()
{
	return ScriptToLatin1(LastText());
}

void UDXTextParser::GotoLabel(const std::string& label)
{
	// The original's does nothing.
}

uint8_t UDXTextParser::GetTag()
{
	return static_cast<uint8_t>(LastTag());
}

NameString UDXTextParser::GetName()
{
	return LastName();
}

Color UDXTextParser::GetColor()
{
	// After /C, the colour the parser was made with: black, as nothing
	// sets it.
	if (LastTag() == DeusExTextTags::TT_RevertColor)
	{
		Color black;
		black.R = 0;
		black.G = 0;
		black.B = 0;
		black.A = 255;
		return black;
	}
	return LastColor();
}

void UDXTextParser::GetEmailInfo(std::string& name, std::string& subject, std::string& from, std::string& to, std::string& cc)
{
	// The last ones read, whatever the token, as the original's.
	name = ScriptToLatin1(LastEmailName());
	subject = ScriptToLatin1(LastEmailSubject());
	from = ScriptToLatin1(LastEmailFrom());
	to = ScriptToLatin1(LastEmailTo());
	cc = ScriptToLatin1(LastEmailCC());
}

void UDXTextParser::GetFileInfo(std::string& fileName, std::string& fileDescription)
{
	fileName = ScriptToLatin1(LastFileName());
	fileDescription = ScriptToLatin1(LastFileDescription());
}

void UDXTextParser::SetPlayerName(const std::string& newPlayerName)
{
	PlayerName() = Latin1ToScript(newPlayerName);
	PlayerFirstName() = Latin1ToScript(newPlayerName.substr(0, newPlayerName.find(' ')));
}

void UDXTextParser::Mark(GCMarker& marker)
{
	UObject::Mark(marker);
	marker.SetField("textObject");
	marker.Mark(textObject);
}
