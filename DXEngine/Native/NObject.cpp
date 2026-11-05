
#include "Precomp.h"
#include "NObject.h"
#include "VM/NativeFunc.h"
#include "VM/Frame.h"
#include "Package/PackageManager.h"
#include "Engine.h"
#include "Utils/StrTools.h"
#include "Utils/Random.h"
#include "Package/ObjectFlags.h"
#include "Packages/Core/UClass.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Subsystems/UDXEngineNetworkDevice.h"
#include "Packages/Engine/Subsystems/UDXEngineAudioDevice.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"
#include "Packages/Engine/Subsystems/UGameEngine.h"
#include "Packages/Engine/UDXEngineClient.h"
#include "Packages/Engine/UConsole.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UGameInfo.h"
#include "Packages/DeusEx/UDeusExLevelInfo.h"
#include "Packages/DeusEx/UDeusExSaveInfo.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Core/USubsystem.h"
#include "Packages/Core/UPackage.h"
#include "Packages/Core/UEnum.h"
#include <cmath>
#include <chrono>

#ifdef _MSC_VER
#pragma warning(disable: 4244) // warning C4244: '/=': conversion from 'float' to 'int', possible loss of data
#endif

void NObject::RegisterFunctions()
{
	RegisterVMNativeFunc_2("Object", "Abs", &NObject::Abs, 186);
	RegisterVMNativeFunc_2("Object", "AddAdd_Byte", &NObject::AddAdd_Byte, 139);
	RegisterVMNativeFunc_2("Object", "AddAdd_Int", &NObject::AddAdd_Int, 165);
	RegisterVMNativeFunc_2("Object", "AddAdd_PreByte", &NObject::AddAdd_PreByte, 137);
	RegisterVMNativeFunc_2("Object", "AddAdd_PreInt", &NObject::AddAdd_PreInt, 163);
	RegisterVMNativeFunc_3("Object", "AddEqual_ByteByte", &NObject::AddEqual_ByteByte, 135);
	RegisterVMNativeFunc_3("Object", "AddEqual_FloatFloat", &NObject::AddEqual_FloatFloat, 184);
	RegisterVMNativeFunc_3("Object", "AddEqual_IntInt", &NObject::AddEqual_IntInt, 161);
	RegisterVMNativeFunc_3("Object", "AddEqual_RotatorRotator", &NObject::AddEqual_RotatorRotator, 318);
	RegisterVMNativeFunc_3("Object", "AddEqual_VectorVector", &NObject::AddEqual_VectorVector, 223);
	RegisterVMNativeFunc_3("Object", "Add_FloatFloat", &NObject::Add_FloatFloat, 174);
	RegisterVMNativeFunc_3("Object", "Add_IntInt", &NObject::Add_IntInt, 146);
	RegisterVMNativeFunc_3("Object", "Add_RotatorRotator", &NObject::Add_RotatorRotator, 316);
	RegisterVMNativeFunc_3("Object", "Add_VectorVector", &NObject::Add_VectorVector, 215);
	RegisterVMNativeFunc_3("Object", "AndAnd_BoolBool", &NObject::AndAnd_BoolBool, 130);
	RegisterVMNativeFunc_3("Object", "And_IntInt", &NObject::And_IntInt, 156);
	RegisterVMNativeFunc_2("Object", "Asc", &NObject::Asc, 237);
	RegisterVMNativeFunc_2("Object", "Atan", &NObject::Atan, 190);
	RegisterVMNativeFunc_2("Object", "Caps", &NObject::Caps, 235);
	RegisterVMNativeFunc_2("Object", "Chr", &NObject::Chr, 236);
	RegisterVMNativeFunc_4("Object", "Clamp", &NObject::Clamp, 251);
	RegisterVMNativeFunc_3("Object", "ClassIsChildOf", &NObject::ClassIsChildOf, 258);
	RegisterVMNativeFunc_3("Object", "ComplementEqual_FloatFloat", &NObject::ComplementEqual_FloatFloat, 210);
	RegisterVMNativeFunc_3("Object", "ComplementEqual_StrStr", &NObject::ComplementEqual_StrStr, 124);
	RegisterVMNativeFunc_2("Object", "Complement_PreInt", &NObject::Complement_PreInt, 141);
	RegisterVMNativeFunc_3("Object", "Concat_StrStr", &NObject::Concat_StrStr, 112);
	RegisterVMNativeFunc_2("Object", "Cos", &NObject::Cos, 188);
	RegisterVMNativeFunc_3("Object", "Cross_VectorVector", &NObject::Cross_VectorVector, 220);
	RegisterVMNativeFunc_1("Object", "Disable", &NObject::Disable, 118);
	RegisterVMNativeFunc_3("Object", "DivideEqual_ByteByte", &NObject::DivideEqual_ByteByte, 134);
	RegisterVMNativeFunc_3("Object", "DivideEqual_FloatFloat", &NObject::DivideEqual_FloatFloat, 183);
	RegisterVMNativeFunc_3("Object", "DivideEqual_IntFloat", &NObject::DivideEqual_IntFloat, 160);
	RegisterVMNativeFunc_3("Object", "DivideEqual_RotatorFloat", &NObject::DivideEqual_RotatorFloat, 291);
	RegisterVMNativeFunc_3("Object", "DivideEqual_VectorFloat", &NObject::DivideEqual_VectorFloat, 222);
	RegisterVMNativeFunc_3("Object", "Divide_FloatFloat", &NObject::Divide_FloatFloat, 172);
	RegisterVMNativeFunc_3("Object", "Divide_IntInt", &NObject::Divide_IntInt, 145);
	RegisterVMNativeFunc_3("Object", "Divide_RotatorFloat", &NObject::Divide_RotatorFloat, 289);
	RegisterVMNativeFunc_3("Object", "Divide_VectorFloat", &NObject::Divide_VectorFloat, 214);
	RegisterVMNativeFunc_3("Object", "Dot_VectorVector", &NObject::Dot_VectorVector, 219);
	RegisterVMNativeFunc_4("Object", "DynamicLoadObject", &NObject::DynamicLoadObject, 0);
	RegisterVMNativeFunc_1("Object", "Enable", &NObject::Enable, 117);
	RegisterVMNativeFunc_3("Object", "EqualEqual_BoolBool", &NObject::EqualEqual_BoolBool, 242);
	RegisterVMNativeFunc_3("Object", "EqualEqual_FloatFloat", &NObject::EqualEqual_FloatFloat, 180);
	RegisterVMNativeFunc_3("Object", "EqualEqual_IntInt", &NObject::EqualEqual_IntInt, 154);
	RegisterVMNativeFunc_3("Object", "EqualEqual_NameName", &NObject::EqualEqual_NameName, 254);
	RegisterVMNativeFunc_3("Object", "EqualEqual_ObjectObject", &NObject::EqualEqual_ObjectObject, 114);
	RegisterVMNativeFunc_3("Object", "EqualEqual_RotatorRotator", &NObject::EqualEqual_RotatorRotator, 142);
	RegisterVMNativeFunc_3("Object", "EqualEqual_StrStr", &NObject::EqualEqual_StrStr, 122);
	RegisterVMNativeFunc_3("Object", "EqualEqual_VectorVector", &NObject::EqualEqual_VectorVector, 217);
	RegisterVMNativeFunc_2("Object", "Exp", &NObject::Exp, 191);
	RegisterVMNativeFunc_4("Object", "FClamp", &NObject::FClamp, 246);
	RegisterVMNativeFunc_3("Object", "FMax", &NObject::FMax, 245);
	RegisterVMNativeFunc_3("Object", "FMin", &NObject::FMin, 244);
	RegisterVMNativeFunc_1("Object", "FRand", &NObject::FRand, 195);
	RegisterVMNativeFunc_4("Object", "GetAxes", &NObject::GetAxes, 229);
	RegisterVMNativeFunc_3("Object", "GetEnum", &NObject::GetEnum, 0);
	RegisterVMNativeFunc_2("Object", "GetPropertyText", &NObject::GetPropertyText, 0);
	RegisterVMNativeFunc_1("Object", "GetStateName", &NObject::GetStateName, 284);
	RegisterVMNativeFunc_4("Object", "GetUnAxes", &NObject::GetUnAxes, 230);
	RegisterVMNativeFunc_2("Object", "GotoState", &NObject::GotoState, 113);
	RegisterVMNativeFunc_3("Object", "GreaterEqual_FloatFloat", &NObject::GreaterEqual_FloatFloat, 179);
	RegisterVMNativeFunc_3("Object", "GreaterEqual_IntInt", &NObject::GreaterEqual_IntInt, 153);
	RegisterVMNativeFunc_3("Object", "GreaterEqual_StrStr", &NObject::GreaterEqual_StrStr, 121);
	RegisterVMNativeFunc_3("Object", "GreaterGreaterGreater_IntInt", &NObject::GreaterGreaterGreater_IntInt, 196);
	RegisterVMNativeFunc_3("Object", "GreaterGreater_IntInt", &NObject::GreaterGreater_IntInt, 149);
	RegisterVMNativeFunc_3("Object", "GreaterGreater_VectorRotator", &NObject::GreaterGreater_VectorRotator, 276);
	RegisterVMNativeFunc_3("Object", "Greater_FloatFloat", &NObject::Greater_FloatFloat, 177);
	RegisterVMNativeFunc_3("Object", "Greater_IntInt", &NObject::Greater_IntInt, 151);
	RegisterVMNativeFunc_3("Object", "Greater_StrStr", &NObject::Greater_StrStr, 116);
	RegisterVMNativeFunc_3("Object", "InStr", &NObject::InStr, 126);
	RegisterVMNativeFunc_3("Object", "Invert", &NObject::Invert, 227);
	RegisterVMNativeFunc_2("Object", "IsA", &NObject::IsA, 303);
	RegisterVMNativeFunc_2("Object", "IsInState", &NObject::IsInState, 281);
	RegisterVMNativeFunc_3("Object", "Left", &NObject::Left, 128);
	RegisterVMNativeFunc_2("Object", "Len", &NObject::Len, 125);
	RegisterVMNativeFunc_4("Object", "Lerp", &NObject::Lerp, 247);
	RegisterVMNativeFunc_3("Object", "LessEqual_FloatFloat", &NObject::LessEqual_FloatFloat, 178);
	RegisterVMNativeFunc_3("Object", "LessEqual_IntInt", &NObject::LessEqual_IntInt, 152);
	RegisterVMNativeFunc_3("Object", "LessEqual_StrStr", &NObject::LessEqual_StrStr, 120);
	RegisterVMNativeFunc_3("Object", "LessLess_IntInt", &NObject::LessLess_IntInt, 148);
	RegisterVMNativeFunc_3("Object", "LessLess_VectorRotator", &NObject::LessLess_VectorRotator, 275);
	RegisterVMNativeFunc_3("Object", "Less_FloatFloat", &NObject::Less_FloatFloat, 176);
	RegisterVMNativeFunc_3("Object", "Less_IntInt", &NObject::Less_IntInt, 150);
	RegisterVMNativeFunc_3("Object", "Less_StrStr", &NObject::Less_StrStr, 115);
	RegisterVMNativeFunc_4("Object", "Localize", &NObject::Localize, 0);
	RegisterVMNativeFunc_2("Object", "Log", &NObject::Log, 231);
	RegisterVMNativeFunc_2("Object", "Loge", &NObject::Loge, 192);
	RegisterVMNativeFunc_3("Object", "Max", &NObject::Max, 250);
	RegisterVMNativeFunc_4("Object", "Mid", &NObject::Mid, 127);
	RegisterVMNativeFunc_3("Object", "Min", &NObject::Min, 249);
	RegisterVMNativeFunc_3("Object", "MirrorVectorByNormal", &NObject::MirrorVectorByNormal, 300);
	RegisterVMNativeFunc_3("Object", "MultiplyEqual_ByteByte", &NObject::MultiplyEqual_ByteByte, 133);
	RegisterVMNativeFunc_3("Object", "MultiplyEqual_FloatFloat", &NObject::MultiplyEqual_FloatFloat, 182);
	RegisterVMNativeFunc_3("Object", "MultiplyEqual_IntFloat", &NObject::MultiplyEqual_IntFloat, 159);
	RegisterVMNativeFunc_3("Object", "MultiplyEqual_RotatorFloat", &NObject::MultiplyEqual_RotatorFloat, 290);
	RegisterVMNativeFunc_3("Object", "MultiplyEqual_VectorFloat", &NObject::MultiplyEqual_VectorFloat, 221);
	RegisterVMNativeFunc_3("Object", "MultiplyEqual_VectorVector", &NObject::MultiplyEqual_VectorVector, 297);
	RegisterVMNativeFunc_3("Object", "MultiplyMultiply_FloatFloat", &NObject::MultiplyMultiply_FloatFloat, 170);
	RegisterVMNativeFunc_3("Object", "Multiply_FloatFloat", &NObject::Multiply_FloatFloat, 171);
	RegisterVMNativeFunc_3("Object", "Multiply_FloatRotator", &NObject::Multiply_FloatRotator, 288);
	RegisterVMNativeFunc_3("Object", "Multiply_FloatVector", &NObject::Multiply_FloatVector, 213);
	RegisterVMNativeFunc_3("Object", "Multiply_IntInt", &NObject::Multiply_IntInt, 144);
	RegisterVMNativeFunc_3("Object", "Multiply_RotatorFloat", &NObject::Multiply_RotatorFloat, 287);
	RegisterVMNativeFunc_3("Object", "Multiply_VectorFloat", &NObject::Multiply_VectorFloat, 212);
	RegisterVMNativeFunc_3("Object", "Multiply_VectorVector", &NObject::Multiply_VectorVector, 296);
	RegisterVMNativeFunc_2("Object", "Normal", &NObject::Normal, 226);
	RegisterVMNativeFunc_2("Object", "Normalize", &NObject::Normalize, 0);
	RegisterVMNativeFunc_3("Object", "NotEqual_BoolBool", &NObject::NotEqual_BoolBool, 243);
	RegisterVMNativeFunc_3("Object", "NotEqual_FloatFloat", &NObject::NotEqual_FloatFloat, 181);
	RegisterVMNativeFunc_3("Object", "NotEqual_IntInt", &NObject::NotEqual_IntInt, 155);
	RegisterVMNativeFunc_3("Object", "NotEqual_NameName", &NObject::NotEqual_NameName, 255);
	RegisterVMNativeFunc_3("Object", "NotEqual_ObjectObject", &NObject::NotEqual_ObjectObject, 119);
	RegisterVMNativeFunc_3("Object", "NotEqual_RotatorRotator", &NObject::NotEqual_RotatorRotator, 203);
	RegisterVMNativeFunc_3("Object", "NotEqual_StrStr", &NObject::NotEqual_StrStr, 123);
	RegisterVMNativeFunc_3("Object", "NotEqual_VectorVector", &NObject::NotEqual_VectorVector, 218);
	RegisterVMNativeFunc_2("Object", "Not_PreBool", &NObject::Not_PreBool, 129);
	RegisterVMNativeFunc_3("Object", "OrOr_BoolBool", &NObject::OrOr_BoolBool, 132);
	RegisterVMNativeFunc_3("Object", "Or_IntInt", &NObject::Or_IntInt, 158);
	RegisterVMNativeFunc_4("Object", "OrthoRotation", &NObject::OrthoRotation, 0);
	RegisterVMNativeFunc_3("Object", "Percent_FloatFloat", &NObject::Percent_FloatFloat, 173);
	RegisterVMNativeFunc_2("Object", "Rand", &NObject::Rand, 167);
	RegisterVMNativeFunc_3("Object", "RandRange", &NObject::RandRange, 1033);
	RegisterVMNativeFunc_0("Object", "ResetConfig", &NObject::ResetConfig, 0);
	RegisterVMNativeFunc_3("Object", "Right", &NObject::Right, 234);
	RegisterVMNativeFunc_2("Object", "RotRand", &NObject::RotRand, 320);
	RegisterVMNativeFunc_0("Object", "SaveConfig", &NObject::SaveConfig, 536);
	RegisterVMNativeFunc_2("Object", "SetPropertyText", &NObject::SetPropertyText, 0);
	RegisterVMNativeFunc_2("Object", "Sin", &NObject::Sin, 187);
	RegisterVMNativeFunc_4("Object", "Smerp", &NObject::Smerp, 248);
	RegisterVMNativeFunc_2("Object", "Sqrt", &NObject::Sqrt, 193);
	RegisterVMNativeFunc_2("Object", "Square", &NObject::Square, 194);
	RegisterVMNativeFunc_0("Object", "StaticSaveConfig", &NObject::StaticSaveConfig, 0);
	RegisterVMNativeFunc_3("Object", "SubtractEqual_ByteByte", &NObject::SubtractEqual_ByteByte, 136);
	RegisterVMNativeFunc_3("Object", "SubtractEqual_FloatFloat", &NObject::SubtractEqual_FloatFloat, 185);
	RegisterVMNativeFunc_3("Object", "SubtractEqual_IntInt", &NObject::SubtractEqual_IntInt, 162);
	RegisterVMNativeFunc_3("Object", "SubtractEqual_RotatorRotator", &NObject::SubtractEqual_RotatorRotator, 319);
	RegisterVMNativeFunc_3("Object", "SubtractEqual_VectorVector", &NObject::SubtractEqual_VectorVector, 224);
	RegisterVMNativeFunc_2("Object", "SubtractSubtract_Byte", &NObject::SubtractSubtract_Byte, 140);
	RegisterVMNativeFunc_2("Object", "SubtractSubtract_Int", &NObject::SubtractSubtract_Int, 166);
	RegisterVMNativeFunc_2("Object", "SubtractSubtract_PreByte", &NObject::SubtractSubtract_PreByte, 138);
	RegisterVMNativeFunc_2("Object", "SubtractSubtract_PreInt", &NObject::SubtractSubtract_PreInt, 164);
	RegisterVMNativeFunc_3("Object", "Subtract_FloatFloat", &NObject::Subtract_FloatFloat, 175);
	RegisterVMNativeFunc_3("Object", "Subtract_IntInt", &NObject::Subtract_IntInt, 147);
	RegisterVMNativeFunc_2("Object", "Subtract_PreFloat", &NObject::Subtract_PreFloat, 169);
	RegisterVMNativeFunc_2("Object", "Subtract_PreInt", &NObject::Subtract_PreInt, 143);
	RegisterVMNativeFunc_2("Object", "Subtract_PreVector", &NObject::Subtract_PreVector, 211);
	RegisterVMNativeFunc_3("Object", "Subtract_RotatorRotator", &NObject::Subtract_RotatorRotator, 317);
	RegisterVMNativeFunc_3("Object", "Subtract_VectorVector", &NObject::Subtract_VectorVector, 216);
	RegisterVMNativeFunc_2("Object", "Tan", &NObject::Tan, 189);
	RegisterVMNativeFunc_1("Object", "VRand", &NObject::VRand, 252);
	RegisterVMNativeFunc_2("Object", "VSize", &NObject::VSize, 225);
	RegisterVMNativeFunc_1("Object", "Warn", &NObject::Warn, 232);
	RegisterVMNativeFunc_3("Object", "XorXor_BoolBool", &NObject::XorXor_BoolBool, 131);
	RegisterVMNativeFunc_3("Object", "Xor_IntInt", &NObject::Xor_IntInt, 157);

	RegisterVMNativeFunc_2("Object", "AllObjects", &NObject::AllObjects_DeusEx, 1001);
	RegisterVMNativeFunc_1("Object", "CriticalDelete", &NObject::CriticalDelete, 751);
	RegisterVMNativeFunc_3("Object", "GetConfig", &NObject::GetConfig, 0);
	RegisterVMNativeFunc_1("Object", "clock", &NObject::Clock, 1005);
	RegisterVMNativeFunc_1("Object", "unclock", &NObject::Unclock, 1006);
	RegisterVMNativeFunc_2("Object", "CyclesToSeconds", &NObject::CyclesToSeconds, 1007);

	// Package 61 stuff
	RegisterVMNativeFunc_3("Object", "At_StrStr", &NObject::At_StrStr, 168);

}

