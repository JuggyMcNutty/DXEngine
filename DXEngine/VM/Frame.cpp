
#include "Precomp.h"
#include <cstring>
#include "Utils/StrTools.h"
#include "Frame.h"
#include "Bytecode.h"
#include "Expression.h"
#include "ExpressionEvaluator.h"
#include "NativeFunc.h"
#include "Packages/Core/UTextBuffer.h"
#include "Packages/Core/UFunction.h"
#include "VM/ScriptCall.h"
#include "Packages/Core/Properties/UByteProperty.h"
#include "Packages/Core/Properties/UIntProperty.h"
#include "Packages/Core/Properties/UFloatProperty.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UNameProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Engine.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Package/PackageManager.h"
#include "Network/NetDriver.h"
#include "Network/NetRemote.h"
#include "Utils/AlignedAlloc.h"
#include "VM/PrintPrettyExpression.h"

Array<Frame*> Frame::Callstack;
std::string Frame::ExceptionText;
bool Frame::Halted = false;
std::unique_ptr<Iterator> Frame::CreatedIterator;
bool Frame::Guards = false;
bool Frame::NoRunaway = false;
int Frame::Jumps = 0;
int Frame::ScriptDepth = 0;

Frame::Frame(UObject* instance, UStruct* func)
{
	Object = instance;
	SetState(func);
}

void Frame::Mark(GCMarker& marker)
{
	// Its object is the one that holds it, marked already.
	marker.MarkConst(Func);
	marker.MarkConst(State);
	if (Variables.Func && Variables.Data)
	{
		for (UProperty* prop : Variables.Func->RefProps())
			prop->MarkProperty(marker, static_cast<uint8_t*>(Variables.Data) + prop->DataOffset.DataOffset);
	}
	marker.SetField("Iterators");
	for (auto& iterator : Iterators)
		iterator->Mark(marker);
}

void Frame::MarkNames()
{
	if (Variables.Func && Variables.Data)
	{
		for (UProperty* prop : Variables.Func->NameProps())
			prop->MarkPropertyNames(static_cast<uint8_t*>(Variables.Data) + prop->DataOffset.DataOffset);
	}
}

void Frame::SetState(UStruct* func)
{
	Func = func;
	State = func;
	Variables.Init(func);
}

void Frame::Break()
{
	Halted = true;

	if (!ExceptionText.empty())
	{
		std::string callstack = Frame::GetCallstack();
		std::string message = "Script execution error:\r\n\r\n";
		message += ExceptionText;
		message += "\r\n\r\nCall stack:\r\n\r\n" + callstack;
		Exception::Throw(message);
	}
}

// The original logs these as critical errors: the game stops with the
// message; a runaway loop runs on with -norunaway, its count started again
// either way (Core.dll execJump 0x1012faf0)
void Frame::RunawayLoop()
{
	Jumps = 0;
	if (NoRunaway)
		return;
	throw ScriptCriticalError("Script execution error:\r\n\r\nRunaway loop detected (over 10000000 iterations)\r\n\r\nCall stack:\r\n\r\n" + GetCallstack());
}

void Frame::InfiniteRecursion()
{
	throw ScriptCriticalError("Script execution error:\r\n\r\nInfinite script recursion (250 calls) detected\r\n\r\nCall stack:\r\n\r\n" + GetCallstack());
}

// A singular function's mark is its object's flag of the function flag's own
// bit, as the original's: SourceModified, which no package's object carries
// (the SDK's InSingularFunc goes unused)
Frame::GuardedRun::GuardedRun(UFunction* func, UObject* instance)
{
	if (!Guards)
		return;
	if (AllFlags(func->FuncFlags, FunctionFlags::Singular))
	{
		if (AllFlags(instance->Flags, ObjectFlags::SourceModified))
		{
			HeldOff = true;
			return;
		}
		instance->Flags |= ObjectFlags::SourceModified;
		Singular = instance;
	}
	ScriptDepth++;
	Counted = true;
}

Frame::GuardedRun::~GuardedRun()
{
	if (Singular)
		Singular->Flags = Singular->Flags & (ObjectFlags)~(uint32_t)ObjectFlags::SourceModified;
	if (Counted)
		ScriptDepth--;
}

