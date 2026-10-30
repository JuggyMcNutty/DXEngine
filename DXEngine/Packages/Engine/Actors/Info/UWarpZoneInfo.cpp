#include "Precomp.h"
#include "UWarpZoneInfo.h"

// AWarpZoneInfo::execWarp (Engine.dll 0x103e39e0) and execUnWarp
// (0x103e3e70): the place, the velocity and the rotation taken through the
// zone's WarpCoords -- Warp by their inverse (out of the warp's space),
// UnWarp by the coords themselves (into it): a point less the coords' origin,
// and a vector, dotted with the three axes; the rotation's own coords (its
// facing and sides) dotted the same way and read back as a rotation with
// its roll (FCoords::OrthoRotation).

static vec3 PointBy(const Coords& c, const vec3& p)
{
	vec3 d = p - c.Origin;
	return { dot(d, c.XAxis), dot(d, c.YAxis), dot(d, c.ZAxis) };
}

static vec3 VectorBy(const Coords& c, const vec3& v)
{
	return { dot(v, c.XAxis), dot(v, c.YAxis), dot(v, c.ZAxis) };
}

static Coords InverseOf(const Coords& c)
{
	Coords r;
	r.Origin = -VectorBy(c, c.Origin);
	r.XAxis = { c.XAxis.x, c.YAxis.x, c.ZAxis.x };
	r.YAxis = { c.XAxis.y, c.YAxis.y, c.ZAxis.y };
	r.ZAxis = { c.XAxis.z, c.YAxis.z, c.ZAxis.z };
	return r;
}

static void Through(const Coords& by, vec3& Loc, vec3& Vel, Rotator& R)
{
	Loc = PointBy(by, Loc);
	Vel = VectorBy(by, Vel);
	Coords c = Coords::Rotation(R);
	c.XAxis = VectorBy(by, c.XAxis);
	c.YAxis = VectorBy(by, c.YAxis);
	c.ZAxis = VectorBy(by, c.ZAxis);
	R = c.OrthoRotation();
}

void UWarpZoneInfo::Warp(vec3& Loc, vec3& Vel, Rotator& R)
{
	Through(InverseOf(WarpCoords()), Loc, Vel, R);
}

void UWarpZoneInfo::UnWarp(vec3& Loc, vec3& Vel, Rotator& R)
{
	Through(WarpCoords(), Loc, Vel, R);
}