void NObject::Abs(float A, float& ReturnValue)
{
	ReturnValue = std::abs(A);
}

void NObject::AddAdd_Byte(uint8_t& A, uint8_t& ReturnValue)
{
	ReturnValue = A++;
}

void NObject::AddAdd_Int(int& A, int& ReturnValue)
{
	ReturnValue = A++;
}

void NObject::AddAdd_PreByte(uint8_t& A, uint8_t& ReturnValue)
{
	ReturnValue = ++A;
}

void NObject::AddAdd_PreInt(int& A, int& ReturnValue)
{
	ReturnValue = ++A;
}

void NObject::AddEqual_ByteByte(uint8_t& A, uint8_t B, uint8_t& ReturnValue)
{
	ReturnValue = A += B;
}

void NObject::AddEqual_FloatFloat(float& A, float B, float& ReturnValue)
{
	ReturnValue = A += B;
}

void NObject::AddEqual_IntInt(int& A, int B, int& ReturnValue)
{
	ReturnValue = A += B;
}

void NObject::AddEqual_RotatorRotator(Rotator& A, const Rotator& B, Rotator& ReturnValue)
{
	ReturnValue = A += B;
}

void NObject::AddEqual_VectorVector(vec3& A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = A += B;
}

void NObject::Add_FloatFloat(float A, float B, float& ReturnValue)
{
	ReturnValue = A + B;
}