void Frame::ThrowException(const std::string& text)
{
#if defined(_DEBUG) && defined(WIN32)
	DebugBreak();
#endif

	ExceptionText = text;
	Break();
}

std::string Frame::GetName()
{
	std::string name;
	if (Func)
	{
		for (UStruct* s = Func; s != nullptr; s = s->StructParent)
		{
			if (name.empty())
				name = s->Name.ToString();
			else
				name = s->Name.ToString() + "." + name;
		}
	}
	return name;
}

std::string Frame::GetDisassembly(Expression* statement)
{
	std::string result;
	PrintPrettyExpression::Print([&](const std::string& text) { result += text; }, statement);
	return result;
}

std::string Frame::GetCallstack()
{
	std::string result;

#ifdef WIN32
	std::string newline = "\r\n";
#else
	std::string newline = "\n";
#endif

	for (auto it = Callstack.rbegin(); it != Callstack.rend(); ++it)
	{
		Frame* frame = *it;
		std::string name = frame->GetName();
		if (UStruct* func = frame->Func)
		{
			name += " line " + std::to_string(func->Line);

			if (frame->StatementIndex > 0) // StatementIndex points at the NEXT statement to be executed
			{
				name += ": ";
				name += GetDisassembly(func->Code->Statements[frame->StatementIndex - 1]);
			}
		}
		if (!result.empty()) result += newline;
		result += "at " + name;
	}
	return result;
}

const Array<UProperty*>& Frame::CallParms(UFunction* func)
{
	if (!func->CallParmsReady)
	{
		// A function's Properties are its UProperty children in order, collected at load
		for (UProperty* prop : func->Properties)
		{
			if (AllFlags(prop->PropFlags, PropertyFlags::Parm))
				func->CallParms.push_back(prop);
			if (!func->ReturnParm && AllFlags(prop->PropFlags, PropertyFlags::Parm | PropertyFlags::ReturnParm))
				func->ReturnParm = prop;
		}
		func->CallParmsReady = true;
	}
	return func->CallParms;
}

size_t CallArguments::Room(UFunction* func, size_t passed)
{
	// Frame::Call adds optional parameters up to the last, then a native's return value
	return std::max(passed, Frame::CallParms(func).size()) + 1;
}

// A value stored whole: a static array element by element, as the
// original's CopyCompleteValue copies every element
static void StoreWhole(ExpressionValue& lvalue, ExpressionValue& value, UProperty* parm)
{
	if (parm->ArrayDimension > 1 && lvalue.IsVariable() && value.IsVariable())
	{
		for (int element = 0; element < parm->ArrayDimension; element++)
			lvalue.ItemAt(element).Store(value.ItemAt(element));
	}
	else
	{
		lvalue.Store(value);
	}
}

CallArguments::ArrayCopy::ArrayCopy(UProperty* prop) : Prop(prop)
{
	Data = AlignedAlloc(prop->ElementAlignment(), prop->ArrayPitch());
	prop->ConstructArray(Data);
}

CallArguments::ArrayCopy::~ArrayCopy()
{
	if (Data)
	{
		Prop->DestructArray(Data);
		AlignedFree(Data);
	}
}

ExpressionValue CallArguments::CopyOf(ExpressionValue& variable, UProperty* parm)
{
	if (!Copies)
		Copies = std::make_unique<Array<ArrayCopy>>();
	ExpressionValue copy = ExpressionValue::At(Copies->emplace_back(parm)->Data, parm);
	StoreWhole(copy, variable, parm);
	return copy;
}

// A script function's call held off, or sent to the other side, runs
// nothing, but the original's CallFunction (Core.dll 0x1013f4e0) copies the
// frame's out parameters back all the same, the last first: a variable a
// later argument changed takes back its value as it was reached (SetStart)
static void CopyStartsBack(UFunction* func, CallArguments& args)
{
	const Array<UProperty*>& parms = Frame::CallParms(func);
	for (size_t i = std::min(parms.size(), args.size()); i-- > 0;)
	{
		if (ExpressionValue* start = args.StartOf(i))
			StoreWhole(args[i], *start, parms[i]);
	}
}

