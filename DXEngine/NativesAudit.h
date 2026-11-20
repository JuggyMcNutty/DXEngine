#pragma once

#include <string>

// --audit-natives=<dir> (ENGINE.md, running it): the native functions the
// packages of a folder declare -- a mod's System -- each checked against the
// natives this engine registers as a call finds its handler (VM/Frame.cpp's
// CallNative: by its number, else by its class's and its own name), so that
// a launcher can say before a mod is played that a call stops it ("Unknown
// native function"). Its packages are read with the game's for what they
// import, no window, sound or map, but for Editor.u, the editor's own, whose
// natives no game calls; the answer on standard output:
//
//   native-audit: <dir>
//   damaged: <file>: <why>                      a package not read, passed over
//   lacks: <Package>.<Class>.<Function>[ (<number>)]   each once, in order
//   apart: <Package>: <n> the engine lacks, in a package no script of the folder's reaches
//   natives: <n> declared, <n> the engine has, <n> it lacks
//
// A native the engine lacks counts where a script can reach it: in a package
// that takes a game package's place, that another of the folder's builds on
// (imports), or that an ini of the folder names a class of. One in a package
// apart from all three is the mod's own DLLs' to load, and stops no game
// here: told on its own line, counted in none of the three numbers.
// the last line last, and only when the audit is whole. Returns the exit
// code: 0, or 1 with the error on standard error (no game, no folder).
namespace NativesAudit
{
	int Run(const std::string& folder);
}
