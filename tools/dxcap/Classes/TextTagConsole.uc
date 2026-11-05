//=============================================================================
// TextTagConsole: the text parser's names and colours -- the G, F, L, NOTE
// and GOAL tags' names, the DC, C and /C tags' colours, the FILE and EMAIL
// tags' fields -- read from texts of the console's own, whose tags no stock
// text holds in these forms (docs/NATIVES.md, what the player reads), each
// line starting "DXTEXTTAG:" (docs/DEVELOPMENT.md, scripted runs).
//
// 2 s into the run's first map, five texts, each given to a stock text's
// object, DeusExText.15_Datacube21, by its SetText, then opened by a
// DeusExTextParser of the console's own (SetPlayerName, OpenText from
// DeusExText, ProcessText until it gives false, CloseText, CriticalDelete).
// The original's OpenText copies the object's text and deletes the object,
// so each text loads it afresh, and the console never touches it after its
// OpenText; DXEngine's keeps it, the last text in it.
//
// Logged before the text is opened: the new parser's tag, GetColor, and its
// lastName, lastColor and defaultColor. Then each token: its tag (GetTag, by
// EDeusExTextTags' name), its text (GetText), GetName at a NOTE, G, F or L
// token alone (at any other the original's GetName fails an assertion,
// which stops the game), GetColor, the parser's lastName, lastColor and
// defaultColor, and at a FILE or EMAIL token the fields GetFileInfo or
// GetEmailInfo give; then the number of tokens and what the call that gave
// false left. A colour's alpha, the original's whatever byte was left
// there, is not logged.
//
// - names: a G, an F with its '=' after a word, an L, a G with no '=', an
//   F with nothing after its '=', a G, then a NOTE never closed;
// - goals: a G, a GOAL with a name, an L, a GOAL with none, an L, a GOAL
//   with its '=' after a word, then a NOTE never closed;
// - colours: a DC before the first paragraph, a C, a /C, a C with no '=', a
//   /C with numbers, a DC with its '=' after a word, a C of 300,-1,7;
// - files: a FILE, a FILE with no '=', a FILE with its '=' after a word and
//   spaces about its fields, a FILE of one field, an EMAIL with its '='
//   after a word, then a paragraph;
// - text: a G, then a text token, for Provoke.
//
// With Provoke=True, last: the text read again, and GetName asked at its
// text token, where the original's fails its assertion and stops the game.
// Then an exit. The player takes no damage (ReducedDamageType 'All').
//=============================================================================
class TextTagConsole extends Console;

var config bool Provoke;

var float RunTime;
var int Phase;
var string TagNames[31];

function string TagName(int Tag)
{
	if (Tag >= 0 && Tag < 31)
		return Tag $ " " $ TagNames[Tag];
	return string(Tag);
}

function string ColorOf(color C)
{
	return C.R $ "," $ C.G $ "," $ C.B;
}

// What the parser holds after a token, GetName asked at a NOTE, G, F or L
// token alone.
function string Held(DeusExTextParser Parser)
{
	local int Tag;
	local string Line;

	Tag = Parser.GetTag();
	Line = "tag " $ TagName(Tag);
	if (Tag == 3 || Tag == 25 || Tag == 26 || Tag == 27)
		Line = Line $ " GetName " $ Parser.GetName();
	return Line $ " GetColor " $ ColorOf(Parser.GetColor()) $ " lastName " $ Parser.lastName
		$ " lastColor " $ ColorOf(Parser.lastColor) $ " defaultColor " $ ColorOf(Parser.defaultColor);
}

// The stock text's object given the text; false if it does not load. The
// object is not kept: the original's OpenText deletes it.
function bool Give(string Body)
{
	local ExtString S;

	S = ExtString(DynamicLoadObject("DeusExText.15_Datacube21", class'ExtString'));
	if (S == None)
		return false;
	S.SetText(Body);
	return true;
}

