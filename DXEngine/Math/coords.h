#pragma once

#include "Math/vec.h"
#include "Math/mat.h"
#include "Math/rotator.h"
#include "Math/scale.h"

class Coords
{
public:
	vec3 Origin;
	vec3 XAxis;
	vec3 YAxis;
	vec3 ZAxis;

	Coords Inverse() const;
	Coords Transpose() const;

	static Coords Identity();
	static Coords Location(const vec3& origin);

	static Coords Rotation(const Rotator& rotator);
	// The rotation of these axes (the original's FCoords::OrthoRotation)
	Rotator OrthoRotation() const;
	static Coords InverseRotation(const Rotator& rotator);
	// The original's sine table (GMath's) at an angle, 16384 steps round the
	// turn, an angle's low 2 bits dropped; a cosine is the table a quarter
	// turn on
	static float SinTab(int angle);

	static Coords ViewToRenderDev();

	void GetAxes(vec3& X, vec3& Y, vec3& Z) const;
	void GetUnAxes(vec3& X, vec3& Y, vec3& Z) const;

	Coords& operator *=(const Coords& rhs)
	{
		Origin *= rhs.Origin;
		XAxis = { dot(XAxis, rhs.XAxis), dot(XAxis, rhs.YAxis), dot(XAxis, rhs.ZAxis) };
		YAxis = { dot(YAxis, rhs.XAxis), dot(YAxis, rhs.YAxis), dot(YAxis, rhs.ZAxis) };
		ZAxis = { dot(ZAxis, rhs.XAxis), dot(ZAxis, rhs.YAxis), dot(ZAxis, rhs.ZAxis) };
		return *this;
	}

	bool operator==(const Coords& rhs) const
	{
		return Origin == rhs.Origin && XAxis == rhs.XAxis && YAxis == rhs.YAxis && ZAxis == rhs.ZAxis;
	}

	mat4 ToMatrix() const;
	static Coords FromMatrix(const mat4& m);
};

inline vec3 operator*(const Coords& coords, const vec3& v)
{
	vec3 p = v - coords.Origin;
	return { dot(p, coords.XAxis), dot(p, coords.YAxis), dot(p, coords.ZAxis) };
}

inline Coords operator*(const Coords& a, const Coords& b)
{
	Coords result;
	result.Origin = a * b.Origin;
	result.XAxis = { dot(a.XAxis, b.XAxis), dot(a.XAxis, b.YAxis), dot(a.XAxis, b.ZAxis) };
	result.YAxis = { dot(a.YAxis, b.XAxis), dot(a.YAxis, b.YAxis), dot(a.YAxis, b.ZAxis) };
	result.ZAxis = { dot(a.ZAxis, b.XAxis), dot(a.ZAxis, b.YAxis), dot(a.ZAxis, b.ZAxis) };
	return result;
}

inline Coords Coords::Inverse() const
{
	float det =
		XAxis.x * (YAxis.y * ZAxis.z - YAxis.z * ZAxis.y) +
		XAxis.y * (YAxis.z * ZAxis.x - YAxis.x * ZAxis.z) +
		XAxis.z * (YAxis.x * ZAxis.y - YAxis.y * ZAxis.x);
	float invdet = 1.0f / det;

	Coords result;
	result.Origin =
	{
		-dot(Origin, XAxis),
		-dot(Origin, YAxis),
		-dot(Origin, ZAxis)
	};
	result.XAxis =
	{
		(YAxis.y * ZAxis.z - YAxis.z * ZAxis.y) * invdet,
		(ZAxis.y * XAxis.z - ZAxis.z * XAxis.y) * invdet,
		(XAxis.y * YAxis.z - XAxis.z * YAxis.y) * invdet
	};
	result.YAxis =
	{
		(YAxis.z * ZAxis.x - ZAxis.z * YAxis.x) * invdet,
		(ZAxis.z * XAxis.x - XAxis.z * ZAxis.x) * invdet,
		(XAxis.z * YAxis.x - XAxis.x * YAxis.z) * invdet
	};
	result.ZAxis =
	{
		(YAxis.x * ZAxis.y - YAxis.y * ZAxis.x) * invdet,
		(ZAxis.x * XAxis.y - ZAxis.y * XAxis.x) * invdet,
		(XAxis.x * YAxis.y - XAxis.y * YAxis.x) * invdet
	};
	return result;
}

