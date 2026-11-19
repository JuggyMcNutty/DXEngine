#pragma once

class GameApp
{
public:
	int main(Array<std::string> args);

	// An error before the error window can open shown in a message box: on
	// Windows, the engine's stderr going nowhere (MainGame.cpp)
	bool ErrorBox = false;

private:
	static void Relaunch(const Array<std::string>& args, const std::string& line);
};