void NObject::Add_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = A + B;
}

void NObject::Add_RotatorRotator(const Rotator& A, const Rotator& B, Rotator& ReturnValue)
{
	ReturnValue = A + B;
}

void NObject::Add_VectorVector(const vec3& A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = A + B;
}

void NObject::OrOr_BoolBool(bool A, std::optional<bool> B, BitfieldBool& ReturnValue)
{
	ReturnValue = A || *B;
}

void NObject::AndAnd_BoolBool(bool A, std::optional<bool> B, BitfieldBool& ReturnValue)
{
	ReturnValue = A && *B;
}

void NObject::And_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = A & B;
}

void NObject::Asc(const ScriptString& S, int& ReturnValue)
{
	ReturnValue = !S.empty() ? (int)S.front() : 0;
}

void NObject::At_StrStr(const ScriptString& A, const ScriptString& B, ScriptString& ReturnValue)
{
	ReturnValue = A + u" " + B;
}

void NObject::Atan(float A, float& ReturnValue)
{
	ReturnValue = std::atan(A);
}

void NObject::Caps(const ScriptString& S, ScriptString& ReturnValue)
{
	// a to z alone, as the original's towupper in the C locale changes them
	// and leaves every other character, Latin-1's letters among them
	ReturnValue.clear();
	ReturnValue.reserve(S.size());
	for (char16_t c : S)
		ReturnValue.push_back(c >= 'a' && c <= 'z' ? (char16_t)(c - 'a' + 'A') : c);
}

