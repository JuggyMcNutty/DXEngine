//=============================================================================
// IniEdgeProbe: a class with a config file of its own, DXCapEdge.ini, which
// IniEdgeConsole reads and writes back, the file's edges written by hand
// (bugs.md PKG.25), and a float the file holds to six decimals (PKG.27).
//=============================================================================
class IniEdgeProbe extends Object config(DXCapEdge);

var config string Word;
var config int Count;
var config float Ratio;
