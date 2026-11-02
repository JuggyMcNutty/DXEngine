//=============================================================================
// ShipOpsConsole: the email account whose list leaves its comment open --
// 09_EmailMenu_ShipOps, a COMMENT tag whose end is a second COMMENT tag,
// then a DC tag, an EMAIL tag with no fields and the text's end
// (docs/NATIVES.md, what the player reads) -- read as the game reads an
// account's list, each line starting "DXSHIPOPS:" (docs/DEVELOPMENT.md,
// scripted runs).
//
// A round of reads, 2 s into the run's first map: the list opened by a
// DeusExTextParser of the console's own as ComputerUIWindow's
// ProcessDeusExText opens one (SetPlayerName, OpenText from DeusExText,
// ProcessText until it gives false, CloseText, CriticalDelete), three times;
// then six controls, 09_EmailMenu_KZhao (two emails; its copy as long as
// ShipOps's within a character), 09_EmailMenu_SDowd (one email),
// 09_Datacube13 (a comment closed), 00_Datacube01 and 01_Book08 (a <P> and a
// <B> straight after a text: GetText at the tag), 04_EmailMenu_klloyd (its
// first paragraph never started, it ends after its last tag in a line
// break); then ShipOps's list once more. Each token is logged: its tag
// (GetTag, by EDeusExTextTags' name), where the parse stands after it, in
// characters from the text's start (the original's text and textPos are
// pointers into its copy, two bytes a character; DXEngine's text is 1 and
// its textPos a character's index, -1 once the text is used up), its text
// (GetText, at most 60 characters) and, at an EMAIL or a FILE tag, the
// fields GetEmailInfo or GetFileInfo give; then the number of tokens, and
// what the call that gave false left: its tag, where the parse stands, its
// text and IsEOF. The original's copy's address (its text) is logged on a
// line of its own.
//
// Then 09_NYC_Ship is opened (by way of Via first, DXCAP_CONFIG "Via=<map>",
// a round there 4 s in), and a round there 4 s in. Then the ship's computer
// that holds the ShipOps account is opened as the player opens one
// (InvokeComputerScreen), logged in to as ShipOps through its login screen
// (the account's name and password in its fields, ProcessLogin), and its
// email screen read: the user, emailIndex, each row of its list, the
// header fields and its text window's text (the screen's "No
// Email Today" when the list gives no email), then the list read once by
// the console's own parser beside it; a marked shot; the computer closed.
// Rounds times (3 by default), then once more logged in as the computer's
// first user, root, whose list DeusExText has none of: the screen's own
// "No Email Today".
//
// With Provoke=True, last: two reads of ShipOps's list, each straight after
// two strings of 100 characters were freed, the first freed ending in <B> at
// its character 97. The original's allocator gives a pool's 224-byte blocks
// out last freed first, the text it loads taking one and the copy it reads
// the next: where the copy takes the first string's block, the original's
// reading past the text's end meets that tag. Then an exit. The player takes
// no damage (ReducedDamageType 'All').
//=============================================================================
class ShipOpsConsole extends Console;

var config string Via;
var config int Rounds;
var config bool Provoke;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var int RoundNo;
var bool bViaDone;
var bool bOpened;
var int ShotsTaken;
var Computers Comp;
var string User;
var string Bait1, Bait2;
var string TagNames[31];

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;

	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

function string TagName(int Tag)
{
	if (Tag >= 0 && Tag < 31)
		return Tag $ " " $ TagNames[Tag];
	return string(Tag);
}

// Where the parse stands, in characters from the text's start.
function string PosOf(DeusExTextParser Parser)
{
	if (Parser.text == 1)
	{
		if (Parser.textPos < 0)
			return "none";
		return string(Parser.textPos);
	}
	if (Parser.textPos == 0)
		return "none";
	return string((Parser.textPos - Parser.text) / 2);
}