ExpressionValue Frame::Call(UFunction* func, UObject* instance, Array<ExpressionValue> args)
{
	CallArguments callArgs(CallArguments::Room(func, args.size()));
	for (ExpressionValue& arg : args)
		callArgs.push_back(std::move(arg));
	return Call(func, instance, callArgs);
}

ExpressionValue Frame::Call(UFunction* func, UObject* instance, CallArguments& args, ExpressionValue* into)
{
	if (!instance)
	{
		LogMessage("Accessed None when calling " + func->Name.ToString());
		LogMessage(Frame::GetCallstack());
		return ExpressionValue::NothingValue();
	}

	TraceCall(func, instance, args);

	// In a net game an actor's call may go to the other side instead, or not
	// run at all; a native with its own number never does either. Either way
	// it writes nothing, as the original's: ProcessInternal (Core.dll
	// 0x1013f690) asks the object's ProcessRemoteFunction first (an actor's,
	// Engine.dll 0x103e5d90) and, when that takes the call, runs none of it
	// and steps nothing into the caller's result; CallFunction (0x1013f4e0)
	// so calls no native without its own number that it takes. A variable
	// assigned from it keeps its value, a bool is cleared (CallScript); its
	// out arguments are copied back from the frame (CopyStartsBack).
	if (engine->LevelNetDriver && !(AllFlags(func->FuncFlags, FunctionFlags::Native) && func->NativeFuncIndex != 0) && NetProcessRemoteFunction(func, instance, args))
	{
		CopyStartsBack(func, args);
		return ExpressionValue::NothingValue(true);
	}

	// Whether func is an event is a property of its name: look it up once
	if (func->EventIndex == -2)
	{
		EventName eventName = {};
		func->EventIndex = NameStringToEventName(func->Name, eventName) ? (int)eventName : -1;
	}
	// A script call of a function named after a probe runs only with its
	// bit set; any other call runs, nothing checked, as the original's. One
	// held off writes nothing, as the original's ProcessInternal (Core.dll
	// 0x1013f690) skips the run: a variable assigned from it keeps its value.
	bool enabled = func->EventIndex >= 0 ? instance->IsEventEnabled((EventName)func->EventIndex) : true;
	if (!enabled)
	{
		CopyStartsBack(func, args);
		return ExpressionValue::NothingValue(true);
	}

	// Trailing optional args may be missing. Add nothing values so the args list matches the function signature.
	const Array<UProperty*>& parms = CallParms(func);
	for (size_t i = args.size(); i < parms.size() && AllFlags(parms[i]->PropFlags, PropertyFlags::Parm | PropertyFlags::OptionalParm); i++)
		args.push_back(ExpressionValue::NothingValue());

	if (AllFlags(func->FuncFlags, FunctionFlags::Native))
	{
		return CallNative(func, instance, args);
	}
	else
	{
		return CallScript(func, instance, args, into);
	}
}

