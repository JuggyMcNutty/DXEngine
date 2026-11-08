#pragma once

#include "Packages/Core/Properties/UArrayProperty.h"
#include "Utils/AlignedAlloc.h"
#include "Math/quaternion.h"
#include "Math/coords.h"
#include <optional>

class UArrayProperty;

// A value of a dynamic array's property, or of a fixed array's, held on its
// own: the property's elements, copied, constructed and destructed by it
class ArrayValue
{
public:
	ArrayValue() = default;

	ArrayValue(const ArrayValue& other)
	{
		Load(other.Type, other.Ptr);
	}

	ArrayValue(ArrayValue&& other)
	{
		*this = std::move(other);
	}

	~ArrayValue()
	{
		Reset();
	}

	void Store(void* dest) const
	{
		if (Type)
		{
			Type->CopyArray(dest, Ptr);
		}
	}

	void Load(UProperty* type, void* src)
	{
		Reset();
		Type = type;
		if (Type)
		{
			Ptr = AlignedAlloc(Type->ArrayAlignment(), Type->ArraySize());
			Type->CopyConstructArray(Ptr, src);
		}
	}

	// An empty array of the type
	void Init(UProperty* type)
	{
		Reset();
		Type = type;
		if (Type)
		{
			Ptr = AlignedAlloc(Type->ArrayAlignment(), Type->ArraySize());
			Type->ConstructArray(Ptr);
		}
	}

	void Reset()
	{
		if (Type)
		{
			Type->DestructArray(Ptr);
			AlignedFree(Ptr);
			Type = nullptr;
		}
	}

	ArrayValue& operator=(const ArrayValue& other)
	{
		if (this != &other)
		{
			Load(other.Type, other.Ptr);
		}
		return *this;
	}

	ArrayValue& operator=(ArrayValue&& other)
	{
		if (this != &other)
		{
			Type = other.Type;
			Ptr = other.Ptr;
			other.Type = nullptr;
			other.Ptr = nullptr;
		}
		return *this;
	}

	UProperty* Type = nullptr;
	void* Ptr = nullptr;
};