inline Coords Coords::Transpose() const
{
	Coords result;
	result.Origin = { -dot(Origin, XAxis), -dot(Origin, YAxis), -dot(Origin, ZAxis) };
	result.XAxis = { XAxis.x, YAxis.x, ZAxis.x };
	result.YAxis = { XAxis.y, YAxis.y, ZAxis.y };
	result.ZAxis = { XAxis.z, YAxis.z, ZAxis.z };
	return result;
}

inline Coords Coords::Identity()
{
	Coords coords;
	coords.Origin = vec3(0.0f);
	coords.XAxis = vec3(1.0f, 0.0f, 0.0f);
	coords.YAxis = vec3(0.0f, 1.0f, 0.0f);
	coords.ZAxis = vec3(0.0f, 0.0f, 1.0f);
	return coords;
}

inline Coords Coords::Location(const vec3& origin)
{
	Coords coords;
	coords.Origin = origin;
	coords.XAxis = vec3(1.0f, 0.0f, 0.0f);
	coords.YAxis = vec3(0.0f, 1.0f, 0.0f);
	coords.ZAxis = vec3(0.0f, 0.0f, 1.0f);
	return coords;
}

// Core.dll's table (FGlobalMath::FGlobalMath, 0x10144130): entry i the float
// of the sine of i times 0.0003834951969714103, 2 pi over 16384, the product
// a double. A pi of 3.14159265358979 gives the same floats but at the half
// turn, 3.2310891e-15 where the original's is 1.2246468e-16.
inline float Coords::SinTab(int angle)
{
	return (float)std::sin(((angle >> 2) & 16383) * 0.0003834951969714103);
}

// The unit coords turned by a rotator, as the original turns every one -- the
// scripts' conversions (vector(rotator), GetAxes, GetUnAxes; Core.dll
// FRotator::Vector), the view's coords (Engine.dll
// FSceneNode::ComputeRenderCoords), a mesh's frame, an actor's and a brush's
// coords (UMesh::GetFrame, AActor::ToWorld, ABrush::ToWorld): by its sine
// table, 16384 steps round the turn, so an angle's low 2 bits are dropped; a
// cosine is the table a quarter turn on. Its axes are the rotator's facing,
// right and up; Rotation(r) * v gives v in them.
inline Coords Coords::Rotation(const Rotator& rotator)
{
	auto tsin = [](int angle) { return SinTab(angle); };
	auto tcos = [&](int angle) { return tsin(angle + 0x4000); }; // the table a quarter turn on
	float sy = tsin(rotator.Yaw), cy = tcos(rotator.Yaw);
	float sp = tsin(rotator.Pitch), cp = tcos(rotator.Pitch);
	float sr = tsin(rotator.Roll), cr = tcos(rotator.Roll);

	Coords yaw, pitch, roll;
	yaw.Origin = pitch.Origin = roll.Origin = { 0.0f, 0.0f, 0.0f };
	yaw.XAxis = { cy, -sy, 0.0f };
	yaw.YAxis = { sy, cy, 0.0f };
	yaw.ZAxis = { 0.0f, 0.0f, 1.0f };
	pitch.XAxis = { cp, 0.0f, -sp };
	pitch.YAxis = { 0.0f, 1.0f, 0.0f };
	pitch.ZAxis = { sp, 0.0f, cp };
	roll.XAxis = { 1.0f, 0.0f, 0.0f };
	roll.YAxis = { 0.0f, cr, -sr };
	roll.ZAxis = { 0.0f, sr, cr };
	return roll * pitch * yaw;
}