ExpressionValue Frame::CallScript(UFunction* func, UObject* instance, CallArguments& args, ExpressionValue* into)
{
	// Deus Ex: a singular function held off gives nothing, a zero to whatever
	// reads it, and leaves a variable assigned from it as it was, as the
	// original's, whose Let has the call write its result in the variable
	GuardedRun guarded(func, instance);
	if (guarded.HeldOff)
	{
		CopyStartsBack(func, args);
		return ExpressionValue::NothingValue(true);
	}
	if (ScriptDepth > 250)
		InfiniteRecursion();

	Frame frame(instance, func);
	frame.Into = into;
	const Array<UProperty*>& parms = func->CallParms;

	// Store args in function frame local variables. An optional argument the
	// caller left out keeps the value the frame gave it, zero, as in UE1: a
	// struct or an array would be copied from nothing.
	for (size_t i = 0; i < parms.size() && i < args.size(); i++)
	{
		ExpressionValue& arg = args.Start(i);
		if (arg.GetType() == ExpressionValueType::Nothing)
			continue;
		ExpressionValue lvalue = ExpressionValue::Variable(frame.Variables.Data, parms[i]);
		// A static array goes whole, element by element, when the caller
		// gives one (a native's buffer: ReceivedBinary's bytes)
		StoreWhole(lvalue, arg, parms[i]);
	}

	// Run the function
	ExpressionValue result = frame.Run().Value;

	// Load the result from the frame local result variable
	result.Load();

	// A function that ends without a value -- off its end, or at a bare
	// return -- writes nothing to its caller's result: the original's
	// ProcessInternal (Core.dll 0x1013f690) steps the return's expression
	// into the caller's own buffer, the variable itself for a Let (execLet,
	// 0x1012fe40), and its EX_Nothing (execNothing, 0x10102095) writes
	// nothing. So the variable keeps its value, and whatever else reads the
	// result reads its own zero (ExpressionEvaluator: a bool's LetBool, a
	// condition, an argument). A value the return zero-fills, a None
	// context's, is the type's zero; a string's through None, which it does
	// not fill, keeps the variable too.
	if (func->ReturnParm && result.GetType() == ExpressionValueType::Nothing && !result.KeepsVariable)
	{
		result = ExpressionValue::DefaultValue(func->ReturnParm);
	}

	// A Let's call writes its value into the variable itself, then copies
	// its out parameters back, as the original's CallFunction (Core.dll
	// 0x1013f4e0) runs the function into the caller's buffer, the variable
	// for a Let (execLet 0x1012fe40), and copies them back after: an out
	// parameter that is also the variable ends with the parameter's value.
	// The Let then stores nothing.
	if (into && result.GetType() != ExpressionValueType::Nothing)
	{
		into->Store(result);
		result = ExpressionValue::NothingValue(true);
	}

	// Copy out params from frame local variables, the last first, as the
	// original's CallFunction (Core.dll 0x1013f4e0) copies them back: of two
	// given the same variable, the first one's value is the one it keeps; a
	// static array every element (CopyCompleteValue)
	for (size_t i = std::min(parms.size(), args.size()); i-- > 0;)
	{
		if (AllFlags(parms[i]->PropFlags, PropertyFlags::OutParm))
		{
			ExpressionValue lvalue = ExpressionValue::Variable(frame.Variables.Data, parms[i]);
			StoreWhole(args[i], lvalue, parms[i]);
		}
	}

	return result;
}

ExpressionValue Frame::CallNative(UFunction* func, UObject* instance, CallArguments& args)
{
	// Native functions expect the last parameter to be the return value
	bool returnparmfound = func->ReturnParm != nullptr;
	if (returnparmfound)
		args.push_back(ExpressionValue::PropertyValue(func->ReturnParm));

	if (func->NativeFuncIndex != 0)
	{
		// The table reaches only the highest number registered: a mod's
		// native numbered past it has no handler either
		static const NativeFuncHandler unregistered;
		const NativeFuncHandler& callback = (size_t)func->NativeFuncIndex < NativeFunctions::NativeByIndex.size() ? NativeFunctions::NativeByIndex[func->NativeFuncIndex] : unregistered;
		if (callback)
		{
			Frame frame(instance, func, Frame::NoLocals());
			ActiveCallStackFrame activeFrame(&frame);
			try
			{
				callback(instance, args.data());
			}
			catch (const ScriptCriticalError&)
			{
				throw;
			}
			catch (const std::exception& e)
			{
				// A script error under the native (an event it called) has
				// halted the scripts: it stops the game as any other does
				if (Frame::Halted)
					throw;
				LogMessage(std::string("Script error: ") + e.what());
				return ExpressionValue::NothingValue();
			}
			catch (...)
			{
				if (Frame::Halted)
					throw;
				LogMessage("Script error: Unknown error");
				return ExpressionValue::NothingValue();
			}
		}
		else
		{
			Exception::Throw("Unknown native function " + func->NativeStruct->Name.ToString() + "." + func->Name.ToString());
		}
	}
	else
	{
		auto& callback = NativeFunctions::NativeByName[{ func->Name, func->NativeStruct->Name }];
		if (callback)
		{
			Frame frame(instance, func, Frame::NoLocals());
			ActiveCallStackFrame activeFrame(&frame);
			try
			{
				callback(instance, args.data());
			}
			catch (const ScriptCriticalError&)
			{
				throw;
			}
			catch (const std::exception& e)
			{
				// A script error under the native (an event it called) has
				// halted the scripts: it stops the game as any other does
				if (Frame::Halted)
					throw;
				LogMessage(std::string("Script error: ") + e.what());
				return ExpressionValue::NothingValue();
			}
			catch (...)
			{
				if (Frame::Halted)
					throw;
				LogMessage("Script error: Unknown error");
				return ExpressionValue::NothingValue();
			}
		}
		else
		{
			Exception::Throw("Unknown native function " + func->NativeStruct->Name.ToString() + "." + func->Name.ToString());
		}
	}

	return returnparmfound ? std::move(args.back()) : ExpressionValue::NothingValue();
}

