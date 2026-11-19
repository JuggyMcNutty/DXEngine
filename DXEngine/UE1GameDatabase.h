#pragma once

#include "Utils/File.h"

enum class KnownUE1Games
{
	UE1_GAME_NOT_FOUND,
	DEUS_EX_1002f,
	DEUS_EX_1112fm,
};

static const std::map<std::string, KnownUE1Games> SHA1Database = {
	// Deus Ex, v1002f
	{"9f923d667a396e8243028c14dc3f5e0a6db13d84", KnownUE1Games::DEUS_EX_1002f},

	// Deus Ex, v1112fm
	{"2a933e26aa9cfb33b37f78afe21434caa031f14a", KnownUE1Games::DEUS_EX_1112fm},
	// Deus Ex, v1112fm - Steam Version
	{"af951ddd35b38e8d9cc8501b8a50a02a3ab6cae7", KnownUE1Games::DEUS_EX_1112fm},
};

static const Array<std::string> knownUE1ExecutableNames = {
	"DeusEx.exe",
};

// 1112fm's packages in System, by the GUID in each one's header, as
// Package::GetGuidString writes it. An executable of no hash above is taken
// for 1112fm, with a warning, when Core.u, Engine.u and DeusEx.u each carry
// one of their file's GUIDs here: the owner's choice, where the original
// checks no hash at all (bugs.md D.89). The GUIDs are GOG's and Steam's
// copies', the same files; a copy that carries another is a row more. DXLauncher's
// src/core/gamedb.c holds the same rows, Port Ex Machina's dx.sh check the
// two together.
struct UE1PackageGuid
{
	const char* file;
	const char* guid;
};

static const UE1PackageGuid Packages1112fm[] = {
	{"Core.u", "BFC9A4E444E3B749B688A99529BD75C1"},
	{"Engine.u", "51EA2E914B9530035CB94784FF411102"},
	{"DeusEx.u", "CADECA6B4EB3B7FF1F0CB59221ECCC8E"},
};

struct UE1GameFound
{
	KnownUE1Games game = KnownUE1Games::UE1_GAME_NOT_FOUND;
	std::string executableName;	// "DeusEx.exe"
	bool byPackages = false;	// its executable's hash unknown, its packages 1112fm's
};

// The game in the folder, by its executable's hash or else its packages
UE1GameFound FindUE1GameInPath(const std::string& ue1_game_root_folder_path);

// Why System's packages are not 1112fm's (the first that is not there, no
// package, or another build's), or "" when they are
std::string WhyNot1112fmPackages(const fs::path& systemFolder);
