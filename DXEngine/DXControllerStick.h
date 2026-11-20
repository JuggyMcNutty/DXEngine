#pragma once

#include <algorithm>
#include <cmath>

// The response of a stick and of a trigger in the pad's DXController mode
// (DXControllerPad), as the mod's documentation gives it: a stick's distance
// from its centre in SDL's units (0 to 32767 at full travel), a radial dead
// zone, the rest of the travel rescaled to 0..1 and shaped by a curve, then a
// scale. Kept apart from the engine so that a small program can check it.

enum class StickCurve { Linear, Power, Expo, Sigmoid };

struct StickResponse
{
	static constexpr float FullTravel = 32767.0f;

	int Deadzone = 3500;               // SDL's units, 0 to 32766
	StickCurve Curve = StickCurve::Linear;
	float Power = 2.0f;                // 0.1 to 10
	float Expo = 0.5f;                 // 0 to 1
	float Steepness = 8.0f;            // 1 to 12
	float Midpoint = 0.7f;             // 0.15 to 0.85
	float Strength = 0.9f;             // 0 to 1
	float Scale = 1.0f;                // the right stick's StickScaleRight, 0.1 to 1

	// The curve over the travel past the dead zone, u 0..1: each passes
	// through 0 and 1. Power is u to the power; expo blends u with u cubed;
	// sigmoid is a logistic of the steepness about the midpoint, moved and
	// stretched to pass through 0 and 1, blended with u by the strength.
	float Shape(float u) const
	{
		u = std::clamp(u, 0.0f, 1.0f);
		switch (Curve)
		{
		case StickCurve::Power:
			return u > 0.0f ? std::pow(u, Power) : 0.0f;
		case StickCurve::Expo:
			return (1.0f - Expo) * u + Expo * u * u * u;
		case StickCurve::Sigmoid:
		{
			auto logistic = [&](float x) { return 1.0f / (1.0f + std::exp(-Steepness * (x - Midpoint))); };
			const float low = logistic(0.0f), high = logistic(1.0f);
			const float s = high > low ? (logistic(u) - low) / (high - low) : u;
			return (1.0f - Strength) * u + Strength * s;
		}
		default:
			return u;
		}
	}

	// A distance from the centre in SDL's units to the stick's output, 0..1:
	// nothing within the dead zone, its edge 0 and full travel 1, a diagonal
	// past full travel held at 1
	float Magnitude(float raw) const
	{
		const float deadzone = (float)std::clamp(Deadzone, 0, 32766);
		if (!(raw > deadzone))
			return 0.0f;
		const float u = std::min((raw - deadzone) / (FullTravel - deadzone), 1.0f);
		return std::clamp(Shape(u) * Scale, 0.0f, 1.0f);
	}

	// A stick's two axes in SDL's units (right and down positive) to the
	// engine's, -1000..1000 with up positive: the output along the stick's
	// own direction, each axis held within its range
	void Axes(float rawX, float rawY, float& outX, float& outY) const
	{
		const float raw = std::sqrt(rawX * rawX + rawY * rawY);
		const float magnitude = Magnitude(raw);
		if (magnitude <= 0.0f)
		{
			outX = outY = 0.0f;
			return;
		}
		outX = std::clamp(rawX / raw * magnitude * 1000.0f, -1000.0f, 1000.0f);
		outY = std::clamp(-rawY / raw * magnitude * 1000.0f, -1000.0f, 1000.0f);
	}
};

// A trigger in SDL's units (0 to 32767) to the engine's 0..1000: nothing at
// or below the threshold (0 to 255, a byte's range over the trigger's
// travel), a straight line from there to full travel
inline float TriggerResponse(float raw, int threshold)
{
	const float t = (float)std::clamp(threshold, 0, 255) * StickResponse::FullTravel / 255.0f;
	if (!(raw > t) || t >= StickResponse::FullTravel)
		return 0.0f;
	return std::clamp((raw - t) / (StickResponse::FullTravel - t) * 1000.0f, 0.0f, 1000.0f);
}