// One 16-bit character, the low 16 bits, and 0 the empty string, as the
// original's (Core.dll 0x1013c520)
void NObject::Chr(int i, ScriptString& ReturnValue)
{
	if ((i & 0xffff) == 0)
		ReturnValue.clear();
	else
		ReturnValue = ScriptString(1, (char16_t)i);
}

// UE1's Clamp (UnTemplate.h, Core.dll's execClamp): under A gives A, else
// under B the value, else B, so with A over B the value from A up gives B
void NObject::Clamp(int V, int A, int B, int& ReturnValue)
{
	ReturnValue = V < A ? A : V < B ? V : B;
}

void NObject::ClassIsChildOf(UObject* TestClass, UObject* ParentClass, BitfieldBool& ReturnValue)
{
	for (UClass* cls = UObject::Cast<UClass>(TestClass); cls != nullptr; cls = static_cast<UClass*>(cls->BaseStruct))
	{
		if (cls == ParentClass)
		{
			ReturnValue = true;
			return;
		}
	}
	ReturnValue = false;
}

void NObject::ComplementEqual_FloatFloat(float A, float B, BitfieldBool& ReturnValue)
{
	ReturnValue = std::abs(A - B) < 0.0001;
}

void NObject::ComplementEqual_StrStr(const ScriptString& A, const ScriptString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = ScriptEqualsIgnoreCase(A, B);
}

void NObject::Complement_PreInt(int A, int& ReturnValue)
{
	ReturnValue = ~A;
}

void NObject::Concat_StrStr(const ScriptString& A, const ScriptString& B, ScriptString& ReturnValue)
{
	ReturnValue = A + B;
}

void NObject::Cos(float A, float& ReturnValue)
{
	ReturnValue = std::cos(A);
}

void NObject::Cross_VectorVector(const vec3& A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = cross(A, B);
}

void NObject::Disable(UObject* Self, const NameString& ProbeFunc)
{
	Self->DisableEvent(ProbeFunc);
}

// The original's deletes the object at once, whatever still refers to it
// (dx-reverse-info core-dll.md, CriticalDelete). DXEngine's flags it to go at
// the next collection, which makes every reference to it None: until then
// it is as it was, so a script that reads the object it just deleted (as
// NanoKeyRing.RemoveAllKeys does) reads it whole. An actor is destroyed
// instead, and goes with the destroyed actors (ULevel::CleanupDestroyed):
// flagged at once, its link in their chain was made None at the next
// collection, cutting the chain behind it; one whose Destroy refuses
// (bStatic, bNoDelete) stays, logged, where the original frees it with the
// level's list, its BSP node and the collision hash still pointing at it.
// Code, a package and the engine's own objects -- the levels, their infos
// and games, the save info and the root window among them -- are not
// deleted: the main loop reads them unchecked.
void NObject::CriticalDelete(UObject* Self, UObject* myObject)
{
	if (!myObject || AnyFlags(myObject->Flags, ObjectFlags::EliminateObject))
		return;
	bool engineHeld = myObject == engine->client || myObject == engine->viewport || myObject == engine->canvas || myObject == engine->console ||
		myObject == engine->audiodev || myObject == engine->renderdev || myObject == engine->netdev || myObject == engine->gameengine || myObject == engine->dxgc ||
		myObject == engine->LevelInfo || myObject == engine->GameInfo || myObject == engine->DeusExLevelInfo || myObject == engine->dxSaveInfo || myObject == engine->dxRootWindow ||
		myObject == engine->EntryLevelInfo || myObject == engine->EntryGameInfo || myObject == engine->EntryDeusExLevelInfo;
	if (UObject::TryCast<UField>(myObject) || UObject::TryCast<UPackage>(myObject) || UObject::TryCast<USubsystem>(myObject) || UObject::TryCast<ULevelBase>(myObject) || engineHeld)
	{
		LogMessage("CriticalDelete: not deleting " + UObject::GetUClassName(myObject).ToString() + " " + myObject->Name.ToString());
		return;
	}
	if (UActor* actor = UObject::TryCast<UActor>(myObject))
	{
		if (!actor->Destroy())
			LogMessage("CriticalDelete: Destroy refused " + UObject::GetUClassName(myObject).ToString() + " " + myObject->Name.ToString());
		return;
	}
	myObject->Flags |= ObjectFlags::EliminateObject;
}

