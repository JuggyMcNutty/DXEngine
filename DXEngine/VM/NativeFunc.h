#pragma once

#include "ExpressionValue.h"
#include "Frame.h"
#include <functional>

class UObject;
class UFunction;
class ExpressionValue;

typedef std::function<void(UObject* self, ExpressionValue* Args)> NativeFuncHandler;

class NativeFunctions
{
public:
	static Array<UFunction*> FuncByIndex;
	static Array<NativeFuncHandler> NativeByIndex;
	static std::map<std::pair<NameString, NameString>, NativeFuncHandler> NativeByName;
	static Array<LatentRunState> LatentActionByIndex;
	static std::map<LatentRunState, int> IndexForLatentAction;

	static void RegisterHandler(const NameString& className, const NameString& funcName, int nativeIndex, NativeFuncHandler handler);
	static void RegisterNativeFunc(UFunction* func);
};

void RegisterLatentAction(int nativeIndex, LatentRunState latentAction);

// A native's argument from its value. A script's string is 16-bit
// (ScriptString), and a native that takes the engine's own text gets it as
// UTF-8: by value or const reference a copy, and by reference a copy that
// goes back into the value once the call is done, the temporary lasting the
// call's whole expression.
class NativeStringRef
{
public:
	NativeStringRef(ExpressionValue& value) : Target(&value.ToType<ScriptString&>()), Text(ScriptToUTF8(*Target)) {}
	NativeStringRef(const NativeStringRef&) = delete;
	NativeStringRef& operator=(const NativeStringRef&) = delete;
	~NativeStringRef() { *Target = UTF8ToScript(Text); }
	operator std::string&() { return Text; }

private:
	ScriptString* Target;
	std::string Text;
};

template<typename T>
decltype(auto) NativeArg(ExpressionValue& value)
{
	if constexpr (std::is_same_v<T, const std::string&>)
		return value.ToString();
	else if constexpr (std::is_same_v<T, const ScriptString&>)
		return value.ToScriptString();
	else if constexpr (std::is_same_v<T, std::string&>)
		return NativeStringRef(value);
	else
		return value.ToType<T>();
}

// Static native functions:

inline void RegisterVMNativeFunc_0(const std::string& className, const std::string& funcName, void(*func)(), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func();
	});
}

template<typename Arg1>
void RegisterVMNativeFunc_1(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]));
	});
}

template<typename Arg1, typename Arg2>
void RegisterVMNativeFunc_2(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3>
void RegisterVMNativeFunc_3(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4>
void RegisterVMNativeFunc_4(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
void RegisterVMNativeFunc_5(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
void RegisterVMNativeFunc_6(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
void RegisterVMNativeFunc_7(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
void RegisterVMNativeFunc_8(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
void RegisterVMNativeFunc_9(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
void RegisterVMNativeFunc_10(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11>
void RegisterVMNativeFunc_11(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12>
void RegisterVMNativeFunc_12(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13>
void RegisterVMNativeFunc_13(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12, Arg13 arg13), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]), NativeArg<Arg13>(Args[12]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14>
void RegisterVMNativeFunc_14(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12, Arg13 arg13, Arg14 arg14), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]), NativeArg<Arg13>(Args[12]), NativeArg<Arg14>(Args[13]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15>
void RegisterVMNativeFunc_15(const std::string& className, const std::string& funcName, void(*func)(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12, Arg13 arg13, Arg14 arg14, Arg15 arg15), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]), NativeArg<Arg13>(Args[12]), NativeArg<Arg14>(Args[13]), NativeArg<Arg15>(Args[14]));
		});
}

// Instance native functions:

inline void RegisterVMNativeFunc_0(const std::string& className, const std::string& funcName, void(*func)(UObject* self), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self);
	});
}

template<typename Arg1>
void RegisterVMNativeFunc_1(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]));
	});
}

template<typename Arg1, typename Arg2>
void RegisterVMNativeFunc_2(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3>
void RegisterVMNativeFunc_3(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4>
void RegisterVMNativeFunc_4(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
void RegisterVMNativeFunc_5(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
void RegisterVMNativeFunc_6(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
void RegisterVMNativeFunc_7(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8>
void RegisterVMNativeFunc_8(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
void RegisterVMNativeFunc_9(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10>
void RegisterVMNativeFunc_10(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
	{
		func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]));
	});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11>
void RegisterVMNativeFunc_11(const std::string& className, const std::string& funcName, void(*func)(UObject* self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12>
void RegisterVMNativeFunc_12(const std::string& className, const std::string& funcName, void(*func)(UObject* Self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13>
void RegisterVMNativeFunc_13(const std::string& className, const std::string& funcName, void(*func)(UObject* Self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12, Arg13 arg13), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]), NativeArg<Arg13>(Args[12]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14>
void RegisterVMNativeFunc_14(const std::string& className, const std::string& funcName, void(*func)(UObject* Self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12, Arg13 arg13, Arg14 arg14), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]), NativeArg<Arg13>(Args[12]), NativeArg<Arg14>(Args[13]));
		});
}

template<typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9, typename Arg10, typename Arg11, typename Arg12, typename Arg13, typename Arg14, typename Arg15>
void RegisterVMNativeFunc_15(const std::string& className, const std::string& funcName, void(*func)(UObject* Self, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9, Arg10 arg10, Arg11 arg11, Arg12 arg12, Arg13 arg13, Arg14 arg14, Arg15 arg15), int nativeIndex)
{
	NativeFunctions::RegisterHandler(className, funcName, nativeIndex, [=](UObject* self, ExpressionValue* Args)
		{
			func(self, NativeArg<Arg1>(Args[0]), NativeArg<Arg2>(Args[1]), NativeArg<Arg3>(Args[2]), NativeArg<Arg4>(Args[3]), NativeArg<Arg5>(Args[4]), NativeArg<Arg6>(Args[5]), NativeArg<Arg7>(Args[6]), NativeArg<Arg8>(Args[7]), NativeArg<Arg9>(Args[8]), NativeArg<Arg10>(Args[9]), NativeArg<Arg11>(Args[10]), NativeArg<Arg12>(Args[11]), NativeArg<Arg13>(Args[12]), NativeArg<Arg14>(Args[13]), NativeArg<Arg15>(Args[14]));
		});
}
