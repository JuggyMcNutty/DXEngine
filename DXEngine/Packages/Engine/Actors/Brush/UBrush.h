#pragma once

#include "Packages/Engine/Actors/UActor.h"
#include "Math/coords.h"

class UBrush : public UActor
{
public:
	using UActor::UActor;

	// ABrush::ToWorld (Engine.dll 0x1030f310), the brush's coordinates were
	// it to stand at location, turned to rotation, as the original builds
	// them (CoreCoords): the unit coords moved by the place, scaled by
	// PostScale, turned, scaled by MainScale and moved back by PrePivot. A
	// point of the brush goes into the world by CoreCoords::TransformPoint:
	// off PrePivot, scaled by MainScale, turned, then scaled by PostScale
	// along the world's axes and moved to the place.
	Coords ToWorldAt(const vec3& location, const Rotator& rotation)
	{
		Coords c = Coords::Identity();
		c.Origin = vec3(0.0f - location.x, 0.0f - location.y, 0.0f - location.z);
		CoreCoords::Scale(c, PostScale());
		CoreCoords::Turn(c, rotation);
		CoreCoords::Scale(c, MainScale());
		const vec3& pivot = PrePivot();
		c.Origin = vec3(c.Origin.x - -pivot.x, c.Origin.y - -pivot.y, c.Origin.z - -pivot.z);
		return c;
	}

	// The same transform as one matrix, for the drawing, the light and the
	// boxes the traces take (their sheer, which no stock brush has, left
	// out): off PrePivot, scaled by MainScale, turned, scaled by PostScale
	// along the world's axes, moved to the place
	mat4 ToWorldMatrix(const vec3& location, const Rotator& rotation)
	{
		return mat4::translate(location) * mat4::scale(PostScale().Scale) * Coords::Rotation(rotation).ToMatrix() * mat4::scale(MainScale().Scale) * mat4::translate(-PrePivot());
	}

	// ABrush::BuildCoords (Engine.dll 0x1032f9e0), as the brush tracker's
	// filing takes it: the point transform, the unit coords scaled by
	// PostScale, turned and scaled by MainScale; the vector transform, the
	// unit coords unscaled by MainScale, turned back and unscaled by
	// PostScale, its axes transposed, so that it turns a polygon's normal as
	// the polygon turns, a mirror's too. Gives the orientation, the sign of
	// MainScale's product times PostScale's (FScale::Orientation, Core.dll
	// 0x10118880): under 0 for a mirroring brush, whose polygons' corners
	// the filing reverses.
	float BuildCoords(Coords& pointXform, Coords& vectorXform)
	{
		pointXform = Coords::Identity();
		CoreCoords::Scale(pointXform, PostScale());
		CoreCoords::Turn(pointXform, Rotation());
		CoreCoords::Scale(pointXform, MainScale());
		Coords back = Coords::Identity();
		CoreCoords::Unscale(back, MainScale());
		CoreCoords::Unturn(back, Rotation());
		CoreCoords::Unscale(back, PostScale());
		vectorXform.Origin = vec3(0.0f); // TransformVector reads no origin
		vectorXform.XAxis = vec3(back.XAxis.x, back.YAxis.x, back.ZAxis.x);
		vectorXform.YAxis = vec3(back.XAxis.y, back.YAxis.y, back.ZAxis.y);
		vectorXform.ZAxis = vec3(back.XAxis.z, back.YAxis.z, back.ZAxis.z);
		auto sign = [](const vec3& s) {
			double product = (double)s.z * s.y * s.x;
			return product > 0.0 ? 1.0f : product < 0.0 ? -1.0f : 0.0f;
		};
		return sign(MainScale().Scale) * sign(PostScale().Scale);
	}

	Color& BrushColor() { return Value<Color>(PropOffsets_Brush.BrushColor); }
	uint8_t& CsgOper() { return Value<uint8_t>(PropOffsets_Brush.CsgOper); }
	Scale& MainScale() { return Value<Scale>(PropOffsets_Brush.MainScale); }
	int& PolyFlags() { return Value<int>(PropOffsets_Brush.PolyFlags); }
	vec3& PostPivot() { return Value<vec3>(PropOffsets_Brush.PostPivot); }
	Scale& PostScale() { return Value<Scale>(PropOffsets_Brush.PostScale); }
	Scale& TempScale() { return Value<Scale>(PropOffsets_Brush.TempScale); }
	UObject*& UnusedLightMesh() { return Value<UObject*>(PropOffsets_Brush.UnusedLightMesh); }
	BitfieldBool bColored() { return BoolValue(PropOffsets_Brush.bColored); }
};