// The original's unit coords turned back by a rotator, by the sine table,
// which turns a vector by it (>>, Core.dll
// UObject::execGreaterGreater_VectorRotator 0x101380e0): the yaw undone, then
// the pitch, then the roll, step by step, so its floats round as the
// original's. InverseRotation(r) * v turns v by r.
inline Coords Coords::InverseRotation(const Rotator& rotator)
{
	auto tsin = [](int angle) { return SinTab(angle); };
	auto tcos = [&](int angle) { return tsin(angle + 0x4000); };
	float sy = tsin(rotator.Yaw), cy = tcos(rotator.Yaw);
	float sp = tsin(rotator.Pitch), cp = tcos(rotator.Pitch);
	float sr = tsin(rotator.Roll), cr = tcos(rotator.Roll);

	Coords yaw, pitch, roll;
	yaw.Origin = pitch.Origin = roll.Origin = { 0.0f, 0.0f, 0.0f };
	yaw.XAxis = { cy, sy, 0.0f };
	yaw.YAxis = { -sy, cy, 0.0f };
	yaw.ZAxis = { 0.0f, 0.0f, 1.0f };
	pitch.XAxis = { cp, 0.0f, sp };
	pitch.YAxis = { 0.0f, 1.0f, 0.0f };
	pitch.ZAxis = { -sp, 0.0f, cp };
	roll.XAxis = { 1.0f, 0.0f, 0.0f };
	roll.YAxis = { 0.0f, cr, -sr };
	roll.ZAxis = { 0.0f, sr, cr };

	Coords result = Identity();
	result *= yaw;
	result *= pitch;
	result *= roll;
	return result;
}

inline void Coords::GetAxes(vec3& X, vec3& Y, vec3& Z) const
{
	X = XAxis;
	Y = YAxis;
	Z = ZAxis;
}

inline void Coords::GetUnAxes(vec3& X, vec3& Y, vec3& Z) const
{
	Coords coords = Inverse();
	X = coords.XAxis;
	Y = coords.YAxis;
	Z = coords.ZAxis;
}

inline Coords Coords::ViewToRenderDev()
{
	Coords coords;
	coords.Origin = { 0.0f, 0.0f, 0.0f };
	coords.XAxis = vec3(0.0f, 0.0f, 1.0f);
	coords.YAxis = vec3(1.0f, 0.0f, 0.0f);
	coords.ZAxis = vec3(0.0f, -1.0f, 0.0f);
	return coords;
}

inline mat4 Coords::ToMatrix() const
{
	mat4 coordsmatrix;
	coordsmatrix[0] = XAxis.x;
	coordsmatrix[1] = XAxis.y;
	coordsmatrix[2] = XAxis.z;
	coordsmatrix[3] = 0.0f;
	coordsmatrix[4] = YAxis.x;
	coordsmatrix[5] = YAxis.y;
	coordsmatrix[6] = YAxis.z;
	coordsmatrix[7] = 0.0f;
	coordsmatrix[8] = ZAxis.x;
	coordsmatrix[9] = ZAxis.y;
	coordsmatrix[10] = ZAxis.z;
	coordsmatrix[11] = 0.0f;
	coordsmatrix[12] = -Origin.x;
	coordsmatrix[13] = -Origin.y;
	coordsmatrix[14] = -Origin.z;
	coordsmatrix[15] = 1.0f;
	return coordsmatrix;
}

inline Coords Coords::FromMatrix(const mat4& coordsmatrix)
{
	Coords coords;
	coords.Origin = vec3(coordsmatrix[12], coordsmatrix[13], coordsmatrix[14]);
	coords.XAxis = vec3(coordsmatrix[0], coordsmatrix[1], coordsmatrix[2]);
	coords.YAxis = vec3(coordsmatrix[4], coordsmatrix[5], coordsmatrix[6]);
	coords.ZAxis = vec3(coordsmatrix[8], coordsmatrix[9], coordsmatrix[10]);
	return coords;
}

// The rotation of these axes (the original's FCoords::OrthoRotation, Core.dll
// 0x10144830): the X axis's pitch and yaw, each atan2 x 32768/pi made an int
// as the original's _ftol does; then the unit coords turned by that pitch and
// yaw, and the roll from the Y and Z axes against their Y axis the same way
inline Rotator Coords::OrthoRotation() const
{
	const double scale = 32768.0 / 3.14159265358979;
	Rotator r(
		ftol(std::atan2((double)XAxis.z, std::sqrt((double)(XAxis.x * XAxis.x + XAxis.y * XAxis.y))) * scale),
		ftol(std::atan2((double)XAxis.y, (double)XAxis.x) * scale),
		0);
	Coords s = Rotation(r);
	r.Roll = ftol(std::atan2((double)dot(ZAxis, s.YAxis), (double)dot(YAxis, s.YAxis)) * scale);
	return r;
}

