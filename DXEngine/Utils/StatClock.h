#pragma once

#include <chrono>

// A time summed for STAT's panels (RenderSubsystem::DrawStats), as the
// original's clock and unclock sum a counter's cycles: the seconds from its
// making to its end added to the figure it is given -- only while on, for a
// figure its panel alone shows -- or, split between stages, each stage's to
// its own figure as the next begins (Next).
class StatClock
{
public:
	explicit StatClock(double& sum, bool on = true) : Sum(on ? &sum : nullptr)
	{
		if (Sum)
			Start = std::chrono::steady_clock::now();
	}
	~StatClock()
	{
		if (Sum)
			*Sum += std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	}
	void Next(double& sum)
	{
		if (!Sum)
			return;
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		*Sum += std::chrono::duration<double>(now - Start).count();
		Start = now;
		Sum = &sum;
	}
	StatClock(const StatClock&) = delete;
	StatClock& operator=(const StatClock&) = delete;

private:
	double* Sum;
	std::chrono::steady_clock::time_point Start;
};

// A level's figures, STAT GLOBAL's and STAT GAME's line, as the original's
// level keeps them (ULevel::GetStats, Engine.dll 0x103a0740): each level's
// own, cleared as its tick begins (ULevel::InitStats 0x103a06b0, from
// ULevel::Tick 0x103a523b), the viewer's level's shown
struct LevelStats
{
	double Actor = 0.0; // its actors' tick (ULevel::Tick 0x103a54df-0x103a5922)
	double Path = 0.0;  // FindBestInventoryPath (APawn::execFindBestInventoryPath 0x103b986f)
	double See = 0.0;   // the pawns' sight and hearing (APawn::ShowSelf 0x103c0456, CheckEnemyVisible 0x103c02c6, AActor::CheckNoiseHearing 0x103bfb26)
	double Audio = 0.0; // the audio's update (UGameEngine::Draw 0x1038f5f8-0x1038f660)
	double Move = 0.0;  // the moves (ULevel::MoveActor 0x103992bc-0x10399f42)
	int Moves = 0;
	double Net = 0.0;   // its net tick, a server's or a client's (ULevel::TickNetServer, TickNetClient)
};

// The events' time, the outermost each, as the original's script clock
// counts it (UObject::ProcessEvent, Core.dll 0x1013f880-0x1013f9b0),
// cleared as any level's tick begins (ULevel::InitStats): CallEvent's
extern double ScriptStatTime;
extern int ScriptStatDepth;