// Deus Ex's timing natives (core-dll.md, clock, unclock and CyclesToSeconds)
// count the steady clock's nanoseconds, where the original's count the CPU's
// cycles: clock takes the count's low 32 bits from its argument, unclock
// adds them back, and CyclesToSeconds turns a count into seconds. The
// original's unclock also takes off 34 cycles, its own cost.
static uint32_t TimingCount()
{
	return (uint32_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void NObject::Clock(int& cycles)
{
	cycles = (int)((uint32_t)cycles - TimingCount());
}

void NObject::Unclock(int& cycles)
{
	cycles = (int)((uint32_t)cycles + TimingCount());
}

void NObject::CyclesToSeconds(int cycles, float& ReturnValue)
{
	ReturnValue = (float)(cycles * 1e-9);
}

void NObject::DivideEqual_ByteByte(uint8_t& A, uint8_t B, uint8_t& ReturnValue)
{
	// By zero: 0, and the byte keeps its value, as the original answers.
	ReturnValue = B != 0 ? (A /= B) : 0;
}

void NObject::DivideEqual_FloatFloat(float& A, float B, float& ReturnValue)
{
	ReturnValue = A /= B;
}

// The original's (Core.dll 0x10134370): 0 for a divisor of 0.0, else the
// quotient made an int by _ftol (floating.h's ftol, wrapping past an int's
// range); a C++ cast is undefined there. The quotient is as the original's
// x87 holds it in the tick the script runs in (X87): rounded to a float at
// 24 bits in the console's tick (OpsConsole), whole in the level's.
void NObject::DivideEqual_IntFloat(int& A, float B, int& ReturnValue)
{
	ReturnValue = A = B != 0.0f ? ftol(X87::Round((double)A / B)) : 0;
}

void NObject::DivideEqual_RotatorFloat(Rotator& A, float B, Rotator& ReturnValue)
{
	ReturnValue = A /= B;
}

void NObject::DivideEqual_VectorFloat(vec3& A, float B, vec3& ReturnValue)
{
	ReturnValue = A /= B;
}

void NObject::Divide_FloatFloat(float A, float B, float& ReturnValue)
{
	ReturnValue = A / B;
}

void NObject::Divide_IntInt(int A, int B, int& ReturnValue)
{
	// By zero: 0, as the original answers; the CPU exception would kill the
	// engine. INT_MIN by -1: INT_MIN, the wrap, where the original's idiv
	// faults (Core.dll 0x10133710 guards the zero alone) and C++ is undefined,
	// a SIGFPE on x86 and INT_MIN on aarch64.
	if (B == 0)
		ReturnValue = 0;
	else if (B == -1)
		ReturnValue = (int)(0u - (uint32_t)A);
	else
		ReturnValue = A / B;
}

void NObject::Divide_RotatorFloat(const Rotator& A, float B, Rotator& ReturnValue)
{
	ReturnValue = A / B;
}

void NObject::Divide_VectorFloat(const vec3& A, float B, vec3& ReturnValue)
{
	ReturnValue = A / B;
}

void NObject::Dot_VectorVector(const vec3& A, const vec3& B, float& ReturnValue)
{
	ReturnValue = dot(A, B);
}

void NObject::DynamicLoadObject(const std::string& ObjectName, UObject* ObjectClass, std::optional<bool> MayFail, UObject*& ReturnValue)
{
	ReturnValue = nullptr;

	if (!ObjectName.empty())
	{
		auto dotpos = ObjectName.find('.');
		if (dotpos != 0 && dotpos != std::string::npos)
		{
			std::string packageName = ObjectName.substr(0, dotpos);
			std::string objectName = ObjectName.substr(dotpos + 1);

			try
			{
				// Deus Ex's looks in the first part's package for the name
				// after the last dot, in whatever group, of exactly the class
				// asked for (Core.dll StaticLoadObject, dx-reverse-info
				// core-dll.md, the natives): Effects.Laser.LaserBeam1 and
				// Effects.Wrong.LaserBeam1 are the one FireTexture, which a
				// Texture asked for does not find.
				ReturnValue = engine->packages->GetPackage(packageName)->LoadObjectAnyGroup(UObject::Cast<UClass>(ObjectClass), ObjectName.substr(ObjectName.rfind('.') + 1));
			}
			catch (...)
			{
			}
		}
	}

	if (!ReturnValue && (!MayFail || *MayFail == false))
	{
		LogMessage("Object.DynamicLoadObject: could not load '" + ObjectName + "'");
	}
}

void NObject::Enable(UObject* Self, const NameString& ProbeFunc)
{
	Self->EnableEvent(ProbeFunc);
}

void NObject::EqualEqual_BoolBool(bool A, bool B, BitfieldBool& ReturnValue)
{
	ReturnValue = (A == B);
}

void NObject::EqualEqual_FloatFloat(float A, float B, BitfieldBool& ReturnValue)
{
	// Exact, as the original's (Core.dll 0x10135620) and as != is
	ReturnValue = (A == B);
}

void NObject::EqualEqual_IntInt(int A, int B, BitfieldBool& ReturnValue)
{
	ReturnValue = (A == B);
}

void NObject::EqualEqual_NameName(const NameString& A, const NameString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = (A == B);
}

void NObject::EqualEqual_ObjectObject(UObject* A, UObject* B, BitfieldBool& ReturnValue)
{
	ReturnValue = (A == B);
}

void NObject::EqualEqual_RotatorRotator(const Rotator& A, const Rotator& B, BitfieldBool& ReturnValue)
{
	ReturnValue = identical(A, B);
}

void NObject::EqualEqual_StrStr(const ScriptString& A, const ScriptString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = (A == B);
}

void NObject::EqualEqual_VectorVector(const vec3& A, const vec3& B, BitfieldBool& ReturnValue)
{
	ReturnValue = (A == B);
}

void NObject::Exp(float A, float& ReturnValue)
{
	ReturnValue = std::exp(A);
}

// Clamp's order (execFClamp): its x87 compare takes a NaN for under A
void NObject::FClamp(float V, float A, float B, float& ReturnValue)
{
	ReturnValue = !(V >= A) ? A : !(V >= B) ? V : B;
}

void NObject::FMax(float A, float B, float& ReturnValue)
{
	ReturnValue = std::max(A, B);
}

void NObject::FMin(float A, float B, float& ReturnValue)
{
	ReturnValue = std::min(A, B);
}

void NObject::FRand(float& ReturnValue)
{
	// ReturnValue = (float)(std::rand() / (double)RAND_MAX);
	ReturnValue = ::FRand();
}

void NObject::GetAxes(const Rotator& A, vec3& X, vec3& Y, vec3& Z)
{
	Coords::Rotation(A).GetAxes(X, Y, Z);
}

void NObject::GetEnum(UObject* E, int i, NameString& ReturnValue)
{
	// Its first name too (an empty key slot's None), as the original's
	// (Core.dll 0x1013e9a0)
	UEnum* type = UObject::TryCast<UEnum>(E);
	if (type && i >= 0 && i < (int)type->ElementNames.size())
		ReturnValue = type->ElementNames[i];
	else
		ReturnValue = {};
}

// Deus Ex's are the original's (Core.dll 0x1013e0b0, 0x1013e320): the
// property the class's field walk finds, its own first
// (UStruct::FindProperty), read only if public, as ExportText writes its
// first element, and set only if not const; a private one, or a name the
// class has not, reads empty and sets nothing.
void NObject::GetPropertyText(UObject* Self, const std::string& PropName, std::string& ReturnValue)
{
	ReturnValue = "";
	UProperty* prop = Self->PropertyData.Class->FindProperty(PropName);
	if (prop && AllFlags(prop->Flags, ObjectFlags::Public))
		ReturnValue = Self->ExportPropertyText(prop);
}

// The node a script's state natives start from, as the original's state
// frame has it (Core.dll's execGetStateName 0x1013edb0, execIsInState
// 0x1013ece0): the state; in no state the class, as the frame starts so
// (UObject::InitExecution 0x10150750, every actor's and the console's) and a
// GotoState to no state sets it (0x1012e8f0); none for another object with no
// frame, which the original never gives one. DXEngine makes a frame at the
// first GotoState, so an actor without one is in no state.
static UStruct* StateNode(UObject* Self)
{
	if (Self->StateFrame && Self->StateFrame->State)
		return Self->StateFrame->State;
	if (Self->StateFrame || UObject::TryCast<UActor>(Self) || UObject::TryCast<UConsole>(Self))
		return Self->Class;
	return nullptr;
}

// The GotoState native compares with UObject::GetStateName, None in no
// state, as execGotoState does.
void NObject::GetStateName(UObject* Self, NameString& ReturnValue)
{
	UStruct* node = StateNode(Self);
	ReturnValue = node ? node->Name : NameString();
}

void NObject::GetUnAxes(const Rotator& A, vec3& X, vec3& Y, vec3& Z)
{
	// The original's are GetAxes' transposed (RotatorConsole)
	Coords axes = Coords::Rotation(A);
	X = { axes.XAxis.x, axes.YAxis.x, axes.ZAxis.x };
	Y = { axes.XAxis.y, axes.YAxis.y, axes.ZAxis.y };
	Z = { axes.XAxis.z, axes.YAxis.z, axes.ZAxis.z };
}

void NObject::GotoState(UObject* Self, std::optional<NameString> NewState, std::optional<NameString> Label)
{
	// As the original's execGotoState (Core.dll 0x1013dd00): into the state
	// the object is in (the one omitted names; None for no state), only the
	// label, Begin by default -- no EndState or BeginState, and the probes
	// as they are, so a Disable in BeginState holds (a sitting NPC's
	// AnimEnd, which its Bump on the chair's way sends to CircleToFront).
	// UObject::GotoState, which sets the probe mask, only for another state.
	NameString current = Self->GetStateName();
	NameString stateName = NewState ? *NewState : current;
	NameString labelName = Label ? *Label : NameString();
	if (stateName == current)
	{
		if (Self->StateFrame && Self->StateFrame->Func)
			Self->StateFrame->GotoLabel(labelName);
		return;
	}
	Self->GotoState(stateName, labelName);
}

void NObject::GreaterEqual_FloatFloat(float A, float B, BitfieldBool& ReturnValue)
{
	ReturnValue = A >= B;
}

void NObject::GreaterEqual_IntInt(int A, int B, BitfieldBool& ReturnValue)
{
	ReturnValue = A >= B;
}

void NObject::GreaterEqual_StrStr(const ScriptString& A, const ScriptString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = A >= B;
}

// The shifts take the count's low 5 bits, as the original's x86 shift does;
// C++ is undefined at 32 and over or below 0
void NObject::GreaterGreaterGreater_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = (int)(static_cast<unsigned int>(A) >> (B & 31));
}

void NObject::GreaterGreater_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = A >> (B & 31);
}

