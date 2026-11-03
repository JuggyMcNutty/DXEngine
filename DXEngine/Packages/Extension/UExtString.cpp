
#include "Precomp.h"
#include "UExtString.h"
#include "Utils/Logger.h"

void UDXExtString::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	std::string text = stream->ReadString();
	if (!Text().empty())
		LogMessage("ExtString already has text content in its Text property!");
	Text() = Latin1ToScript(text);
	stream->ThrowIfNotEnd();
}


// The natives are the original's (Extension.dll; extension-dll.md, small):
// the text in parts of 239 characters, GetFirstTextPart giving the first and
// GetNextTextPart each next one, and GetTextPart at most 240 from a place.

void UDXExtString::AppendText(const std::string& newText)
{
	Text() += Latin1ToScript(newText);
}

int UDXExtString::GetFirstTextPart(std::string& outText)
{
	int count = std::min((int)Text().size(), 239);
	outText = ScriptToLatin1(Text().substr(0, count));
	SpeechPage() = 0;
	return count;
}

// The next part, or 0 once none is left, the argument left as it was; but
// for a text a multiple of 239 long the first call past its end empties it
// (Extension.dll 0x10044ad0).
int UDXExtString::GetNextTextPart(std::string& outText)
{
	int page = ++SpeechPage();
	int len = (int)Text().size();
	if (page > len / 239)
		return 0;
	int count = std::min(len - 239 * page, 239);
	outText = ScriptToLatin1(Text().substr(239 * page, count));
	return count;
}

ScriptString& UDXExtString::GetText()
{
	return Text();
}

int UDXExtString::GetTextLength()
{
	return (int)Text().length();
}

// At most 240 characters from startPos, none from a negative one.
int UDXExtString::GetTextPart(int startPos, int count, std::string& outText)
{
	int len = (int)Text().size();
	if (startPos < 0)
		count = 0;
	count = std::min(count, len - startPos);
	count = std::clamp(count, 0, 240);
	outText = count > 0 ? ScriptToLatin1(Text().substr(startPos, count)) : std::string();
	return count;
}

void UDXExtString::SetText(const std::string& newText)
{
	Text() = Latin1ToScript(newText);
}
