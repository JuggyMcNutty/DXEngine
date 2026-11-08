#pragma once

#include "vec.h"
#include <algorithm>
#include <cmath>

// Engine.dll's FGetHSV, the colour Deus Ex's lighting works in
// (dx-reverse-info/render-dll.md, light maps): the value makes the
// brightness, 0.7 b / (sqrt(b) + 0.01) of b = 1.4 value / 255, at most 1; the
// hue is two neighbouring primaries mixed, 85 steps apart round the wheel;
// the saturation mixes the hue toward white, 255 all white.
inline vec3 FGetHSV(uint8_t hue, uint8_t saturation, uint8_t value)
{
	float b = value * (1.4f / 255.0f);
	float brightness = std::clamp(0.7f / (std::sqrt(b) + 0.01f) * b, 0.0f, 1.0f);
	vec3 h;
	if (hue < 86)
		h = vec3((85 - hue) * (1.0f / 85.0f), hue * (1.0f / 85.0f), 0.0f);
	else if (hue < 171)
		h = vec3(0.0f, (170 - hue) * (1.0f / 85.0f), (hue - 85) * (1.0f / 85.0f));
	else
		h = vec3((hue - 170) * (1.0f / 85.0f), 0.0f, (255 - hue) * (1.0f / 84.0f));
	float s = saturation * (1.0f / 255.0f);
	return (h + (vec3(1.0f) - h) * s) * brightness;
}