// Core.dll's FCoords arithmetic as the original builds and uses a brush's
// coordinates (Engine.dll ABrush::ToWorld 0x1030f310, ABrush::BuildCoords
// 0x1032f9e0, FPoly::Transform 0x10387360): each component stored as a
// float, its products and sums at the x87's precision (X87), in Core's order.
// The axes are the rows a vector is dotted with, as Coords keeps them.
struct CoreCoords
{
	// FVector::TransformVectorBy (Core.dll 0x10119e40): each component the
	// vector's dot with that axis, X's term, then Y's, then Z's
	static vec3 TransformVector(const vec3& v, const Coords& c)
	{
		return vec3(Dot(v.x, v.y, v.z, c.XAxis), Dot(v.x, v.y, v.z, c.YAxis), Dot(v.x, v.y, v.z, c.ZAxis));
	}

	// FVector::TransformPointBy (0x10119d70): the origin taken off, each
	// difference held as the x87 holds it, then dotted as a vector
	static vec3 TransformPoint(const vec3& v, const Coords& c)
	{
		double x = X87::Round((double)v.x - c.Origin.x);
		double y = X87::Round((double)v.y - c.Origin.y);
		double z = X87::Round((double)v.z - c.Origin.z);
		return vec3(Dot(x, y, z, c.XAxis), Dot(x, y, z, c.YAxis), Dot(x, y, z, c.ZAxis));
	}

	// FCoords::operator*=(const FCoords&) (0x1011a710): the origin moved as a
	// point by the other, each axis as a vector
	static void Transform(Coords& c, const Coords& by)
	{
		c.Origin = TransformPoint(c.Origin, by);
		c.XAxis = TransformVector(c.XAxis, by);
		c.YAxis = TransformVector(c.YAxis, by);
		c.ZAxis = TransformVector(c.ZAxis, by);
	}

	// FCoords::operator*=(const FRotator&) (0x1011ac80): by the yaw, then the
	// pitch, then the roll, each the unit axes turned by the sine table
	static void Turn(Coords& c, const Rotator& r)
	{
		float sy = Coords::SinTab(r.Yaw), cy = Coords::SinTab(r.Yaw + 0x4000);
		Transform(c, Axes(vec3(cy, sy, 0.0f), vec3(-sy, cy, 0.0f), vec3(0.0f, 0.0f, 1.0f)));
		float sp = Coords::SinTab(r.Pitch), cp = Coords::SinTab(r.Pitch + 0x4000);
		Transform(c, Axes(vec3(cp, 0.0f, sp), vec3(0.0f, 1.0f, 0.0f), vec3(-sp, 0.0f, cp)));
		float sr = Coords::SinTab(r.Roll), cr = Coords::SinTab(r.Roll + 0x4000);
		Transform(c, Axes(vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, cr, -sr), vec3(0.0f, sr, cr)));
	}

	// FCoords::operator/=(const FRotator&) (0x1011b690): the roll undone,
	// then the pitch, then the yaw
	static void Unturn(Coords& c, const Rotator& r)
	{
		float sr = Coords::SinTab(r.Roll), cr = Coords::SinTab(r.Roll + 0x4000);
		Transform(c, Axes(vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, cr, sr), vec3(0.0f, -sr, cr)));
		float sp = Coords::SinTab(r.Pitch), cp = Coords::SinTab(r.Pitch + 0x4000);
		Transform(c, Axes(vec3(cp, 0.0f, -sp), vec3(0.0f, 1.0f, 0.0f), vec3(sp, 0.0f, cp)));
		float sy = Coords::SinTab(r.Yaw), cy = Coords::SinTab(r.Yaw + 0x4000);
		Transform(c, Axes(vec3(cy, -sy, 0.0f), vec3(sy, cy, 0.0f), vec3(0.0f, 0.0f, 1.0f)));
	}

	// FCoords::operator*=(const FScale&) (0x1011c0a0): by the sheer, its rate
	// snapped (FSheerSnap), the unit coords with it on its axis; then each
	// axis times the scale, axis by axis, and the origin over it
	static void Scale(Coords& c, const ::Scale& s)
	{
		Transform(c, Sheer(s, 1.0));
		c.XAxis = vec3(c.XAxis.x * s.Scale.x, c.XAxis.y * s.Scale.y, c.XAxis.z * s.Scale.z);
		c.YAxis = vec3(c.YAxis.x * s.Scale.x, c.YAxis.y * s.Scale.y, c.YAxis.z * s.Scale.z);
		c.ZAxis = vec3(c.ZAxis.x * s.Scale.x, c.ZAxis.y * s.Scale.y, c.ZAxis.z * s.Scale.z);
		c.Origin = vec3(
			(float)X87::Round((double)c.Origin.x / s.Scale.x),
			(float)X87::Round((double)c.Origin.y / s.Scale.y),
			(float)X87::Round((double)c.Origin.z / s.Scale.z));
	}

	// FCoords::operator/=(const FScale&) (0x1011c860): each axis over the
	// scale, axis by axis, and the origin times it; then the sheer undone
	static void Unscale(Coords& c, const ::Scale& s)
	{
		auto over = [&](const vec3& a) {
			return vec3(
				(float)X87::Round((double)a.x / s.Scale.x),
				(float)X87::Round((double)a.y / s.Scale.y),
				(float)X87::Round((double)a.z / s.Scale.z));
		};
		c.XAxis = over(c.XAxis);
		c.YAxis = over(c.YAxis);
		c.ZAxis = over(c.ZAxis);
		c.Origin = vec3(s.Scale.x * c.Origin.x, s.Scale.y * c.Origin.y, c.Origin.z * s.Scale.z);
		Transform(c, Sheer(s, -1.0));
	}

	// FPlane::TransformPlaneByOrtho (0x1011a000): the normal dotted with each
	// axis, Y's and Z's terms first; W less the coords' origin, turned as a
	// vector (turnedOrigin, TransformVector(c.Origin, c)), dotted with the
	// new normal, Y's and Z's terms first
	static vec4 TransformPlaneByOrtho(const vec4& p, const Coords& c, const vec3& turnedOrigin)
	{
		auto R = X87::Round;
		vec4 world;
		world.x = (float)R(R(R((double)c.XAxis.y * p.y) + R((double)c.XAxis.z * p.z)) + R((double)c.XAxis.x * p.x));
		world.y = (float)R(R(R((double)c.YAxis.y * p.y) + R((double)c.YAxis.z * p.z)) + R((double)p.x * c.YAxis.x));
		world.z = (float)R(R(R((double)c.ZAxis.y * p.y) + R((double)c.ZAxis.z * p.z)) + R((double)p.x * c.ZAxis.x));
		double moved = R(R(R((double)turnedOrigin.y * world.y) + R((double)turnedOrigin.z * world.z)) + R((double)turnedOrigin.x * world.x));
		world.w = (float)R((double)p.w - moved);
		return world;
	}