void NObject::GreaterGreater_VectorRotator(const vec3& A, const Rotator& B, vec3& ReturnValue)
{
	// The sine table's, as the original's (Core.dll 0x101380e0)
	ReturnValue = Coords::InverseRotation(B) * A;
}

void NObject::Greater_FloatFloat(float A, float B, BitfieldBool& ReturnValue)
{
	ReturnValue = A > B;
}

void NObject::Greater_IntInt(int A, int B, BitfieldBool& ReturnValue)
{
	ReturnValue = A > B;
}

void NObject::Greater_StrStr(const ScriptString& A, const ScriptString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = A > B;
}

void NObject::InStr(const ScriptString& S, const ScriptString& t, int& ReturnValue)
{
	auto pos = S.find(t);
	ReturnValue = (pos != ScriptString::npos) ? (int)pos : -1;
}

void NObject::Invert(vec3& X, vec3& Y, vec3& Z)
{
	mat3 m;
	m[0] = X.x; m[1] = X.y; m[2] = X.z;
	m[3] = Y.x; m[4] = Y.y; m[5] = Y.z;
	m[6] = Z.x; m[7] = Z.y; m[8] = Z.z;
	m = mat3::inverse(m);
	X.x = m[0]; X.y = m[1]; X.z = m[2];
	Y.x = m[3]; Y.y = m[4]; Y.z = m[5];
	Z.x = m[6]; Z.y = m[7]; Z.z = m[8];
}

void NObject::IsA(UObject* Self, const NameString& ClassName, BitfieldBool& ReturnValue)
{
	ReturnValue = Self->IsA(ClassName);
}

// The original's (Core.dll's execIsInState 0x1013ece0): the state frame's
// node or any it extends -- in no state the class and its supers, the class
// being the node there (StateNode).
void NObject::IsInState(UObject* Self, const NameString& TestState, BitfieldBool& ReturnValue)
{
	ReturnValue = false;
	for (UStruct* state = StateNode(Self); state; state = state->BaseStruct)
	{
		if (state->Name == TestState)
		{
			ReturnValue = true;
			return;
		}
	}
}

void NObject::Left(const ScriptString& S, int i, ScriptString& ReturnValue)
{
	ReturnValue = S.substr(0, clamp(i, 0, (int)S.size()));
}

void NObject::Len(const ScriptString& S, int& ReturnValue)
{
	ReturnValue = (int)S.size();
}

void NObject::Lerp(float Alpha, float A, float B, float& ReturnValue)
{
	ReturnValue = mix(A, B, Alpha);
}

void NObject::LessEqual_FloatFloat(float A, float B, BitfieldBool& ReturnValue)
{
	ReturnValue = A <= B;
}

void NObject::LessEqual_IntInt(int A, int B, BitfieldBool& ReturnValue)
{
	ReturnValue = A <= B;
}

void NObject::LessEqual_StrStr(const ScriptString& A, const ScriptString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = A <= B;
}

void NObject::LessLess_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = (int)(static_cast<unsigned int>(A) << (B & 31));
}

void NObject::LessLess_VectorRotator(const vec3& A, const Rotator& B, vec3& ReturnValue)
{
	// The sine table's, as the original's (Core.dll 0x10137d90)
	ReturnValue = Coords::Rotation(B) * A;
}

void NObject::Less_FloatFloat(float A, float B, BitfieldBool& ReturnValue)
{
	ReturnValue = A < B;
}

void NObject::Less_IntInt(int A, int B, BitfieldBool& ReturnValue)
{
	ReturnValue = A < B;
}

void NObject::Less_StrStr(const ScriptString& A, const ScriptString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = A < B;
}

void NObject::Localize(const std::string& SectionName, const std::string& KeyName, const std::string& PackageName, std::string& ReturnValue)
{
	// A key no .int has: the original's line in the log, and its marker for
	// the text (dx-reverse-info core-dll.md, the .int files).
	if (!engine->packages->FindLocalized(PackageName, SectionName, KeyName, ReturnValue))
	{
		std::string key = PackageName + "." + SectionName + "." + KeyName;
		LogMessage("No localization: " + key + " (int)");
		ReturnValue = "<?int?" + key + "?>";
	}
}

void NObject::Log(const std::string& S, std::optional<NameString> Tag)
{
	if (Tag)
		LogMessage("[" + Tag->ToString() + "] " + S);
	else
		LogMessage(S);
}

void NObject::Loge(float A, float& ReturnValue)
{
	ReturnValue = std::log(A);
}

void NObject::Max(int A, int B, int& ReturnValue)
{
	ReturnValue = std::max(A, B);
}