void Frame::TraceCall(UFunction* func, UObject* instance, const CallArguments& args)
{
#if 0 // To do: create a commandlet that lets us do this
	static NameString TraceActorClass = "CTFGame";
	static NameString TraceActorFunc = "PostBeginPlay";
	if (instance->Class->Name == TraceActorClass && func->Name == TraceActorFunc)
	{
		LogMessage("RemainingBots=" + std::to_string(instance->GetInt("RemainingBots")));
		LogMessage("InitialBots=" + std::to_string(instance->GetInt("InitialBots")));
		std::string traceMessage = "Called " + func->Name.ToString() + "(";
		bool first = true;
		for (auto& arg : args)
		{
			if (first)
				first = false;
			else
				traceMessage += ", ";
			if (arg.GetType() == ExpressionValueType::ValueString)
			{
				traceMessage += "\"";
				traceMessage += arg.ToString();
				traceMessage += "\"";
			}
			else if (arg.GetType() == ExpressionValueType::ValueName)
			{
				traceMessage += "'";
				traceMessage += arg.ToName().ToString();
				traceMessage += "'";
			}
			else if (arg.GetType() == ExpressionValueType::ValueBool)
			{
				traceMessage += "'";
				traceMessage += arg.ToBool() ? "true" : "false";
				traceMessage += "'";
			}
			else if (arg.GetType() == ExpressionValueType::ValueObject)
			{
				traceMessage += arg.ToObject() ? UObject::GetUClassName(arg.ToObject()).ToString() : "null";
			}
			else if (arg.GetType() == ExpressionValueType::Nothing)
			{
				traceMessage += "None";
			}
			else
			{
				traceMessage += "?";
			}
		}
		traceMessage += ")";
		LogMessage(traceMessage);
	}
#endif
}

// The original's UObject::GotoLabel (Core.dll 0x1012eb10): the label looked
// for in the object's state and the states above it -- the one a state
// extends, a parent class's of the name -- and the code then that state's,
// the object's state kept. None having it stops the state code.
bool Frame::FindLabel(const NameString& label)
{
	for (UStruct* s = State; s; s = s->BaseStruct)
	{
		UState* state = UObject::TryCast<UState>(s);
		if (!state || !state->Code)
			break;
		int labelIndex = state->Code->FindLabelIndex(label);
		if (labelIndex != -1)
		{
			Func = state;
			StatementIndex = labelIndex;
			return true;
		}
	}
	// A state whose super link did not load: the states of its name up the classes
	for (UClass* cls = State ? Object->Class : nullptr; cls != nullptr; cls = static_cast<UClass*>(cls->BaseStruct))
	{
		UState* state = cls->GetState(State->Name);
		if (state && state->Code)
		{
			int labelIndex = state->Code->FindLabelIndex(label);
			if (labelIndex != -1)
			{
				Func = state;
				StatementIndex = labelIndex;
				return true;
			}
		}
	}
	return false;
}

bool Frame::GotoLabel(const NameString& label)
{
	bool found = FindLabel(label.IsNone() ? NameString("Begin") : label);
	LatentState = found ? LatentRunState::Continue : LatentRunState::Stop;
	return found;
}

void Frame::Tick()
{
	if (LatentState == LatentRunState::Continue)
		Run();
}