// One text opened, read token by token and closed, as the game's computer
// screens read one.
function ParseText(string Label, name TextName)
{
	local DeusExTextParser Parser;
	local bool bOpen;
	local int N, Tag;
	local string Line, A, B, C, D, E;

	Parser = new(None) class'DeusExTextParser';
	Parser.SetPlayerName("JC Denton");
	bOpen = Parser.OpenText(TextName, "DeusExText");
	Log("DXSHIPOPS: " $ Label $ " " $ TextName $ " opened " $ bOpen $ " at " $ CapTime);
	if (bOpen)
	{
		if (Parser.text != 1)
			Log("DXSHIPOPS: " $ Label $ " " $ TextName $ " copy at " $ Parser.text);
		while (Parser.ProcessText())
		{
			N++;
			Tag = Parser.GetTag();
			Line = Label $ " " $ TextName $ " token " $ N $ ": tag " $ TagName(Tag) $ " at " $ PosOf(Parser)
				$ " text '" $ Left(Parser.GetText(), 60) $ "'";
			if (Tag == 2)
			{
				Parser.GetEmailInfo(A, B, C, D, E);
				Line = Line $ " email '" $ A $ "' '" $ B $ "' '" $ C $ "' '" $ D $ "' '" $ E $ "'";
			}
			else if (Tag == 1)
			{
				Parser.GetFileInfo(A, B);
				Line = Line $ " file '" $ A $ "' '" $ B $ "'";
			}
			Log("DXSHIPOPS: " $ Line);
			if (N >= 40)
			{
				Log("DXSHIPOPS: " $ Label $ " " $ TextName $ " stopped at 40 tokens");
				break;
			}
		}
		Log("DXSHIPOPS: " $ Label $ " " $ TextName $ " " $ N $ " tokens, then tag " $ TagName(Parser.GetTag())
			$ " at " $ PosOf(Parser) $ " text '" $ Left(Parser.GetText(), 60) $ "', IsEOF " $ Parser.IsEOF());
		Parser.CloseText();
	}
	CriticalDelete(Parser);
}

function ReadRound(string Label)
{
	ParseText(Label, '09_EmailMenu_ShipOps');
	ParseText(Label, '09_EmailMenu_ShipOps');
	ParseText(Label, '09_EmailMenu_ShipOps');
	ParseText(Label, '09_EmailMenu_KZhao');
	ParseText(Label, '09_EmailMenu_SDowd');
	ParseText(Label, '09_Datacube13');
	ParseText(Label, '00_Datacube01');
	ParseText(Label, '01_Book08');
	ParseText(Label, '04_EmailMenu_klloyd');
	ParseText(Label, '09_EmailMenu_ShipOps');
}

// Two strings of 100 characters, the first ending in <B> at its character
// 97; built in a function of their own, so that what building them frees is
// freed before them.
function MakeBait()
{
	local string S;
	local int i;

	for (i = 0; i < 97; i++)
		S = S $ "x";
	Bait1 = S $ "<B>";
	Bait2 = S $ "yyy";
}

// The first string freed, then the second, then the list read: nothing
// logged in between.
function ProvokeRead(string Label)
{
	Bait1 = "";
	Bait2 = "";
	ParseText(Label, '09_EmailMenu_ShipOps');
}

// The ship's computer that holds the ShipOps account.
function FindComputer(DeusExPlayer P)
{
	local Computers C;
	local int i;

	foreach P.AllActors(class'Computers', C)
	{
		for (i = 0; i < C.NumUsers(); i++)
		{
			if (C.GetUserName(i) ~= "ShipOps")
				Comp = C;
		}
		if (Comp != None)
			break;
	}
	if (Comp == None)
		Log("DXSHIPOPS: no computer holds ShipOps");
	else
		Log("DXSHIPOPS: ShipOps is on " $ Comp.Name $ ", " $ Comp.NumUsers() $ " users, the first " $ Comp.GetUserName(0));
}

function NetworkTerminal Terminal(DeusExPlayer P)
{
	return NetworkTerminal(DeusExRootWindow(P.rootWindow).GetTopWindow());
}

function OpenComputer(DeusExPlayer P)
{
	Comp.bLockedOut = false;
	P.InvokeComputerScreen(Comp, 0, P.Level.TimeSeconds);
	Log("DXSHIPOPS: opened " $ Comp.Name $ " at " $ CapTime);
}

// The account's name and password in the login screen's fields, and its
// login, as the player logs in.
function LogIn(DeusExPlayer P)
{
	local NetworkTerminal Term;
	local ComputerScreenLogin Login;
	local int i, Index;

	Term = Terminal(P);
	if (Term == None)
	{
		Log("DXSHIPOPS: no terminal to log in to");
		return;
	}
	Login = ComputerScreenLogin(Term.winComputer);
	if (Login == None)
	{
		Log("DXSHIPOPS: no login screen: " $ Term.winComputer);
		return;
	}
	Index = -1;
	for (i = 0; i < Comp.NumUsers(); i++)
		if (Index < 0 && Comp.GetUserName(i) ~= User)
			Index = i;
	if (Index < 0)
	{
		Log("DXSHIPOPS: no user " $ User);
		return;
	}
	Login.editUserName.SetText(Comp.GetUserName(Index));
	Login.editPassword.SetText(Comp.GetPassword(Index));
	Log("DXSHIPOPS: logging in as " $ Comp.GetUserName(Index));
	Login.ProcessLogin();
}