void NObject::Mid(const ScriptString& S, int i, std::optional<int> j, ScriptString& ReturnValue)
{
	// As the original's FString::Mid (Core.dll 0x101086e0; core-dll.md,
	// Conversions): up to 65,535 characters by default, and both ends clamped
	// as unsigned -- so a negative start wraps past the end and gives an empty
	// string, and a negative count that takes the end below 0 wraps the same
	// way and gives the rest of the string.
	uint32_t size = (uint32_t)S.size();
	uint32_t start = (uint32_t)i;
	uint32_t end = start + (uint32_t)(j ? *j : 65535);
	if (start > size)
		start = size;
	if (end < start)
		end = start;
	else if (end > size)
		end = size;
	ReturnValue = S.substr(start, end - start);
}

void NObject::Min(int A, int B, int& ReturnValue)
{
	ReturnValue = std::min(A, B);
}

void NObject::MirrorVectorByNormal(const vec3& Vect, const vec3& Normal, vec3& ReturnValue)
{
	ReturnValue = reflect(Vect, Normal);
}

void NObject::MultiplyEqual_ByteByte(uint8_t& A, uint8_t B, uint8_t& ReturnValue)
{
	ReturnValue = A *= B;
}

void NObject::MultiplyEqual_FloatFloat(float& A, float B, float& ReturnValue)
{
	ReturnValue = A *= B;
}

// The original's (Core.dll 0x10134280): the product made an int by _ftol
// (ftol, wrapping past an int's range), the product as its x87 holds it in
// the tick the script runs in (X87): 7 *= 3e9 is -474,836,992 in the
// console's tick, the product rounded to a float at 24 bits, 512 under the
// exact product's low bits (OpsConsole), and -474,836,480 in the level's
// (PrecisionConsole)
void NObject::MultiplyEqual_IntFloat(int& A, float B, int& ReturnValue)
{
	ReturnValue = A = ftol(X87::Round((double)A * B));
}

void NObject::MultiplyEqual_RotatorFloat(Rotator& A, float B, Rotator& ReturnValue)
{
	ReturnValue = A *= B;
}

void NObject::MultiplyEqual_VectorFloat(vec3& A, float B, vec3& ReturnValue)
{
	ReturnValue = A *= B;
}

void NObject::MultiplyEqual_VectorVector(vec3& A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = A *= B;
}

void NObject::MultiplyMultiply_FloatFloat(float A, float B, float& ReturnValue)
{
	ReturnValue = std::pow(A, B);
}

void NObject::Multiply_FloatFloat(float A, float B, float& ReturnValue)
{
	ReturnValue = A * B;
}

void NObject::Multiply_FloatRotator(float A, const Rotator& B, Rotator& ReturnValue)
{
	ReturnValue = A * B;
}

void NObject::Multiply_FloatVector(float A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = A * B;
}

void NObject::Multiply_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = A * B;
}

void NObject::Multiply_RotatorFloat(const Rotator& A, float B, Rotator& ReturnValue)
{
	ReturnValue = A * B;
}

void NObject::Multiply_VectorFloat(const vec3& A, float B, vec3& ReturnValue)
{
	ReturnValue = A * B;
}

void NObject::Multiply_VectorVector(const vec3& A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = A * B;
}

// The original's FVector::SafeNormal (Core.dll): zero under a squared
// length of 1e-8, otherwise scaled by one over the root, in double.
void NObject::Normal(const vec3& A, vec3& ReturnValue)
{
	double squared = (double)A.x * A.x + (double)A.y * A.y + (double)A.z * A.z;
	if (squared < 0.00000001)
	{
		ReturnValue = vec3(0.0f);
		return;
	}
	double scale = 1.0 / std::sqrt(squared);
	ReturnValue = vec3((float)(scale * A.x), (float)(scale * A.y), (float)(scale * A.z));
}

void NObject::Normalize(const Rotator& Rot, Rotator& ReturnValue)
{
	ReturnValue = normalize(Rot);
}

void NObject::NotEqual_BoolBool(bool A, bool B, BitfieldBool& ReturnValue)
{
	ReturnValue = A != B;
}

void NObject::NotEqual_FloatFloat(float A, float B, BitfieldBool& ReturnValue)
{
	ReturnValue = A != B;
}

void NObject::NotEqual_IntInt(int A, int B, BitfieldBool& ReturnValue)
{
	ReturnValue = A != B;
}

void NObject::NotEqual_NameName(const NameString& A, const NameString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = A != B;
}

void NObject::NotEqual_ObjectObject(UObject* A, UObject* B, BitfieldBool& ReturnValue)
{
	ReturnValue = A != B;
}

void NObject::NotEqual_RotatorRotator(const Rotator& A, const Rotator& B, BitfieldBool& ReturnValue)
{
	ReturnValue = !identical(A, B);
}

void NObject::NotEqual_StrStr(const ScriptString& A, const ScriptString& B, BitfieldBool& ReturnValue)
{
	ReturnValue = A != B;
}

void NObject::NotEqual_VectorVector(const vec3& A, const vec3& B, BitfieldBool& ReturnValue)
{
	ReturnValue = A != B;
}

void NObject::Not_PreBool(bool A, BitfieldBool& ReturnValue)
{
	ReturnValue = !A;
}

void NObject::Or_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = A | B;
}

// The original's (Core.dll execOrthoRotation 0x1013a700): the rotation of
// the three axes as coords (FCoords::OrthoRotation)
void NObject::OrthoRotation(const vec3& X, const vec3& Y, const vec3& Z, Rotator& ReturnValue)
{
	Coords c;
	c.Origin = vec3(0.0f);
	c.XAxis = X;
	c.YAxis = Y;
	c.ZAxis = Z;
	ReturnValue = c.OrthoRotation();
}

void NObject::Percent_FloatFloat(float A, float B, float& ReturnValue)
{
	ReturnValue = std::fmod(A, B);
}

void NObject::Rand(int Max, int& ReturnValue)
{
	// Note: Object.Rand is Max exclusive, while RandInt is Max inclusive!
	ReturnValue = RandInt(std::min(std::max(Max - 1, 0), 32767));
}

void NObject::RandRange(UObject* Self, float Min, float Max, float& ReturnValue)
{
	ReturnValue = FRandRange(Min, Max);
}

// The config read again into a class's defaults or an object, its base
// classes' sections first, as the original's LoadConfig propagates it.
static void LoadConfigPropagated(UClass* cls, PropertyDataBlock* block)
{
	if (!(cls->ClsFlags & ClassFlags::Config))
		return;
	if (UClass* base = UObject::TryCast<UClass>(cls->BaseStruct))
		LoadConfigPropagated(base, block);
	cls->LoadProperties(block);
}

static bool IsClassOrSubclass(UStruct* cls, UClass* of)
{
	for (UStruct* s = cls; s; s = s->BaseStruct)
	{
		if (s == of)
			return true;
	}
	return false;
}