ExpressionEvalResult Frame::Run()
{
	if (!Func)
		return {};

	ActiveCallStackFrame activeFrame(this);

	// A state frame's run ends as the original's ProcessState (Engine.dll
	// 0x103e5b60) ends it: before a statement once the actor is destroyed,
	// and at the fifth state change of the tick (a GotoState moves the frame
	// to the state's code, Func), which the next tick's run takes up. It ran
	// on after Destroy, and for ever through a bounce between two states by
	// GotoState alone, which the runaway guard never counted.
	const bool stateFrame = Object->StateFrame.get() == this;
	UActor* stateActor = stateFrame ? UObject::TryCast<UActor>(Object) : nullptr;
	UStruct* stateCode = Func;
	int stateChanges = 0;

	const int maxInstructions = 500'000;
	int instructionsRetired = 0;
	while (true)
	{
		if (stateFrame)
		{
			if (stateActor && stateActor->bDeleteMe())
				return {};
			if (Func != stateCode)
			{
				stateCode = Func;
				if (++stateChanges > 4)
					return {};
			}
		}

		if (StatementIndex >= Func->Code->Statements.size())
			ThrowException("Unexpected end of code statements");

		// Note: GotoState may change StatementIndex (jump to a different location) so we have to increment the index before executing the statement
		size_t curStatementIndex = StatementIndex;
		StatementIndex++;

		if (instructionsRetired >= maxInstructions && !Guards)
		{
			LogMessage("Too many VM instructions executed in a single tick");
			Break();
		}

		Expression* statement = Func->Code->Statements[curStatementIndex];
		if (statement->Statement == Expression::StatementKind::Unknown)
			ClassifyStatement(statement, *Func->Code);

		// The commonest statements -- conditions, jumps, assignments to plain
		// variables, calls, return;, the next pass of a foreach -- run in
		// place, without the ExpressionEvalResult Eval makes for each; the
		// general path below does the same with each one's result. Only this
		// object's state frame can lose its Func or wait on a latent function,
		// and what its Run returns is not used.
		if (statement->Statement != Expression::StatementKind::General)
		{
			switch (statement->Statement)
			{
			default:
				break;
			case Expression::StatementKind::Jump:
				CountJump();
				StatementIndex = static_cast<JumpExpression*>(statement)->Target;
				break;
			case Expression::StatementKind::JumpIfNot:
				{
					CountJump();
					auto jump = static_cast<JumpIfNotExpression*>(statement);
					UStruct* func = Func;
					bool taken = !ExpressionEvaluator::Condition(jump, Object, Variables.Data);
					if (!Func)
						return {};
					// A GotoState in the condition moves the frame to other code
					if (taken)
						StatementIndex = Func == func ? jump->Target : Func->Code->FindStatementIndex(jump->Offset);
					break;
				}
			case Expression::StatementKind::Let:
				ExpressionEvaluator::Assignment(static_cast<LetExpression*>(statement)->LeftSide, static_cast<LetExpression*>(statement)->RightSide, Object, Variables.Data);
				if (!Func)
					return {};
				break;
			case Expression::StatementKind::LetBool:
				ExpressionEvaluator::Assignment(static_cast<LetBoolExpression*>(statement)->LeftSide, static_cast<LetBoolExpression*>(statement)->RightSide, Object, Variables.Data);
				if (!Func)
					return {};
				break;
			case Expression::StatementKind::Call:
				ExpressionEvaluator::CallStatement(statement, Object, Variables.Data);
				if (!Func)
					return {};
				break;
			case Expression::StatementKind::ReturnNothing:
				{
					// A bare return, and the one every function ends with:
					// its EX_Nothing writes nothing to the caller's result
					// (CallScript)
					ExpressionEvalResult result;
					result.Result = StatementResult::Return;
					result.Value = ExpressionValue::NothingValue(true);
					return result;
				}
			case Expression::StatementKind::IteratorNext:
				if (Iterators.empty())
					ThrowException("Iterator next statement without an iterator in " + Object->Name.ToString() + "." + Func->Name.ToString());
				if (Iterators.back()->Next())
					StatementIndex = Iterators.back()->StartStatementIndex;
				else
					StatementIndex = Iterators.back()->EndStatementIndex;
				break;
			}

			if (Object->StateFrame.get() == this && LatentState != LatentRunState::Continue)
				return {};

			instructionsRetired++;
			continue;
		}

		if (statement->Statement == Expression::StatementKind::Jump || statement->Statement == Expression::StatementKind::JumpIfNot)
			CountJump();
		ExpressionEvalResult result = ExpressionEvaluator::Eval(statement, Object, Object, Variables.Data, Into);
		if (!Func)
			return result;
		switch (result.Result)
		{
		case StatementResult::Next:
			break;
		case StatementResult::Jump:
			StatementIndex = Func->Code->FindStatementIndex(result.JumpAddress);
			break;
		case StatementResult::Switch:
			ProcessSwitch(result.Value);
			break;
		case StatementResult::GotoLabel:
			{
				// The label in the running node, else in the object's state
				// and the states above it (FindLabel)
				int index = Func->Code->FindLabelIndex(result.Label);
				if (index != -1)
					StatementIndex = index;
				else if (!FindLabel(result.Label))
					ThrowException("Could not find label: " + result.Label.ToString());
			}
			break;
		case StatementResult::Stop:
			LatentState = LatentRunState::Stop;
			return result;
		case StatementResult::Return:
			// Package 61 and earlier transfered the return value in an out parameter
			if (!static_cast<ReturnExpression*>(statement)->Value)
			{
				for (UField* field = Func->Children; field != nullptr; field = field->Next)
				{
					UProperty* prop = UObject::TryCast<UProperty>(field);
					if (prop && AllFlags(prop->PropFlags, PropertyFlags::Parm | PropertyFlags::ReturnParm))
					{
						result.Value = ExpressionValue::Variable(Variables.Data, prop);
						result.Value.Load();
						break;
					}
				}
			}

			return result;
		case StatementResult::Iterator:
			if (!result.Iter)
				ThrowException("Iterator statement without an iterator in " + Object->Name.ToString() + "." + Func->Name.ToString());
			Iterators.push_back(std::move(result.Iter));
			Iterators.back()->StartStatementIndex = curStatementIndex + 1;
			Iterators.back()->EndStatementIndex = Func->Code->FindStatementIndex(result.JumpAddress);
			if (Iterators.back()->Next())
				StatementIndex = Iterators.back()->StartStatementIndex;
			else
				StatementIndex = Iterators.back()->EndStatementIndex;
			break;
		case StatementResult::IteratorNext:
			if (Iterators.empty())
				ThrowException("Iterator next statement without an iterator in " + Object->Name.ToString() + "." + Func->Name.ToString());
			if (Iterators.back()->Next())
				StatementIndex = Iterators.back()->StartStatementIndex;
			else
				StatementIndex = Iterators.back()->EndStatementIndex;
			break;
		case StatementResult::IteratorPop:
			if (Iterators.empty())
				ThrowException("Iterator pop statement without an iterator in " + Object->Name.ToString() + "." + Func->Name.ToString());
			Iterators.pop_back();
			break;
		case StatementResult::AccessedNone:
			LogMessage("Accessed None");
			LogMessage(Frame::GetCallstack());
			break;
		}

		if (Object->StateFrame.get() == this && LatentState != LatentRunState::Continue)
		{
			return result;
		}

		instructionsRetired++;
	}
}