// One text given, opened, read token by token and closed. With bAsk, GetName
// asked at the text's token 2 as well.
function ReadText(string Label, string Body, bool bAsk)
{
	local DeusExTextParser Parser;
	local bool bOpen;
	local int N, Tag;
	local string Line, A, B, C, D, E;

	if (!Give(Body))
	{
		Log("DXTEXTTAG: " $ Label $ " no DeusExText.15_Datacube21 to give the text to");
		return;
	}
	Parser = new(None) class'DeusExTextParser';
	Parser.SetPlayerName("JC Denton");
	Log("DXTEXTTAG: " $ Label $ " new parser: " $ Held(Parser));
	bOpen = Parser.OpenText('15_Datacube21', "DeusExText");
	Log("DXTEXTTAG: " $ Label $ " opened " $ bOpen);
	if (bOpen)
	{
		while (Parser.ProcessText())
		{
			N++;
			Tag = Parser.GetTag();
			Line = Label $ " token " $ N $ ": text '" $ Left(Parser.GetText(), 40) $ "' " $ Held(Parser);
			if (Tag == 1)
			{
				Parser.GetFileInfo(A, B);
				Line = Line $ " file '" $ A $ "' '" $ B $ "'";
			}
			else if (Tag == 2)
			{
				Parser.GetEmailInfo(A, B, C, D, E);
				Line = Line $ " email '" $ A $ "' '" $ B $ "' '" $ C $ "' '" $ D $ "' '" $ E $ "'";
			}
			Log("DXTEXTTAG: " $ Line);
			if (bAsk && N == 2)
			{
				Log("DXTEXTTAG: " $ Label $ " GetName at token 2, a text token, next");
				Log("DXTEXTTAG: " $ Label $ " GetName at token 2: " $ Parser.GetName());
			}
			if (N >= 40)
			{
				Log("DXTEXTTAG: " $ Label $ " stopped at 40 tokens");
				break;
			}
		}
		Log("DXTEXTTAG: " $ Label $ " " $ N $ " tokens, then IsEOF " $ Parser.IsEOF() $ " " $ Held(Parser));
		Parser.CloseText();
	}
	CriticalDelete(Parser);
}

function string NamesText()
{
	return "<P>a<G=Dx4Alpha>b<F x=Dx4Beta>c<L>d<G>e<F=>f<G=Dx4Gamma><NOTE>tail";
}

function string GoalsText()
{
	return "<P><G=Dx4Alpha>a<GOAL=Dx4Goal>hidden</GOAL>b<L>c<GOAL>gone</GOAL>d<L>e"
		$ "<GOAL id=Dx4Aim>x</GOAL>f<NOTE>tail";
}

function string ColoursText()
{
	return "<DC=10,20,30><P>a<C=40,50,60>b</C>c<C>d</C=70,80,90>e<DC x=1,2,3>f<C=300,-1,7>g";
}

function string FilesText()
{
	return "<FILE=one.pcx,First file><FILE><FILE note=two.pcx , Second ><FILE=three.pcx>"
		$ "<EMAIL x=Dx4Mail,Subject,From,To,CC><P>end";
}

function string PlainText()
{
	return "<P><G=Dx4Zeta>plain";
}

function ReadAll()
{
	ReadText("names", NamesText(), false);
	ReadText("goals", GoalsText(), false);
	ReadText("colours", ColoursText(), false);
	ReadText("files", FilesText(), false);
	ReadText("text", PlainText(), false);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 0 && RunTime > 2.0)
	{
		ReadAll();
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		if (Provoke)
			ReadText("provoke", PlainText(), true);
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 4.0)
	{
		Log("DXTEXTTAG: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

defaultproperties
{
	Provoke=False
	TagNames(0)="TT_Text"
	TagNames(1)="TT_File"
	TagNames(2)="TT_Email"
	TagNames(3)="TT_Note"
	TagNames(4)="TT_EndNote"
	TagNames(5)="TT_Goal"
	TagNames(6)="TT_EndGoal"
	TagNames(7)="TT_Comment"
	TagNames(8)="TT_EndComment"
	TagNames(9)="TT_PlayerName"
	TagNames(10)="TT_PlayerFirstName"
	TagNames(11)="TT_NewPage"
	TagNames(12)="TT_CenterText"
	TagNames(13)="TT_LeftJustify"
	TagNames(14)="TT_RightJustify"
	TagNames(15)="TT_DefaultColor"
	TagNames(16)="TT_TextColor"
	TagNames(17)="TT_RevertColor"
	TagNames(18)="TT_NewParagraph"
	TagNames(19)="TT_Bold"
	TagNames(20)="TT_EndBold"
	TagNames(21)="TT_Underline"
	TagNames(22)="TT_EndUnderline"
	TagNames(23)="TT_Italics"
	TagNames(24)="TT_EndItalics"
	TagNames(25)="TT_Graphic"
	TagNames(26)="TT_Font"
	TagNames(27)="TT_Label"
	TagNames(28)="TT_OpenBracket"
	TagNames(29)="TT_CloseBracket"
	TagNames(30)="TT_None"
}