// As the original's (Core.dll 0x10151ac0; core-dll.md, configuration), of
// the object's class -- a static call's context is the class itself: for a
// class of System or User config, its section is copied key by key into the
// live ini from Default.ini or DefUser.ini, the keys the default file lacks
// keeping their values, and the config read again into the class's
// defaults and every subclass's and into every object of them. A class of
// any other config is left alone. Each object is then told of it
// (PostEditChange).
void NObject::ResetConfig(UObject* Self)
{
	UClass* cls = UObject::TryCast<UClass>(Self);
	if (!cls)
		cls = Self->Class;
	NameString configName = cls->ClassConfigName;
	if (configName.IsNone())
		configName = "system";
	if (configName != "system" && configName != "user")
		return;

	engine->packages->ResetIniSection(configName, cls->package->GetPackageName().ToString() + "." + cls->Name.ToString());

	for (GCObject* gcObj : GC::GetObjects())
	{
		UObject* obj = dynamic_cast<UObject*>(gcObj);
		if (!obj || AnyFlags(obj->Flags, ObjectFlags::EliminateObject) || obj->IsDefaultsObject())
			continue;
		if (UClass* objClass = UObject::TryCast<UClass>(obj))
		{
			if (IsClassOrSubclass(objClass, cls))
				LoadConfigPropagated(objClass, &objClass->PropertyData);
		}
		else if (IsClassOrSubclass(obj->Class, cls))
		{
			LoadConfigPropagated(obj->Class, &obj->PropertyData);
			obj->PostEditChange();
		}
	}
}

void NObject::Right(const ScriptString& S, int i, ScriptString& ReturnValue)
{
	int count = clamp(i, 0, (int)S.size());
	ReturnValue = S.substr(S.size() - count);
}

void NObject::RotRand(std::optional<bool> bRoll, Rotator& ReturnValue)
{
	ReturnValue.Yaw = (int)(std::rand() * 0xffffLL / RAND_MAX);
	ReturnValue.Pitch = (int)(std::rand() * 0xffffLL / RAND_MAX);
	ReturnValue.Roll = (bRoll && *bRoll) ? (int)(rand() * 65535LL / RAND_MAX) : 0;
}

void NObject::SaveConfig(UObject* Self)
{
	Self->SaveConfig();
}

void NObject::SetPropertyText(UObject* Self, const std::string& PropName, const std::string& PropValue)
{
	UProperty* prop = Self->PropertyData.Class->FindProperty(PropName);
	if (prop && AllFlags(prop->Flags, ObjectFlags::Public) && !AnyFlags(prop->PropFlags, PropertyFlags::Const))
		prop->SetValueFromString(Self->PropertyData.Ptr(prop), PropValue);
}

void NObject::Sin(float A, float& ReturnValue)
{
	ReturnValue = std::sin(A);
}

void NObject::Smerp(float Alpha, float A, float B, float& ReturnValue)
{
	ReturnValue = (float)(A + (3.0 * Alpha * Alpha - 2.0 * Alpha * Alpha * Alpha) * (B - A));
}

void NObject::Sqrt(float A, float& ReturnValue)
{
	ReturnValue = std::sqrt(A);
}

void NObject::Square(float A, float& ReturnValue)
{
	ReturnValue = A * A;
}

// The original's (Core.dll execStaticSaveConfig 0x1013e7c0): the class's
// defaults, from a class context (Self the class itself) and from an
// object's alike, never the object's own values
void NObject::StaticSaveConfig(UObject* Class)
{
	UClass* cls = UObject::TryCast<UClass>(Class);
	if (!cls)
		cls = Class->Class;
	cls->SaveConfig();
}

void NObject::SubtractEqual_ByteByte(uint8_t& A, uint8_t B, uint8_t& ReturnValue)
{
	ReturnValue = A -= B;
}

void NObject::SubtractEqual_FloatFloat(float& A, float B, float& ReturnValue)
{
	ReturnValue = A -= B;
}

void NObject::SubtractEqual_IntInt(int& A, int B, int& ReturnValue)
{
	ReturnValue = A -= B;
}

void NObject::SubtractEqual_RotatorRotator(Rotator& A, const Rotator& B, Rotator& ReturnValue)
{
	ReturnValue = A -= B;
}

void NObject::SubtractEqual_VectorVector(vec3& A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = A -= B;
}

void NObject::SubtractSubtract_Byte(uint8_t& A, uint8_t& ReturnValue)
{
	ReturnValue = A--;
}

void NObject::SubtractSubtract_Int(int& A, int& ReturnValue)
{
	ReturnValue = A--;
}

void NObject::SubtractSubtract_PreByte(uint8_t& A, uint8_t& ReturnValue)
{
	ReturnValue = --A;
}

void NObject::SubtractSubtract_PreInt(int& A, int& ReturnValue)
{
	ReturnValue = --A;
}

void NObject::Subtract_FloatFloat(float A, float B, float& ReturnValue)
{
	ReturnValue = A - B;
}

void NObject::Subtract_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = A - B;
}

void NObject::Subtract_PreFloat(float A, float& ReturnValue)
{
	ReturnValue = -A;
}

void NObject::Subtract_PreInt(int A, int& ReturnValue)
{
	ReturnValue = -A;
}

void NObject::Subtract_PreVector(const vec3& A, vec3& ReturnValue)
{
	ReturnValue = vec3(0.0f) - A;
}

void NObject::Subtract_RotatorRotator(const Rotator& A, const Rotator& B, Rotator& ReturnValue)
{
	ReturnValue = A - B;
}

void NObject::Subtract_VectorVector(const vec3& A, const vec3& B, vec3& ReturnValue)
{
	ReturnValue = A - B;
}

void NObject::Tan(float A, float& ReturnValue)
{
	ReturnValue = std::tan(A);
}

void NObject::VRand(vec3& ReturnValue)
{
	// The original keeps the points it draws inside the unit sphere, which
	// lean nowhere; those outside lean toward the cube's diagonals.
	while (true)
	{
		vec3 randomVector = vec3(FRandRange(-1.0f, 1.0f), FRandRange(-1.0f, 1.0f), FRandRange(-1.0f, 1.0f));
		float lengthSq = dot(randomVector, randomVector);
		if (lengthSq > 0.0f && lengthSq <= 1.0f)
		{
			ReturnValue = normalize(randomVector);
			break;
		}
	}
}

void NObject::VSize(const vec3& A, float& ReturnValue)
{
	ReturnValue = length(A);
}

void NObject::Warn(const std::string& S)
{
	LogMessage("Warning: " + S);
}

void NObject::XorXor_BoolBool(bool A, bool B, BitfieldBool& ReturnValue)
{
	ReturnValue = !A ^ !B;
}

void NObject::Xor_IntInt(int A, int B, int& ReturnValue)
{
	ReturnValue = A ^ B;
}

// None for the class is Object, as the original's (Core.dll execAllObjects
// 0x1013f1f0); DXEngine dereferenced it
void NObject::AllObjects_DeusEx(UObject* Self, UObject* BaseClass, UObject*& Actor)
{
	if (!BaseClass)
	{
		UStruct* cls = Self->Class;
		while (cls->BaseStruct)
			cls = cls->BaseStruct;
		BaseClass = cls;
	}
	Frame::CreatedIterator = std::make_unique<AllObjectsIterator>(BaseClass, &Actor);
}

void NObject::GetConfig(const std::string& section, const std::string& key, std::string& ReturnValue)
{
	// Deus Ex, called by name (no native number): a key's value in the system
	// ini, or an empty string when the section or the key is not there.
	ReturnValue = engine->packages->GetIniValue("system", section, key, "");
}