// What the email screen lists, as the player reads it.
function ReadScreen(DeusExPlayer P, string Label)
{
	local NetworkTerminal Term;
	local ComputerScreenEmail Screen;
	local int i, Row;

	Term = Terminal(P);
	if (Term == None)
	{
		Log("DXSHIPOPS: " $ Label $ " no terminal");
		return;
	}
	Screen = ComputerScreenEmail(Term.winComputer);
	if (Screen == None)
	{
		Log("DXSHIPOPS: " $ Label $ " no email screen: " $ Term.winComputer);
		return;
	}
	// Its emailInfo, a struct of 520 bytes, is out of a context expression's
	// reach (255 at most); the rows and the header give what it holds.
	Log("DXSHIPOPS: " $ Label $ " user " $ Term.GetUserName() $ ", emailIndex " $ Screen.emailIndex
		$ ", rows " $ Screen.lstEmail.GetNumRows());
	for (i = 0; i < Screen.lstEmail.GetNumRows(); i++)
	{
		Row = Screen.lstEmail.IndexToRowId(i);
		Log("DXSHIPOPS: " $ Label $ " row " $ i $ ": '" $ Screen.lstEmail.GetField(Row, 0) $ "' '"
			$ Screen.lstEmail.GetField(Row, 1) $ "' '" $ Screen.lstEmail.GetField(Row, 2) $ "'");
	}
	Log("DXSHIPOPS: " $ Label $ " header from '" $ Screen.winEmailFrom.GetText() $ "' to '"
		$ Screen.winEmailTo.GetText() $ "' subject '" $ Screen.winEmailSubject.GetText() $ "'");
	Log("DXSHIPOPS: " $ Label $ " text '" $ class'WindowDump'.static.Clean(Left(Screen.winEmail.GetText(), 80)) $ "'");
}

function CloseComputer(DeusExPlayer P)
{
	local NetworkTerminal Term;

	Term = Terminal(P);
	if (Term != None)
		Term.CloseScreen("EXIT");
	else
		Log("DXSHIPOPS: no terminal to close");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXSHIPOPS: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
		bOpened = false;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	// A shot's mark up for 0.8 s before it.
	if (bMarking)
	{
		if (StepTime > 0.8)
		{
			Log("DXSHIPOPS: shot " $ ShotsTaken $ " at " $ CapTime);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			StepTime = 0;
		}
		return;
	}

	if (Phase == 0)
	{
		if (MapTime > 2.0)
		{
			ReadRound("start");
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		// To the ship, by way of Via; a dxe run started on the ship stays.
		if (M ~= "09_NYC_Ship")
		{
			if (MapTime > 4.0)
			{
				ReadRound("ship");
				Phase = 2;
				StepTime = 0;
			}
		}
		else if (Via != "" && !bViaDone && M ~= Via)
		{
			if (MapTime > 4.0)
			{
				ReadRound("via");
				bViaDone = true;
			}
		}
		else if (MapTime > 3.0 && !bOpened)
		{
			if (Via != "" && !bViaDone)
			{
				Log("DXSHIPOPS: opening " $ Via);
				P.ConsoleCommand("open " $ Via);
			}
			else
			{
				Log("DXSHIPOPS: opening 09_NYC_Ship");
				P.ConsoleCommand("open 09_NYC_Ship");
			}
			bOpened = true;
		}
	}
	else if (DXP == None)
	{
		Log("DXSHIPOPS: no Deus Ex player on the ship, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		FindComputer(DXP);
		if (Comp == None)
		{
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		RoundNo = 1;
		User = "ShipOps";
		Phase = 3;
		StepTime = 0;
	}
	else if (Phase == 3 && StepTime > 1.0)
	{
		OpenComputer(DXP);
		Phase = 4;
		StepTime = 0;
	}
	else if (Phase == 4 && StepTime > 0.5)
	{
		LogIn(DXP);
		Phase = 5;
		StepTime = 0;
	}
	else if (Phase == 5 && StepTime > 0.5)
	{
		ReadScreen(DXP, "screen " $ RoundNo $ " " $ User);
		ParseText("screen " $ RoundNo $ " read", '09_EmailMenu_ShipOps');
		MarkShot = ShotsTaken;
		bMarking = true;
		Phase = 6;
		StepTime = 0;
	}
	else if (Phase == 6 && StepTime > 0.5)
	{
		CloseComputer(DXP);
		RoundNo++;
		if (RoundNo <= Rounds)
			Phase = 3;
		else if (RoundNo == Rounds + 1)
		{
			// The control: the computer's first user, whose list is none.
			User = Comp.GetUserName(0);
			Phase = 3;
		}
		else
			Phase = 7;
		StepTime = 0;
	}
	else if (Phase == 7 && StepTime > 1.0)
	{
		if (Provoke)
		{
			MakeBait();
			ProvokeRead("provoke 1");
			MakeBait();
			ProvokeRead("provoke 2");
		}
		Phase = 8;
		StepTime = 0;
	}
	else if (Phase == 8 && StepTime > 1.0)
	{
		Log("DXSHIPOPS: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

defaultproperties
{
	Rounds=3
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
