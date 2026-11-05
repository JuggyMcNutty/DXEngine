//=============================================================================
// CapStateObject: StateObjectConsole's object, made by new: a function of
// its own and the same one in its state Foo, each saying whose it is.
//=============================================================================
class CapStateObject extends Object;

function string Which()
{
	return "the class's";
}

state Foo
{
	function string Which()
	{
		return "Foo's";
	}
}