void Frame::ClassifyStatement(Expression* statement, const Bytecode& code)
{
	Expression::StatementKind kind = Expression::StatementKind::General;
	if (auto jump = dynamic_cast<JumpExpression*>(statement))
	{
		jump->Target = code.FindStatementIndex(jump->Offset);
		kind = Expression::StatementKind::Jump;
	}
	else if (auto jumpIfNot = dynamic_cast<JumpIfNotExpression*>(statement))
	{
		jumpIfNot->Target = code.FindStatementIndex(jumpIfNot->Offset);
		kind = Expression::StatementKind::JumpIfNot;
	}
	else if (auto let = dynamic_cast<LetExpression*>(statement))
	{
		if (ExpressionEvaluator::IsAssignable(let->LeftSide))
			kind = Expression::StatementKind::Let;
	}
	else if (auto letBool = dynamic_cast<LetBoolExpression*>(statement))
	{
		if (ExpressionEvaluator::IsAssignable(letBool->LeftSide))
			kind = Expression::StatementKind::LetBool;
	}
	else if (dynamic_cast<VirtualFunctionExpression*>(statement) || dynamic_cast<FinalFunctionExpression*>(statement) ||
		dynamic_cast<GlobalFunctionExpression*>(statement) || dynamic_cast<NativeFunctionExpression*>(statement))
	{
		kind = Expression::StatementKind::Call;
	}
	else if (auto ret = dynamic_cast<ReturnExpression*>(statement))
	{
		if (ret->Value && dynamic_cast<NothingExpression*>(ret->Value))
			kind = Expression::StatementKind::ReturnNothing;
	}
	else if (dynamic_cast<IteratorNextExpression*>(statement))
	{
		kind = Expression::StatementKind::IteratorNext;
	}
	statement->Statement = kind;
}