private:
	static float Dot(double x, double y, double z, const vec3& a)
	{
		auto R = X87::Round;
		return (float)R(R(R(x * a.x) + R(y * a.y)) + R(z * a.z));
	}

	static Coords Axes(const vec3& x, const vec3& y, const vec3& z)
	{
		Coords c;
		c.Origin = vec3(0.0f);
		c.XAxis = x;
		c.YAxis = y;
		c.ZAxis = z;
		return c;
	}

	// The unit coords with the scale's sheer, snapped as FSheerSnap snaps it
	// (0x1011c0b5): past 0.65 either way 0.15 nearer 0, past 0.55 a half,
	// past 0.05 0.05 nearer 0, else none; on the axis the scale names, made
	// negative to undo it
	static Coords Sheer(const ::Scale& s, double sign)
	{
		auto R = X87::Round;
		double rate = s.SheerRate;
		double snap;
		if (X87::Under(rate, -0.65))
			snap = R(rate + 0.15);
		else if (rate > 0.65)
			snap = R(rate - 0.15);
		else if (X87::Under(rate, -0.55))
			snap = -0.5;
		else if (rate > 0.55)
			snap = 0.5;
		else if (X87::Under(rate, -0.05))
			snap = R(rate + 0.05);
		else if (rate > 0.05)
			snap = R(rate - 0.05);
		else
			snap = 0.0;
		float value = (float)(sign * snap);
		Coords c = Coords::Identity();
		switch (s.SheerAxis)
		{
		case SHEER_XY: c.XAxis.y = value; break;
		case SHEER_XZ: c.XAxis.z = value; break;
		case SHEER_YX: c.YAxis.x = value; break;
		case SHEER_YZ: c.YAxis.z = value; break;
		case SHEER_ZX: c.ZAxis.x = value; break;
		case SHEER_ZY: c.ZAxis.y = value; break;
		default: break;
		}
		return c;
	}
};
