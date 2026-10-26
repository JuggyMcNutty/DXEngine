//=============================================================================
// ConfigProbe: a class with a config file of its own, DXCapConfig.ini, as a
// mod's class has (DXMTL.ini): ConfigConsole reads and writes it.
//=============================================================================
class ConfigProbe extends Object config(DXCapConfig);

var config int Runs;
// Written by ConfigConsole for the config types the original writes
// (bugs.md E5a.18): a struct as its members' text, a class as Class'Path',
// a static array's elements as Words[0] and Words[1]
var config vector Spot;
var config class<Actor> Kind;
var config string Words[2];