// The original's execSwitch (Core.dll 0x1012f850): a string case matches
// without regard to case (appStricmp); any other type by its bytes
// (appMemcmp over the switch's size), so a float case by the condition's
// bits, -0.0 and 0.0 apart
static bool CaseMatches(const ExpressionValue& condition, const ExpressionValue& casevalue)
{
	switch (condition.GetType())
	{
	case ExpressionValueType::ValueString:
		return StrTools::equals_ignore_case(condition.ToString(), casevalue.ToString());
	case ExpressionValueType::ValueFloat:
	{
		float a = condition.ToFloat(), b = casevalue.ToFloat();
		return memcmp(&a, &b, sizeof(float)) == 0;
	}
	case ExpressionValueType::ValueVector:
	{
		vec3 a = condition.ToVector(), b = casevalue.ToVector();
		return memcmp(&a, &b, sizeof(vec3)) == 0;
	}
	default:
		return condition.IsEqual(casevalue);
	}
}

void Frame::ProcessSwitch(ExpressionValue& condition)
{
	while (true)
	{
		CaseExpression* caseexpr = static_cast<CaseExpression*>(Func->Code->Statements[StatementIndex++]);
		if (caseexpr->Value)
		{
			// The original's execSwitch (Core.dll 0x1012f850) steps the
			// condition into a buffer of its own, once: a variable is read
			// before a case that may change it
			if (condition.IsVariable() && ExpressionEvaluator::MayChange(caseexpr->Value))
				condition.Load();
			ExpressionValue casevalue = ExpressionEvaluator::Eval(caseexpr->Value, Object, Object, Variables.Data).Value;
			if (CaseMatches(condition, casevalue))
				break;
			else
				StatementIndex = Func->Code->FindStatementIndex(caseexpr->NextOffset);
		}
		else
		{
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

// Whether every property of s constructs to zero bytes and has nothing to
// destruct: numbers, bools, names, object references, and structs of those.
static bool IsPlainData(UStruct* s)
{
	for (UProperty* prop : s->Properties)
	{
		if (UStructProperty* structProp = UObject::TryCast<UStructProperty>(prop))
		{
			if (!structProp->Struct || !IsPlainData(structProp->Struct))
				return false;
		}
		else if (!UObject::TryCast<UByteProperty>(prop) && !UObject::TryCast<UIntProperty>(prop) &&
			!UObject::TryCast<UFloatProperty>(prop) && !UObject::TryCast<UBoolProperty>(prop) &&
			!UObject::TryCast<UNameProperty>(prop) && !UObject::TryCast<UObjectProperty>(prop))
		{
			return false;
		}
	}
	return true;
}

void LocalVariables::Init(UStruct* func)
{
	Reset();
	Func = func;
	if (func)
	{
		if (func->StructSize <= sizeof(Inline) && func->StructAlignment <= 16)
			Data = Inline;
		else
			Data = AlignedAlloc(func->StructAlignment, func->StructSize);

		// Most functions' locals are plain data, and constructing them one
		// virtual call at a time was a measurable part of every script call
		if (func->PlainDataLocals < 0)
			func->PlainDataLocals = IsPlainData(func) ? 1 : 0;

		if (func->PlainDataLocals)
		{
			memset(Data, 0, func->StructSize);
		}
		else
		{
			for (UProperty* prop : func->Properties)
			{
				prop->ConstructArray(static_cast<uint8_t*>(Data) + prop->DataOffset.DataOffset);
			}
		}
	}
}

void LocalVariables::Reset()
{
	if (Func && Data && !Func->PlainDataLocals)
	{
		for (UProperty* prop : Func->Properties)
		{
			prop->DestructArray(static_cast<uint8_t*>(Data) + prop->DataOffset.DataOffset);
		}
	}

	if (Data != Inline)
		AlignedFree(Data);
	Func = nullptr;
	Data = nullptr;
}
