
#include "Precomp.h"
#include "Utils/StrTools.h"
#include "ExpressionEvaluator.h"
#include "Expression.h"
#include "Bytecode.h"
#include "Frame.h"
#include "NativeFunc.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UFunction.h"
#include "ScriptCall.h"

#include <cstdio>

using TypedKind = Expression::TypedKind;

static bool IsVariable(TypedKind kind)
{
	return kind >= TypedKind::LocalByte && kind <= TypedKind::InstanceBoolVariable;
}

// A bool variable's kinds: LetBool alone assigns them
static bool IsBoolVariable(TypedKind kind)
{
	return kind == TypedKind::LocalBool || kind == TypedKind::InstanceBool || kind == TypedKind::LocalBoolVariable || kind == TypedKind::InstanceBoolVariable;
}

ExpressionEvalResult ExpressionEvaluator::Eval(Expression* expr, UObject* self, UObject* context, void* localVariables, ExpressionValue* into)
{
	ExpressionEvalResult result;
	ExpressionEvaluator evaluator(result, self, context, localVariables);
	evaluator.ReturnInto = into;
	expr->Visit(&evaluator);
	return result;
}

ExpressionEvalResult ExpressionEvaluator::NoResult;

bool ExpressionEvaluator::Condition(JumpIfNotExpression* statement, UObject* self, void* localVariables)
{
	ExpressionEvaluator evaluator(self, localVariables);
	return evaluator.EvalBool(statement->Condition);
}

void ExpressionEvaluator::Assignment(Expression* lhs, Expression* rhs, UObject* self, void* localVariables)
{
	ExpressionEvaluator evaluator(self, localVariables);
	evaluator.Assign(lhs, rhs);
}

void ExpressionEvaluator::CallStatement(Expression* statement, UObject* self, void* localVariables)
{
	// No call's Expr reports anything to the frame: only its value, dropped
	ExpressionEvaluator evaluator(self, localVariables);
	ExpressionValue value;
	evaluator.Out = &value;
	statement->Visit(&evaluator);
}

bool ExpressionEvaluator::IsAssignable(Expression* lhs)
{
	if (lhs->Typed == TypedKind::Unknown)
		Classify(lhs);
	return IsVariable(lhs->Typed);
}

ExpressionValue ExpressionEvaluator::Value(Expression* expr)
{
	// The commonest leaves, made here as their Expr would make them.
	switch (expr->Leaf)
	{
	default: break;
	case Expression::LeafKind::LocalVariable: return ExpressionValue::Variable(LocalVariables, static_cast<LocalVariableExpression*>(expr)->Variable);
	case Expression::LeafKind::InstanceVariable: return ExpressionValue::Variable(Context->PropertyData.Data, static_cast<InstanceVariableExpression*>(expr)->Variable);
	case Expression::LeafKind::BoolVariable: return Value(static_cast<BoolVariableExpression*>(expr)->Variable);
	case Expression::LeafKind::Self: return ExpressionValue::ObjectValue(Self);
	case Expression::LeafKind::NoObject: return ExpressionValue::ObjectValue(nullptr);
	case Expression::LeafKind::ObjectConst: return ExpressionValue::ObjectValue(static_cast<ObjectConstExpression*>(expr)->Object);
	case Expression::LeafKind::NameConst: return ExpressionValue::NameValue(static_cast<NameConstExpression*>(expr)->Value);
	case Expression::LeafKind::IntConst: return ExpressionValue::IntValue(static_cast<IntConstExpression*>(expr)->Value);
	case Expression::LeafKind::IntZero: return ExpressionValue::IntValue(0);
	case Expression::LeafKind::IntOne: return ExpressionValue::IntValue(1);
	case Expression::LeafKind::IntConstByte: return ExpressionValue::ByteValue(static_cast<IntConstByteExpression*>(expr)->Value);
	case Expression::LeafKind::ByteConst: return ExpressionValue::ByteValue(static_cast<ByteConstExpression*>(expr)->Value);
	case Expression::LeafKind::FloatConst: return ExpressionValue::FloatValue(static_cast<FloatConstExpression*>(expr)->Value);
	case Expression::LeafKind::True: return ExpressionValue::BoolValue(true);
	case Expression::LeafKind::False: return ExpressionValue::BoolValue(false);
	case Expression::LeafKind::Nothing: return ExpressionValue::NothingValue();
	}
	return VisitValue(expr);
}

ExpressionValue ExpressionEvaluator::VisitValue(Expression* expr)
{
	ExpressionValue value;
	ExpressionValue* out = Out;
	Out = &value;
	expr->Visit(this);
	Out = out;
	return value;
}

ExpressionValue ExpressionEvaluator::LetValue(Expression* expr, ExpressionValue& variable)
{
	// A leaf calls nothing; through None there is no variable, where the
	// original's Let writes to a scratch of its own
	if (expr->Leaf != Expression::LeafKind::Other || !variable.IsVariable())
		return Value(expr);

	// As VisitValue, the call whose value this is given the variable (Call):
	// a context passes its expression this value's place, as the original's
	// execContext hands on the Let's buffer (0x101300d0); a nested value has
	// a place of its own
	ExpressionValue value;
	ExpressionValue* out = Out;
	Out = &value;
	LetVariable = &variable;
	LetOut = &value;
	expr->Visit(this);
	LetVariable = nullptr;
	LetOut = nullptr;
	Out = out;
	return value;
}

ExpressionValue ExpressionEvaluator::Value(Expression* expr, UObject* context)
{
	UObject* oldContext = Context;
	Context = context;
	ExpressionValue value = Value(expr);
	Context = oldContext;
	return value;
}

void ExpressionEvaluator::PassThrough(Expression* expr, UObject* context)
{
	UObject* oldContext = Context;
	Context = context;
	expr->Visit(this);
	Context = oldContext;
}

void ExpressionEvaluator::Expr(LocalVariableExpression* expr)
{
	*Out = ExpressionValue::Variable(LocalVariables, expr->Variable);
}

void ExpressionEvaluator::Expr(InstanceVariableExpression* expr)
{
	*Out = ExpressionValue::Variable(Context->PropertyData.Data, expr->Variable);
}

void ExpressionEvaluator::Expr(DefaultVariableExpression* expr)
{
	if (UObject::TryCast<UClass>(Context))
		*Out = ExpressionValue::Variable(Context->PropertyData.Data, expr->Variable);
	else
		*Out = ExpressionValue::Variable(Context->Class->GetDefaultObject<UObject>()->PropertyData.Data, expr->Variable);
}

void ExpressionEvaluator::Expr(ReturnExpression* expr)
{
	// A call returned writes its value into the caller's Let's variable
	// itself, before it copies its out parameters back, as the original's
	// return steps it into the caller's buffer (ProcessInternal, Core.dll
	// 0x1013f690)
	if (expr->Value && ReturnInto && IsStatement())
		*Out = LetValue(expr->Value, *ReturnInto);
	else if (expr->Value)
		*Out = Value(expr->Value);
	else
		*Out = ExpressionValue::NothingValue();
	if (IsStatement())
		Result.Result = StatementResult::Return;
}

void ExpressionEvaluator::Expr(SwitchExpression* expr)
{
	*Out = Value(expr->Condition);
	if (IsStatement())
		Result.Result = StatementResult::Switch;
}

void ExpressionEvaluator::Expr(JumpExpression* expr)
{
	if (IsStatement())
	{
		Result.Result = StatementResult::Jump;
		Result.JumpAddress = expr->Offset;
	}
}

void ExpressionEvaluator::Expr(JumpIfNotExpression* expr)
{
	if (!EvalBool(expr->Condition) && IsStatement())
	{
		Result.Result = StatementResult::Jump;
		Result.JumpAddress = expr->Offset;
	}
}

void ExpressionEvaluator::Expr(StopExpression* expr)
{
	if (IsStatement())
		Result.Result = StatementResult::Stop;
}

void ExpressionEvaluator::Expr(AssertExpression* expr)
{
	if (!Value(expr->Condition).ToBool())
	{
		Frame::ThrowException("Script assert failed for " + Self->Name.ToString() + " line " + std::to_string(expr->Line));
	}
}

void ExpressionEvaluator::Expr(CaseExpression* expr)
{
	*Out = ExpressionValue::NothingValue();
}

void ExpressionEvaluator::Expr(NothingExpression* expr)
{
	*Out = ExpressionValue::NothingValue();
}

void ExpressionEvaluator::Expr(LabelTableExpression* expr)
{
	// A state's label table follows its EX_Stop: read as data (FindLabelIndex),
	// never run.
	Frame::ThrowException("Label table expression is not implemented");
}

void ExpressionEvaluator::Expr(GotoLabelExpression* expr)
{
	NameString label = Value(expr->Value).ToName();
	if (IsStatement())
	{
		Result.Result = StatementResult::GotoLabel;
		Result.Label = label;
	}
}

void ExpressionEvaluator::Expr(EatStringExpression* expr)
{
	Value(expr->Value);
	*Out = ExpressionValue::NothingValue();
}

void ExpressionEvaluator::Expr(LetExpression* expr)
{
	if (IsStatement() && Assign(expr->LeftSide, expr->RightSide))
		return;

	ExpressionValue lvalue = Value(expr->LeftSide);
	ExpressionValue rvalue = LetValue(expr->RightSide, lvalue);
	if (lvalue.GetType() != ExpressionValueType::Nothing)
	{
		if (!rvalue.KeepsVariable)
			lvalue.Store(rvalue);
		*Out = std::move(lvalue);
	}
	else
	{
		*Out = std::move(rvalue);
	}
}

void ExpressionEvaluator::Expr(LetBoolExpression* expr)
{
	if (IsStatement() && Assign(expr->LeftSide, expr->RightSide))
		return;

	// The original's LetBool (Core.dll 0x1012ff30) steps the value into a
	// zeroed int of its own and sets or clears the bit by it: a value that
	// writes nothing (a call held off, or ended without one) clears it
	ExpressionValue lvalue = Value(expr->LeftSide);
	ExpressionValue rvalue = Value(expr->RightSide);
	if (lvalue.GetType() != ExpressionValueType::Nothing)
	{
		lvalue.Store(rvalue);
		*Out = std::move(lvalue);
	}
	else
	{
		*Out = std::move(rvalue);
	}
}

void ExpressionEvaluator::Expr(DynArrayElementExpression* expr)
{
	int index = Value(expr->Index).ToInt();
	auto arrayval = Value(expr->Array);
	if (arrayval.IsVariable())
	{
		if (index < 0)
		{
			LogMessage("Negative index used");
			*Out = ExpressionValue::NothingValue();
		}
		else
		{
			*Out = arrayval.DynArrayItemAt(index);
		}
	}
	else
	{
		Frame::ThrowException("Array is not a variable in DynArrayElementExpression");
	}
}

void ExpressionEvaluator::Expr(NewExpression* expr)
{
	// Each argument read as it is evaluated, as the original's execNew
	// (Core.dll 0x1013d9f0) steps each into a value of its own
	ExpressionValue outer = Value(expr->ParentExpr);
	UObject* outerObj = outer.GetType() != ExpressionValueType::Nothing ? outer.ToObject() : nullptr;
	ExpressionValue name = Value(expr->NameExpr);
	NameString objName = name.GetType() == ExpressionValueType::Nothing ? NameString() : name.ToName();
	ExpressionValue flags = Value(expr->FlagsExpr);
	uint32_t newFlags = flags.GetType() == ExpressionValueType::Nothing ? 0 : (uint32_t)flags.ToInt();
	UClass* cls = UObject::Cast<UClass>(Value(expr->ClassExpr).ToObject());

	// As the original's execNew: the object is made in its outer, or with
	// none in the transient package. So new(Self) on the player -- its
	// goals, notes and key infos -- is in the level's package, which a save
	// writes and a level left behind takes with it.
	Package* package = outerObj && outerObj->package ? outerObj->package : engine->packages->GetTransientPackage();

	// An object given no name is named after its class and a number, as
	// the original's StaticConstructObject names it (DeusExNote0).
	if (objName.IsNone() && cls)
		objName = package->MakeUniqueObjectName(cls);

	// A flag outside the script's own, RF_ScriptMask (Transactional, Public,
	// Transient, NotForClient, NotForServer and NotForEdit), is logged and
	// left out, as the original's execNew does (core-dll.md, the natives)
	const uint32_t scriptMask = 0x00704005;
	if (newFlags & ~scriptMask)
	{
		char text[64];
		snprintf(text, sizeof(text), "new: Flags %08X not allowed", newFlags);
		LogMessage(text);
		newFlags &= scriptMask;
	}
	UObject* newObj = package->NewObject(objName, cls, (ObjectFlags)newFlags, true);

	if (outerObj)
		newObj->Outer() = outerObj;

	*Out = ExpressionValue::ObjectValue(newObj);
}

void ExpressionEvaluator::Expr(ClassContextExpression* expr)
{
	ExpressionValue object = Value(expr->ObjectExpr);
	UClass* cls = UObject::TryCast<UClass>(object.ToObject());
	if (cls)
	{
		PassThrough(expr->ContextExpr, cls->GetDefaultObject<UObject>());
	}
	else
	{
		// As an object's context: a warning, the expression skipped, its result nothing
		*Out = ExpressionValue::NothingValue(expr->ZeroFillSize == 0);
		if (IsStatement())
			Result.Result = StatementResult::AccessedNone;
	}
}

void ExpressionEvaluator::Expr(MetaCastExpression* expr)
{
	UObject* value = Value(expr->Value).ToObject();
	if (value && value != expr->Class)
	{
		UClass* cls = UObject::TryCast<UClass>(value);
		while (cls)
		{
			if (cls == expr->Class)
				break;
			cls = static_cast<UClass*>(cls->BaseStruct);
		}
		if (!cls)
			value = nullptr;
	}
	*Out = ExpressionValue::ObjectValue(value);
}

void ExpressionEvaluator::Expr(Unknown0x15Expression* expr)
{
	// Klingon honor guard has this! (UE 251)
	//Frame::ThrowException("Unknown0x15 expression encountered");
	if (IsStatement())
		Result.Result = StatementResult::Stop;
}

void ExpressionEvaluator::Expr(SelfExpression* expr)
{
	*Out = ExpressionValue::ObjectValue(Self);
}

void ExpressionEvaluator::Expr(SkipExpression* expr)
{
	PassThrough(expr->Value, Context);
}

void ExpressionEvaluator::Expr(ContextExpression* expr)
{
	UObject* context = Value(expr->ObjectExpr).ToObject();
	if (context)
	{
		PassThrough(expr->ContextExpr, context);
	}
	else
	{
		// Nothing, a zero to whatever reads it, as the original's execContext
		// zero-fills the result -- by the size the bytecode gives, which is
		// none for a string: a variable assigned from one keeps its value
		*Out = ExpressionValue::NothingValue(expr->ZeroFillSize == 0);
		if (IsStatement())
			Result.Result = StatementResult::AccessedNone;
	}
}

void ExpressionEvaluator::Expr(ArrayElementExpression* expr)
{
	int index = Value(expr->Index).ToInt();
	auto arrayval = Value(expr->Array);
	if (arrayval.IsVariable())
	{
		*Out = arrayval.ItemAt(index);
	}
	else
	{
		// An array through a None context (`A.MultiSkins[1]` with A None):
		// nothing, as the original's execArrayElement leaves it -- no address
		// to read or write, so a Let through it stores nothing and a variable
		// assigned from it keeps its value
		*Out = ExpressionValue::NothingValue(true);
	}
}

void ExpressionEvaluator::Expr(IntConstExpression* expr)
{
	*Out = ExpressionValue::IntValue(expr->Value);
}

void ExpressionEvaluator::Expr(FloatConstExpression* expr)
{
	*Out = ExpressionValue::FloatValue(expr->Value);
}

void ExpressionEvaluator::Expr(StringConstExpression* expr)
{
	*Out = ExpressionValue::StringValue(expr->Value);
}

void ExpressionEvaluator::Expr(ObjectConstExpression* expr)
{
	*Out = ExpressionValue::ObjectValue(expr->Object);
}

void ExpressionEvaluator::Expr(NameConstExpression* expr)
{
	*Out = ExpressionValue::NameValue(expr->Value);
}

void ExpressionEvaluator::Expr(RotationConstExpression* expr)
{
	*Out = ExpressionValue::RotatorValue({ expr->Pitch, expr->Yaw, expr->Roll });
}

void ExpressionEvaluator::Expr(VectorConstExpression* expr)
{
	*Out = ExpressionValue::VectorValue({ expr->X, expr->Y, expr->Z });
}

void ExpressionEvaluator::Expr(ByteConstExpression* expr)
{
	*Out = ExpressionValue::ByteValue(expr->Value);
}

void ExpressionEvaluator::Expr(IntZeroExpression* expr)
{
	*Out = ExpressionValue::IntValue(0);
}

void ExpressionEvaluator::Expr(IntOneExpression* expr)
{
	*Out = ExpressionValue::IntValue(1);
}

void ExpressionEvaluator::Expr(TrueExpression* expr)
{
	*Out = ExpressionValue::BoolValue(true);
}

void ExpressionEvaluator::Expr(FalseExpression* expr)
{
	*Out = ExpressionValue::BoolValue(false);
}

void ExpressionEvaluator::Expr(NativeParmExpression* expr)
{
	Frame::ThrowException("Native parm expression is not implemented");
}

void ExpressionEvaluator::Expr(NoObjectExpression* expr)
{
	*Out = ExpressionValue::ObjectValue(nullptr);
}

void ExpressionEvaluator::Expr(Unknown0x2bExpression* expr)
{
	PassThrough(expr->Value, Context); // This may have been a truncating instruction from back when strings had a fixed size (package version 61 and earlier)
}

void ExpressionEvaluator::Expr(IntConstByteExpression* expr)
{
	*Out = ExpressionValue::ByteValue(expr->Value);
}

void ExpressionEvaluator::Expr(BoolVariableExpression* expr)
{
	*Out = Value(expr->Variable);
}

void ExpressionEvaluator::Expr(DynamicCastExpression* expr)
{
	// A class import its package lacks reads as null, as a FinalFunction's does
	if (!expr->Class)
		Frame::ThrowException("Script cast to a class import that did not load");
	// The class itself, not one of its name, as the original's
	// execDynamicCast (Core.dll 0x10130ed0)
	UObject* value = Value(expr->Value).ToObject();
	if (value && !value->IsA(expr->Class))
		value = nullptr;
	*Out = ExpressionValue::ObjectValue(value);
}

void ExpressionEvaluator::Expr(IteratorExpression* expr)
{
	Value(expr->Value);
	std::unique_ptr<Iterator> iter = std::move(Frame::CreatedIterator);
	if (IsStatement())
	{
		Result.Result = StatementResult::Iterator;
		Result.Iter = std::move(iter);
		Result.JumpAddress = expr->Offset;
	}
}

void ExpressionEvaluator::Expr(IteratorPopExpression* expr)
{
	if (IsStatement())
		Result.Result = StatementResult::IteratorPop;
}

void ExpressionEvaluator::Expr(IteratorNextExpression* expr)
{
	if (IsStatement())
		Result.Result = StatementResult::IteratorNext;
}

// The first struct read before the second is evaluated, where that may change
// it, as the original's execStructCmpEq and execStructCmpNe (Core.dll
// 0x101303a0, 0x101305a0) step each into a buffer of its own
bool ExpressionEvaluator::StructsEqual(Expression* value1, Expression* value2)
{
	ExpressionValue val1 = Value(value1);
	if (val1.IsVariable() && MayChange(value2))
		val1.Load();
	ExpressionValue val2 = Value(value2);
	return val1.IsEqual(val2);
}

void ExpressionEvaluator::Expr(StructCmpEqExpression* expr)
{
	*Out = ExpressionValue::BoolValue(StructsEqual(expr->Value1, expr->Value2));
}

void ExpressionEvaluator::Expr(StructCmpNeExpression* expr)
{
	*Out = ExpressionValue::BoolValue(!StructsEqual(expr->Value1, expr->Value2));
}

void ExpressionEvaluator::Expr(StructMemberExpression* expr)
{
	if (!expr->Field)
		Frame::ThrowException("Null field encountered in struct member expression");

	// A member of a value (a call's result, a constant) points into that value's storage,
	// which goes when this returns: copy it out
	ExpressionValue base = Value(expr->Value);
	ExpressionValue member = base.Member(expr->Field);
	if (!base.IsVariable())
		member.Load();
	*Out = std::move(member);
}

// Its 16-bit units, each a character, as the original's
void ExpressionEvaluator::Expr(UnicodeStringConstExpression* expr)
{
	ScriptString s;
	s.reserve(expr->Value.size());
	for (wchar_t c : expr->Value)
		s.push_back((char16_t)c);
	*Out = ExpressionValue::StringValue(std::move(s));
}

void ExpressionEvaluator::Expr(RotatorToVectorExpression* expr)
{
	Rotator rot = Value(expr->Value).ToRotator();
	*Out = ExpressionValue::VectorValue(Coords::Rotation(rot).XAxis);
}

void ExpressionEvaluator::Expr(ByteToIntExpression* expr)
{
	*Out = ExpressionValue::IntValue(Value(expr->Value).ToByte());
}

void ExpressionEvaluator::Expr(ByteToBoolExpression* expr)
{
	*Out = ExpressionValue::BoolValue(Value(expr->Value).ToByte() != 0);
}

void ExpressionEvaluator::Expr(ByteToFloatExpression* expr)
{
	*Out = ExpressionValue::FloatValue(Value(expr->Value).ToByte());
}

void ExpressionEvaluator::Expr(IntToByteExpression* expr)
{
	*Out = ExpressionValue::ByteValue(Value(expr->Value).ToInt());
}

void ExpressionEvaluator::Expr(IntToBoolExpression* expr)
{
	*Out = ExpressionValue::BoolValue(Value(expr->Value).ToInt());
}

void ExpressionEvaluator::Expr(IntToFloatExpression* expr)
{
	*Out = ExpressionValue::FloatValue((float)Value(expr->Value).ToInt());
}

void ExpressionEvaluator::Expr(BoolToByteExpression* expr)
{
	*Out = ExpressionValue::ByteValue(Value(expr->Value).ToBool());
}

void ExpressionEvaluator::Expr(BoolToIntExpression* expr)
{
	*Out = ExpressionValue::IntValue(Value(expr->Value).ToBool());
}

void ExpressionEvaluator::Expr(BoolToFloatExpression* expr)
{
	*Out = ExpressionValue::FloatValue(Value(expr->Value).ToBool());
}

void ExpressionEvaluator::Expr(FloatToByteExpression* expr)
{
	*Out = ExpressionValue::ByteValue((uint8_t)ftol(Value(expr->Value).ToFloat()));
}

void ExpressionEvaluator::Expr(FloatToIntExpression* expr)
{
	*Out = ExpressionValue::IntValue(ftol(Value(expr->Value).ToFloat()));
}

void ExpressionEvaluator::Expr(FloatToBoolExpression* expr)
{
	*Out = ExpressionValue::BoolValue((bool)Value(expr->Value).ToFloat());
}

void ExpressionEvaluator::Expr(Unknown0x46Expression* expr)
{
	Frame::ThrowException("Unknown0x46 expression encountered");
}

void ExpressionEvaluator::Expr(ObjectToBoolExpression* expr)
{
	*Out = ExpressionValue::BoolValue(Value(expr->Value).ToObject() != nullptr);
}

void ExpressionEvaluator::Expr(NameToBoolExpression* expr)
{
	*Out = ExpressionValue::BoolValue(Value(expr->Value).ToName().IsNone() == false); // "None" is name index 0: no lookup by spelling
}

// A byte from text is the low 8 bits of the int appAtoi reads (Core.dll
// execStringToByte 0x101321b0): 300 is 44.
void ExpressionEvaluator::Expr(StringToByteExpression* expr)
{
	*Out = ExpressionValue::ByteValue((uint8_t)StrTools::ue1_atoi(Value(expr->Value).ToString()));
}

void ExpressionEvaluator::Expr(StringToIntExpression* expr)
{
	*Out = ExpressionValue::IntValue(StrTools::ue1_atoi(Value(expr->Value).ToString()));
}

void ExpressionEvaluator::Expr(StringToBoolExpression* expr)
{
	// The original's execStringToBool (Core.dll 0x10132490): true for "True",
	// false for "False" in any case (Core.int's words too, the same in the
	// game's), and otherwise true for an int other than 0 by appAtoi, so
	// "0.5", "-0.5" and "0x1" are false.
	std::string v = Value(expr->Value).ToString();
	bool result;
	if (StrTools::equals_ignore_case(v, "True"))
		result = true;
	else if (StrTools::equals_ignore_case(v, "False"))
		result = false;
	else
		result = StrTools::ue1_atoi(v) != 0;
	*Out = ExpressionValue::BoolValue(result);
}

void ExpressionEvaluator::Expr(StringToFloatExpression* expr)
{
	*Out = ExpressionValue::FloatValue((float)std::atof(Value(expr->Value).ToString().c_str()));
}

void ExpressionEvaluator::Expr(StringToVectorExpression* expr)
{
	// The original reads a number at the start, after the first comma and
	// after the second; a part that is missing is 0.
	std::string v = Value(expr->Value).ToString();
	float parts[3] = { 0.0f, 0.0f, 0.0f };
	size_t pos = 0;
	for (int i = 0; i < 3; i++)
	{
		parts[i] = (float)std::atof(v.c_str() + pos);
		pos = v.find(',', pos);
		if (pos == std::string::npos)
			break;
		pos++;
	}
	*Out = ExpressionValue::VectorValue({ parts[0], parts[1], parts[2] });
}

void ExpressionEvaluator::Expr(StringToRotatorExpression* expr)
{
	// As the vector, each part by appAtoi (Core.dll execStringToRotator
	// 0x10136b10): what is there is read, and a missing part is 0.
	std::string v = Value(expr->Value).ToString();
	int parts[3] = { 0, 0, 0 };
	size_t pos = 0;
	for (int i = 0; i < 3; i++)
	{
		parts[i] = StrTools::ue1_atoi(std::string_view(v).substr(pos));
		pos = v.find(',', pos);
		if (pos == std::string::npos)
			break;
		pos++;
	}
	*Out = ExpressionValue::RotatorValue({ parts[0], parts[1], parts[2] });
}

void ExpressionEvaluator::Expr(VectorToBoolExpression* expr)
{
	*Out = ExpressionValue::BoolValue(Value(expr->Value).ToVector() != vec3(0.0f));
}

void ExpressionEvaluator::Expr(VectorToRotatorExpression* expr)
{
	*Out = ExpressionValue::RotatorValue(Rotator::FromVector(Value(expr->Value).ToVector()));
}

void ExpressionEvaluator::Expr(RotatorToBoolExpression* expr)
{
	*Out = ExpressionValue::BoolValue(Value(expr->Value).ToRotator() != Rotator(0, 0, 0));
}

void ExpressionEvaluator::Expr(ByteToStringExpression* expr)
{
	*Out = ExpressionValue::StringValue(std::to_string(Value(expr->Value).ToByte()));
}

void ExpressionEvaluator::Expr(IntToStringExpression* expr)
{
	*Out = ExpressionValue::StringValue(std::to_string(Value(expr->Value).ToInt()));
}

void ExpressionEvaluator::Expr(BoolToStringExpression* expr)
{
	// The original makes it "True" or "False", not 1 or 0.
	*Out = ExpressionValue::StringValue(Value(expr->Value).ToBool() ? "True" : "False");
}

void ExpressionEvaluator::Expr(FloatToStringExpression* expr)
{
	*Out = ExpressionValue::StringValue(StrTools::float_text(Value(expr->Value).ToFloat()));
}

void ExpressionEvaluator::Expr(ObjectToStringExpression* expr)
{
	// The original prints an object's path name: its outers' names and its
	// own, dot-joined, from its package.
	UObject* obj = Value(expr->Value).ToObject();
	if (!obj)
	{
		*Out = ExpressionValue::StringValue("None");
		return;
	}
	*Out = ExpressionValue::StringValue(obj->GetPathName());
}

void ExpressionEvaluator::Expr(NameToStringExpression* expr)
{
	*Out = ExpressionValue::StringValue(Value(expr->Value).ToName().ToString());
}

void ExpressionEvaluator::Expr(VectorToStringExpression* expr)
{
	vec3 v = Value(expr->Value).ToVector();
	*Out = ExpressionValue::StringValue(StrTools::float_text(v.x) + "," + StrTools::float_text(v.y) + "," + StrTools::float_text(v.z));
}

void ExpressionEvaluator::Expr(RotatorToStringExpression* expr)
{
	// The original prints each part wrapped to 0-65535 (RotatorConsole).
	Rotator v = Value(expr->Value).ToRotator();
	*Out = ExpressionValue::StringValue(std::to_string(v.Pitch & 0xffff) + "," + std::to_string(v.Yaw & 0xffff) + "," + std::to_string(v.Roll & 0xffff));
}

void ExpressionEvaluator::Expr(StringToNameExpression* expr)
{
	std::string v = Value(expr->Value).ToString();
	*Out = ExpressionValue::NameValue(v);
}

void ExpressionEvaluator::Expr(VirtualFunctionExpression* expr)
{
	UClass* contextClass = UObject::TryCast<UClass>(Context);
	if (!contextClass)
		contextClass = Context->Class;

	UFunction* func = FindScriptFunction(contextClass, Context->GetStateName(), expr->Name);
	if (func)
		Call(func, expr->Args);
	else
		Frame::ThrowException("Script virtual function " + expr->Name.ToString() + " not found!");
}

void ExpressionEvaluator::Expr(FinalFunctionExpression* expr)
{
	// An import its package lacks reads as null: a script error, not a crash
	if (!expr->Func)
		Frame::ThrowException("Script function call to an import that did not load");
	Call(expr->Func, expr->Args);
}

void ExpressionEvaluator::Expr(GlobalFunctionExpression* expr)
{
	// Global function calls skip the states and only searches normal member functions

	UClass* contextClass = UObject::TryCast<UClass>(Context);
	if (!contextClass)
		contextClass = Context->Class;

	for (UClass* cls = contextClass; cls != nullptr; cls = static_cast<UClass*>(cls->BaseStruct))
	{
		UFunction* func = cls->GetFunction(expr->Name);
		if (func)
		{
			Call(func, expr->Args);
			return;
		}
	}

	Frame::ThrowException("Script global function " + expr->Name.ToString() + " not found!");
}

void ExpressionEvaluator::Expr(NativeFunctionExpression* expr)
{
	if (expr->Typed == TypedKind::Unknown)
		Classify(expr);

	// The general path takes a None context to Frame::Call, which reports it
	if (expr->Typed != TypedKind::Generic && Context)
	{
		*Out = OperatorValue(expr);
		return;
	}
	// A native no loaded class declares: a script error, not a read past the table
	UFunction* func = expr->nativeindex >= 0 && (size_t)expr->nativeindex < NativeFunctions::FuncByIndex.size() ? NativeFunctions::FuncByIndex[expr->nativeindex] : nullptr;
	if (!func)
		Frame::ThrowException("Call to native function " + std::to_string(expr->nativeindex) + ", which no class declares");
	Call(func, expr->Args);
}

// The operators most script calls go to -- about 83% of the native calls on
// Liberty Island, and natives are 84% of all calls -- evaluated here instead
// of through Frame::Call, CallNative and a handler. Each does what its native
// in NObject.cpp does, with the same conversions, its arguments evaluated in
// the caller's context and read as the general path reads them. A function is
// matched by its native index and its name, so a game whose indexes differ
// keeps the general path.
static TypedKind FindFastOperator(UFunction* func)
{
	struct Entry { int index; const char* name; TypedKind op; };
	static const Entry entries[] =
	{
		{ 119, "NotEqual_ObjectObject", TypedKind::NotEqual_ObjectObject },
		{ 114, "EqualEqual_ObjectObject", TypedKind::EqualEqual_ObjectObject },
		{ 129, "Not_PreBool", TypedKind::Not_PreBool },
		{ 150, "Less_IntInt", TypedKind::Less_IntInt },
		{ 152, "LessEqual_IntInt", TypedKind::LessEqual_IntInt },
		{ 151, "Greater_IntInt", TypedKind::Greater_IntInt },
		{ 153, "GreaterEqual_IntInt", TypedKind::GreaterEqual_IntInt },
		{ 154, "EqualEqual_IntInt", TypedKind::EqualEqual_IntInt },
		{ 155, "NotEqual_IntInt", TypedKind::NotEqual_IntInt },
		{ 176, "Less_FloatFloat", TypedKind::Less_FloatFloat },
		{ 178, "LessEqual_FloatFloat", TypedKind::LessEqual_FloatFloat },
		{ 177, "Greater_FloatFloat", TypedKind::Greater_FloatFloat },
		{ 179, "GreaterEqual_FloatFloat", TypedKind::GreaterEqual_FloatFloat },
		{ 181, "NotEqual_FloatFloat", TypedKind::NotEqual_FloatFloat },
		{ 174, "Add_FloatFloat", TypedKind::Add_FloatFloat },
		{ 175, "Subtract_FloatFloat", TypedKind::Subtract_FloatFloat },
		{ 171, "Multiply_FloatFloat", TypedKind::Multiply_FloatFloat },
		{ 172, "Divide_FloatFloat", TypedKind::Divide_FloatFloat },
		{ 147, "Subtract_IntInt", TypedKind::Subtract_IntInt },
		{ 165, "AddAdd_Int", TypedKind::AddAdd_Int },
		{ 161, "AddEqual_IntInt", TypedKind::AddEqual_IntInt },
		{ 184, "AddEqual_FloatFloat", TypedKind::AddEqual_FloatFloat },
		{ 185, "SubtractEqual_FloatFloat", TypedKind::SubtractEqual_FloatFloat },
		{ 254, "EqualEqual_NameName", TypedKind::EqualEqual_NameName },
		{ 255, "NotEqual_NameName", TypedKind::NotEqual_NameName },
	};
	if (AllFlags(func->FuncFlags, FunctionFlags::Native))
	{
		for (const Entry& entry : entries)
		{
			if (entry.index == func->NativeFuncIndex && func->Name == entry.name)
				return entry.op;
		}
	}
	return TypedKind::Generic;
}

bool ExpressionEvaluator::CallFastOperator(UFunction* func, const Array<Expression*>& exprArgs)
{
	if (func->FastOperator < 0)
		func->FastOperator = (int)FindFastOperator(func);
	TypedKind op = (TypedKind)func->FastOperator;
	size_t arity = (op == TypedKind::Not_PreBool || op == TypedKind::AddAdd_Int) ? 1 : 2;
	if (op == TypedKind::Generic || exprArgs.size() != arity || !Context)
		return false;

	UObject* context = Context;
	Context = Self;
	ExpressionValue a = Value(exprArgs[0]);
	// A variable's value before b may change it, as the original's native
	// copies it (P_GET_*); += and -= take the variable itself (P_GET_*_REF)
	bool inPlace = op == TypedKind::AddEqual_IntInt || op == TypedKind::AddEqual_FloatFloat || op == TypedKind::SubtractEqual_FloatFloat;
	if (arity == 2 && !inPlace && a.IsVariable() && MayChange(exprArgs[1]))
		a.Load();
	ExpressionValue b = arity == 2 ? Value(exprArgs[1]) : ExpressionValue();
	Context = context;

	try
	{
		switch (op)
		{
		default: break;
		case TypedKind::NotEqual_ObjectObject: *Out = ExpressionValue::BoolValue(a.ToObject() != b.ToObject()); break;
		case TypedKind::EqualEqual_ObjectObject: *Out = ExpressionValue::BoolValue(a.ToObject() == b.ToObject()); break;
		case TypedKind::Not_PreBool: *Out = ExpressionValue::BoolValue(!a.ToBool()); break;
		case TypedKind::Less_IntInt: *Out = ExpressionValue::BoolValue(a.ToInt() < b.ToInt()); break;
		case TypedKind::LessEqual_IntInt: *Out = ExpressionValue::BoolValue(a.ToInt() <= b.ToInt()); break;
		case TypedKind::Greater_IntInt: *Out = ExpressionValue::BoolValue(a.ToInt() > b.ToInt()); break;
		case TypedKind::GreaterEqual_IntInt: *Out = ExpressionValue::BoolValue(a.ToInt() >= b.ToInt()); break;
		case TypedKind::EqualEqual_IntInt: *Out = ExpressionValue::BoolValue(a.ToInt() == b.ToInt()); break;
		case TypedKind::NotEqual_IntInt: *Out = ExpressionValue::BoolValue(a.ToInt() != b.ToInt()); break;
		case TypedKind::Less_FloatFloat: *Out = ExpressionValue::BoolValue(a.ToFloat() < b.ToFloat()); break;
		case TypedKind::LessEqual_FloatFloat: *Out = ExpressionValue::BoolValue(a.ToFloat() <= b.ToFloat()); break;
		case TypedKind::Greater_FloatFloat: *Out = ExpressionValue::BoolValue(a.ToFloat() > b.ToFloat()); break;
		case TypedKind::GreaterEqual_FloatFloat: *Out = ExpressionValue::BoolValue(a.ToFloat() >= b.ToFloat()); break;
		case TypedKind::NotEqual_FloatFloat: *Out = ExpressionValue::BoolValue(a.ToFloat() != b.ToFloat()); break;
		case TypedKind::Add_FloatFloat: *Out = ExpressionValue::FloatValue(a.ToFloat() + b.ToFloat()); break;
		case TypedKind::Subtract_FloatFloat: *Out = ExpressionValue::FloatValue(a.ToFloat() - b.ToFloat()); break;
		case TypedKind::Multiply_FloatFloat: *Out = ExpressionValue::FloatValue(a.ToFloat() * b.ToFloat()); break;
		case TypedKind::Divide_FloatFloat: *Out = ExpressionValue::FloatValue(a.ToFloat() / b.ToFloat()); break;
		case TypedKind::Subtract_IntInt: *Out = ExpressionValue::IntValue(a.ToInt() - b.ToInt()); break;
		case TypedKind::AddAdd_Int:
			if (Out == LetOut && LetVariable)
				*Out = PostStep(func, a, *LetVariable);
			else
			{
				int32_t& A = a.ToType<int32_t&>();
				*Out = ExpressionValue::IntValue(A++);
			}
			break;
		case TypedKind::AddEqual_IntInt: { int32_t& A = a.ToType<int32_t&>(); int32_t B = b.ToInt(); *Out = ExpressionValue::IntValue(A += B); break; }
		case TypedKind::AddEqual_FloatFloat: { float& A = a.ToType<float&>(); float B = b.ToFloat(); *Out = ExpressionValue::FloatValue(A += B); break; }
		case TypedKind::SubtractEqual_FloatFloat: { float& A = a.ToType<float&>(); float B = b.ToFloat(); *Out = ExpressionValue::FloatValue(A -= B); break; }
		case TypedKind::EqualEqual_NameName: *Out = ExpressionValue::BoolValue(a.ToName() == b.ToName()); break;
		case TypedKind::NotEqual_NameName: *Out = ExpressionValue::BoolValue(a.ToName() != b.ToName()); break;
		}
		return true;
	}
	catch (...)
	{
	}

	// A conversion failed before anything was changed. The general path makes
	// the same conversions in the native's own frame, and reports and
	// recovers exactly as it always has.
	CallArguments args(CallArguments::Room(func, arity));
	args.push_back(std::move(a));
	if (arity == 2)
		args.push_back(std::move(b));
	*Out = Frame::Call(func, Context, args);
	return true;
}

void ExpressionEvaluator::Call(UFunction* func, const Array<Expression*>& exprArgs)
{
	if (CallFastOperator(func, exprArgs))
		return;

	if (func->NativeFuncIndex == 130)
	{
		*Out = ExpressionValue::BoolValue(EvalBool(exprArgs[0], Self) && EvalBool(exprArgs[1], Self));
	}
	else if (func->NativeFuncIndex == 132)
	{
		*Out = ExpressionValue::BoolValue(EvalBool(exprArgs[0], Self) || EvalBool(exprArgs[1], Self));
	}
	else
	{
		// Arguments are evaluated in the caller's own context; the call is made in this one
		CallArguments args(CallArguments::Room(func, exprArgs.size()));
		const Array<UProperty*>& parms = Frame::CallParms(func);
		const bool native = AllFlags(func->FuncFlags, FunctionFlags::Native);
		// The original steps each argument into the callee's frame, or a
		// native into a value of its own (P_GET_*), as it comes to it, a
		// variable's value copied there and then (Core.dll execLocalVariable
		// 0x1012ebc0, CallFunction 0x1013f4e0): `I + Bump(I)`, Bump setting
		// its out I to 10, adds I as it was, 1. A variable before the last
		// argument that may change one (lastChange) is read as it is
		// evaluated; any other is taken in place, as nothing evaluated after
		// it changes it.
		size_t lastChange = 0;
		for (size_t i = exprArgs.size(); i-- > 1;)
		{
			if (MayChange(exprArgs[i]))
			{
				lastChange = i;
				break;
			}
		}
		UObject* context = Context;
		Context = Self;
		for (size_t i = 0; i < exprArgs.size(); i++)
		{
			ExpressionValue value = Value(exprArgs[i]);
			// An argument through a None context (`Other.Location` with Other
			// None) is nothing here, where the original's execContext
			// zero-fills it: the parameter's type's zero, so that a native
			// does not take it for an optional argument left out, which only
			// EX_Nothing (LeafKind::Nothing) is. One that writes nothing -- a
			// call held off or ended without a value, a string through None --
			// leaves a native's optional parameter its default, as left out:
			// the original's natives set it before they step the argument into
			// it (Engine.dll execSpawn 0x103e36d0: the spawner's Location).
			bool keepsDefault = native && value.KeepsVariable && i < parms.size() &&
				AllFlags(parms[i]->PropFlags, PropertyFlags::OptionalParm) && !AllFlags(parms[i]->PropFlags, PropertyFlags::OutParm);
			if (value.GetType() == ExpressionValueType::Nothing && exprArgs[i]->Leaf != Expression::LeafKind::Nothing && i < parms.size() && !keepsDefault)
				value = ExpressionValue::ZeroValue(parms[i]);
			// An out parameter's stays the variable: a native reads and
			// writes it there (P_GET_*_REF, by GPropAddr), and a script
			// function's is copied back to it, the function starting with its
			// value as it is here (CallArguments::SetStart). A static array
			// stays the variable too, the call taking it whole from there
			// (Frame::CallScript; DrawBorders' textures, SendBinary's bytes).
			if (i < lastChange && value.IsVariable() && i < parms.size() && parms[i]->ArrayDimension == 1)
			{
				if (!AllFlags(parms[i]->PropFlags, PropertyFlags::OutParm))
				{
					value.Load();
				}
				else if (!native)
				{
					ExpressionValue start = value;
					start.Load();
					args.SetStart(i, std::move(start));
				}
			}
			args.push_back(std::move(value));
		}
		Context = context;
		// A Let's right side itself writes its value into the variable (LetValue)
		if (Out == LetOut && LetVariable && IsPostStep(func) && args.size() == 1 && args[0].IsVariable())
			*Out = PostStep(func, args[0], *LetVariable);
		else
			*Out = Frame::Call(func, Context, args, Out == LetOut ? LetVariable : nullptr);
	}
}

// The original's post-increment and post-decrement natives (Core.dll
// 0x101347e0, 0x101348a0; the byte ones 0x10133290, 0x10133350) write their
// result, the variable's old value, into the caller's buffer before they step
// the variable: as a Let's right side the buffer is the Let's variable
// (execLet 0x1012fe40), so of `I = I++` the step is what stays.
bool ExpressionEvaluator::IsPostStep(UFunction* func)
{
	int index = func->NativeFuncIndex;
	return index == 139 || index == 140 || index == 165 || index == 166;
}

// The old value into the Let's variable, then the step; the Let then stores
// nothing
ExpressionValue ExpressionEvaluator::PostStep(UFunction* func, ExpressionValue& operand, ExpressionValue& variable)
{
	int index = func->NativeFuncIndex;
	uint32_t step = (index == 139 || index == 165) ? 1u : ~0u;
	if (index == 139 || index == 140)
	{
		uint8_t old = operand.ToByte();
		variable.Store(ExpressionValue::ByteValue(old));
		operand.Store(ExpressionValue::ByteValue((uint8_t)(old + step)));
	}
	else
	{
		int32_t old = operand.ToInt();
		variable.Store(ExpressionValue::IntValue(old));
		operand.Store(ExpressionValue::IntValue((int32_t)((uint32_t)old + step)));
	}
	return ExpressionValue::NothingValue(true);
}

void ExpressionEvaluator::Expr(FunctionArgumentsExpression* expr)
{
	*Out = ExpressionValue::NothingValue();
}

/////////////////////////////////////////////////////////////////////////////
// Typed evaluation
//
// Nearly half of all statements are conditions (JumpIfNot), and most of what
// they and the fast operators evaluate are variables, constants, conversions
// and other operators. Each such node's value went into an 88-byte
// ExpressionValue -- constructed, moved up by assignment through a type
// switch, converted, destructed -- to give a bool, a number or an object.
// The typed evaluators read and compute those directly. What each gives is
// exactly what Value(expr).ToBool() (ToByte, ToInt, ToFloat, ToObject,
// ToName) gives, with the same C++ conversions; a Generic node, one no typed
// kind covers, goes through exactly that.
//
// Operators get typed kinds only when that changes nothing: every operand's
// type is one the operator's conversions accept, so nothing can throw where
// the general path would have caught it. Each operand is read as it is
// evaluated, a variable before the operand after it runs (which might change
// it), as the original's natives copy each (Call).

// The type of value a typed kind yields; Nothing for Generic
static ExpressionValueType TypedValueType(TypedKind kind)
{
	switch (kind)
	{
	default:
		return ExpressionValueType::Nothing;
	case TypedKind::LocalByte: case TypedKind::InstanceByte: case TypedKind::IntConstByte: case TypedKind::ByteConst:
	case TypedKind::IntToByte: case TypedKind::BoolToByte: case TypedKind::FloatToByte:
		return ExpressionValueType::ValueByte;
	case TypedKind::LocalInt: case TypedKind::InstanceInt: case TypedKind::IntConst: case TypedKind::IntZero: case TypedKind::IntOne:
	case TypedKind::ByteToInt: case TypedKind::BoolToInt: case TypedKind::FloatToInt:
	case TypedKind::Subtract_IntInt: case TypedKind::AddAdd_Int: case TypedKind::AddEqual_IntInt:
		return ExpressionValueType::ValueInt;
	case TypedKind::LocalFloat: case TypedKind::InstanceFloat: case TypedKind::FloatConst:
	case TypedKind::ByteToFloat: case TypedKind::IntToFloat: case TypedKind::BoolToFloat:
	case TypedKind::Add_FloatFloat: case TypedKind::Subtract_FloatFloat: case TypedKind::Multiply_FloatFloat: case TypedKind::Divide_FloatFloat:
	case TypedKind::AddEqual_FloatFloat: case TypedKind::SubtractEqual_FloatFloat:
		return ExpressionValueType::ValueFloat;
	case TypedKind::LocalObject: case TypedKind::InstanceObject: case TypedKind::Self: case TypedKind::NoObject: case TypedKind::ObjectConst:
		return ExpressionValueType::ValueObject;
	case TypedKind::LocalName: case TypedKind::InstanceName: case TypedKind::NameConst:
		return ExpressionValueType::ValueName;
	case TypedKind::LocalBool: case TypedKind::InstanceBool: case TypedKind::LocalBoolVariable: case TypedKind::InstanceBoolVariable:
	case TypedKind::True: case TypedKind::False:
	case TypedKind::ByteToBool: case TypedKind::IntToBool: case TypedKind::FloatToBool: case TypedKind::ObjectToBool: case TypedKind::NameToBool:
	case TypedKind::AndAnd: case TypedKind::OrOr: case TypedKind::Not_PreBool:
	case TypedKind::NotEqual_ObjectObject: case TypedKind::EqualEqual_ObjectObject:
	case TypedKind::Less_IntInt: case TypedKind::LessEqual_IntInt: case TypedKind::Greater_IntInt: case TypedKind::GreaterEqual_IntInt:
	case TypedKind::EqualEqual_IntInt: case TypedKind::NotEqual_IntInt:
	case TypedKind::Less_FloatFloat: case TypedKind::LessEqual_FloatFloat: case TypedKind::Greater_FloatFloat: case TypedKind::GreaterEqual_FloatFloat:
	case TypedKind::NotEqual_FloatFloat:
	case TypedKind::EqualEqual_NameName: case TypedKind::NotEqual_NameName:
		return ExpressionValueType::ValueBool;
	}
}

static bool IsNumber(ExpressionValueType type)
{
	return type == ExpressionValueType::ValueByte || type == ExpressionValueType::ValueInt || type == ExpressionValueType::ValueFloat;
}

// A local or instance variable's kind: first is LocalByte or InstanceByte
static TypedKind VariableKind(UProperty* prop, TypedKind first)
{
	int index;
	switch (prop->ValueType)
	{
	default: return TypedKind::Generic;
	case ExpressionValueType::ValueByte: index = 0; break;
	case ExpressionValueType::ValueInt: index = 1; break;
	case ExpressionValueType::ValueBool: index = 2; break;
	case ExpressionValueType::ValueFloat: index = 3; break;
	case ExpressionValueType::ValueObject: index = 4; break;
	case ExpressionValueType::ValueName: index = 5; break;
	}
	return (TypedKind)((int)first + index);
}

TypedKind ExpressionEvaluator::OperatorKind(NativeFunctionExpression* expr)
{
	UFunction* func = (size_t)expr->nativeindex < NativeFunctions::FuncByIndex.size() ? NativeFunctions::FuncByIndex[expr->nativeindex] : nullptr;
	if (!func)
		return TypedKind::Generic;

	const Array<Expression*>& args = expr->Args;
	if (func->FastOperator < 0)
		func->FastOperator = (int)FindFastOperator(func);
	TypedKind op = (TypedKind)func->FastOperator;
	if (op == TypedKind::Generic)
	{
		// As ExpressionEvaluator::Call recognises them
		if ((func->NativeFuncIndex != 130 && func->NativeFuncIndex != 132) || args.size() != 2)
			return TypedKind::Generic;
		expr->Operands[0] = args[0];
		expr->Operands[1] = args[1];
		return func->NativeFuncIndex == 130 ? TypedKind::AndAnd : TypedKind::OrOr;
	}

	size_t arity = (op == TypedKind::Not_PreBool || op == TypedKind::AddAdd_Int) ? 1 : 2;
	if (args.size() != arity)
		return TypedKind::Generic;
	for (Expression* arg : args)
	{
		if (arg->Typed == TypedKind::Unknown)
			Classify(arg);
	}

	TypedKind a = args[0]->Typed;
	ExpressionValueType typeA = TypedValueType(a);
	ExpressionValueType typeB = arity == 2 ? TypedValueType(args[1]->Typed) : ExpressionValueType::Nothing;
	bool typed;
	switch (op)
	{
	case TypedKind::NotEqual_ObjectObject:
	case TypedKind::EqualEqual_ObjectObject:
		typed = typeA == ExpressionValueType::ValueObject && typeB == ExpressionValueType::ValueObject;
		break;
	case TypedKind::EqualEqual_NameName:
	case TypedKind::NotEqual_NameName:
		typed = typeA == ExpressionValueType::ValueName && typeB == ExpressionValueType::ValueName;
		break;
	case TypedKind::Not_PreBool:
		typed = typeA == ExpressionValueType::ValueBool;
		break;
	// ++, += and -= change a variable of exactly their type (ToType<int32_t&>, ToType<float&>)
	case TypedKind::AddAdd_Int:
		typed = a == TypedKind::LocalInt || a == TypedKind::InstanceInt;
		break;
	case TypedKind::AddEqual_IntInt:
		typed = (a == TypedKind::LocalInt || a == TypedKind::InstanceInt) && IsNumber(typeB);
		break;
	case TypedKind::AddEqual_FloatFloat:
	case TypedKind::SubtractEqual_FloatFloat:
		typed = (a == TypedKind::LocalFloat || a == TypedKind::InstanceFloat) && IsNumber(typeB);
		break;
	default:
		typed = IsNumber(typeA) && IsNumber(typeB);
		break;
	}
	if (!typed)
		return TypedKind::Generic;
	expr->Operands[0] = args[0];
	expr->Operands[1] = arity == 2 ? args[1] : nullptr;
	return op;
}

void ExpressionEvaluator::Classify(Expression* expr)
{
	TypedKind kind = TypedKind::Generic;
	switch (expr->Leaf)
	{
	case Expression::LeafKind::LocalVariable:
		{
			auto variable = static_cast<LocalVariableExpression*>(expr);
			kind = VariableKind(variable->Variable, TypedKind::LocalByte);
			variable->Offset = (uint32_t)variable->Variable->DataOffset.DataOffset;
			variable->Mask = variable->Variable->DataOffset.BitfieldMask;
			break;
		}
	case Expression::LeafKind::InstanceVariable:
		{
			auto variable = static_cast<InstanceVariableExpression*>(expr);
			kind = VariableKind(variable->Variable, TypedKind::InstanceByte);
			variable->Offset = (uint32_t)variable->Variable->DataOffset.DataOffset;
			variable->Mask = variable->Variable->DataOffset.BitfieldMask;
			break;
		}
	case Expression::LeafKind::BoolVariable:
		{
			auto boolVariable = static_cast<BoolVariableExpression*>(expr);
			Expression* variable = boolVariable->Variable;
			if (variable->Typed == TypedKind::Unknown)
				Classify(variable);
			if (variable->Typed == TypedKind::LocalBool)
			{
				kind = TypedKind::LocalBoolVariable;
				boolVariable->Offset = static_cast<LocalVariableExpression*>(variable)->Offset;
				boolVariable->Mask = static_cast<LocalVariableExpression*>(variable)->Mask;
			}
			else if (variable->Typed == TypedKind::InstanceBool)
			{
				kind = TypedKind::InstanceBoolVariable;
				boolVariable->Offset = static_cast<InstanceVariableExpression*>(variable)->Offset;
				boolVariable->Mask = static_cast<InstanceVariableExpression*>(variable)->Mask;
			}
			break;
		}
	case Expression::LeafKind::Self: kind = TypedKind::Self; break;
	case Expression::LeafKind::NoObject: kind = TypedKind::NoObject; break;
	case Expression::LeafKind::ObjectConst: kind = TypedKind::ObjectConst; break;
	case Expression::LeafKind::NameConst: kind = TypedKind::NameConst; break;
	case Expression::LeafKind::IntConst: kind = TypedKind::IntConst; break;
	case Expression::LeafKind::IntZero: kind = TypedKind::IntZero; break;
	case Expression::LeafKind::IntOne: kind = TypedKind::IntOne; break;
	case Expression::LeafKind::IntConstByte: kind = TypedKind::IntConstByte; break;
	case Expression::LeafKind::ByteConst: kind = TypedKind::ByteConst; break;
	case Expression::LeafKind::FloatConst: kind = TypedKind::FloatConst; break;
	case Expression::LeafKind::True: kind = TypedKind::True; break;
	case Expression::LeafKind::False: kind = TypedKind::False; break;
	case Expression::LeafKind::Nothing: break;
	case Expression::LeafKind::Other:
		if (dynamic_cast<SkipExpression*>(expr)) kind = TypedKind::Skip;
		else if (dynamic_cast<ByteToIntExpression*>(expr)) kind = TypedKind::ByteToInt;
		else if (dynamic_cast<ByteToBoolExpression*>(expr)) kind = TypedKind::ByteToBool;
		else if (dynamic_cast<ByteToFloatExpression*>(expr)) kind = TypedKind::ByteToFloat;
		else if (dynamic_cast<IntToByteExpression*>(expr)) kind = TypedKind::IntToByte;
		else if (dynamic_cast<IntToBoolExpression*>(expr)) kind = TypedKind::IntToBool;
		else if (dynamic_cast<IntToFloatExpression*>(expr)) kind = TypedKind::IntToFloat;
		else if (dynamic_cast<BoolToByteExpression*>(expr)) kind = TypedKind::BoolToByte;
		else if (dynamic_cast<BoolToIntExpression*>(expr)) kind = TypedKind::BoolToInt;
		else if (dynamic_cast<BoolToFloatExpression*>(expr)) kind = TypedKind::BoolToFloat;
		else if (dynamic_cast<FloatToByteExpression*>(expr)) kind = TypedKind::FloatToByte;
		else if (dynamic_cast<FloatToIntExpression*>(expr)) kind = TypedKind::FloatToInt;
		else if (dynamic_cast<FloatToBoolExpression*>(expr)) kind = TypedKind::FloatToBool;
		else if (dynamic_cast<ObjectToBoolExpression*>(expr)) kind = TypedKind::ObjectToBool;
		else if (dynamic_cast<NameToBoolExpression*>(expr)) kind = TypedKind::NameToBool;
		else if (auto native = dynamic_cast<NativeFunctionExpression*>(expr)) kind = OperatorKind(native);
		else if (dynamic_cast<ContextExpression*>(expr) || dynamic_cast<ClassContextExpression*>(expr) || dynamic_cast<ArrayElementExpression*>(expr)) kind = TypedKind::Context;
		break;
	}
	expr->Typed = kind;
}

// The operand of a conversion or a cast, which changes nothing itself
template<typename E, typename... Rest> static Expression* UnaryOperand(Expression* expr)
{
	if (auto e = dynamic_cast<E*>(expr))
		return e->Value;
	if constexpr (sizeof...(Rest) > 0)
		return UnaryOperand<Rest...>(expr);
	else
		return nullptr;
}

// Object's operators and its math, string and log functions, its static
// natives numbered 112 to 320 in Core.u, change no variable but an out
// parameter. Any other function may: a script function, or a native that runs
// one (Spawn's events, GotoState's, CriticalDelete's Destroy).
static bool ChangesNothing(UFunction* func)
{
	if (!func || !AllFlags(func->FuncFlags, FunctionFlags::Native | FunctionFlags::Static) || func->NativeFuncIndex < 112 || func->NativeFuncIndex > 320)
		return false;
	for (UProperty* parm : Frame::CallParms(func))
	{
		if (AllFlags(parm->PropFlags, PropertyFlags::OutParm))
			return false;
	}
	return true;
}

// A variable, a constant, an argument left out, and what is made of them
// alone -- a context's, an element's or a member's variable, a conversion, a
// cast, a struct ==, a call to one of Object's operators or math, string and
// log functions with no out parameter -- change nothing. Anything else may:
// any other call, new, a dynamic array's element (which grows the array).
bool ExpressionEvaluator::FindChange(Expression* expr)
{
	if (expr->Leaf != Expression::LeafKind::Other)
		return false;
	if (dynamic_cast<DefaultVariableExpression*>(expr) || dynamic_cast<StringConstExpression*>(expr) || dynamic_cast<UnicodeStringConstExpression*>(expr) ||
		dynamic_cast<VectorConstExpression*>(expr) || dynamic_cast<RotationConstExpression*>(expr))
		return false;
	if (auto e = dynamic_cast<ContextExpression*>(expr))
		return MayChange(e->ObjectExpr) || MayChange(e->ContextExpr);
	if (auto e = dynamic_cast<ClassContextExpression*>(expr))
		return MayChange(e->ObjectExpr) || MayChange(e->ContextExpr);
	if (auto e = dynamic_cast<ArrayElementExpression*>(expr))
		return MayChange(e->Index) || MayChange(e->Array);
	if (auto e = dynamic_cast<StructMemberExpression*>(expr))
		return MayChange(e->Value);
	if (auto e = dynamic_cast<StructCmpEqExpression*>(expr))
		return MayChange(e->Value1) || MayChange(e->Value2);
	if (auto e = dynamic_cast<StructCmpNeExpression*>(expr))
		return MayChange(e->Value1) || MayChange(e->Value2);

	UFunction* func = nullptr;
	const Array<Expression*>* args = nullptr;
	if (auto e = dynamic_cast<NativeFunctionExpression*>(expr))
	{
		func = e->nativeindex >= 0 && (size_t)e->nativeindex < NativeFunctions::FuncByIndex.size() ? NativeFunctions::FuncByIndex[e->nativeindex] : nullptr;
		args = &e->Args;
	}
	else if (auto e = dynamic_cast<FinalFunctionExpression*>(expr))
	{
		func = e->Func;
		args = &e->Args;
	}
	if (args)
	{
		if (!ChangesNothing(func))
			return true;
		for (Expression* arg : *args)
		{
			if (MayChange(arg))
				return true;
		}
		return false;
	}

	Expression* operand = UnaryOperand<SkipExpression, DynamicCastExpression, MetaCastExpression, RotatorToVectorExpression,
		ByteToIntExpression, ByteToBoolExpression, ByteToFloatExpression, IntToByteExpression, IntToBoolExpression, IntToFloatExpression,
		BoolToByteExpression, BoolToIntExpression, BoolToFloatExpression, FloatToByteExpression, FloatToIntExpression, FloatToBoolExpression,
		ObjectToBoolExpression, NameToBoolExpression, StringToByteExpression, StringToIntExpression, StringToBoolExpression, StringToFloatExpression,
		StringToVectorExpression, StringToRotatorExpression, VectorToBoolExpression, VectorToRotatorExpression, RotatorToBoolExpression,
		ByteToStringExpression, IntToStringExpression, BoolToStringExpression, FloatToStringExpression, ObjectToStringExpression,
		NameToStringExpression, VectorToStringExpression, RotatorToStringExpression, StringToNameExpression>(expr);
	return !operand || MayChange(operand);
}


#if defined(_MSC_VER)
#define VM_NOINLINE __declspec(noinline)
#else
#define VM_NOINLINE __attribute__((noinline))
#endif

template<typename E> uint8_t* ExpressionEvaluator::Local(Expression* expr) const
{
	return static_cast<uint8_t*>(LocalVariables) + static_cast<E*>(expr)->Offset;
}

template<typename E> uint8_t* ExpressionEvaluator::Instance(Expression* expr) const
{
	return static_cast<uint8_t*>(Context->PropertyData.Data) + static_cast<E*>(expr)->Offset;
}

template<typename E> static bool GetBool(const uint8_t* ptr, Expression* expr)
{
	return (*reinterpret_cast<const uint32_t*>(ptr) & static_cast<E*>(expr)->Mask) != 0;
}

template<typename E> static void SetBool(uint8_t* ptr, Expression* expr, bool value)
{
	BitfieldBool b = { reinterpret_cast<uint32_t*>(ptr), static_cast<E*>(expr)->Mask };
	b.Set(value);
}

template<typename E> static Expression* Operand(Expression* expr) { return static_cast<E*>(expr)->Value; }
static Expression* const* OperandsOf(Expression* expr) { return static_cast<NativeFunctionExpression*>(expr)->Operands; }

using LocalVar = LocalVariableExpression;
using InstanceVar = InstanceVariableExpression;
using BoolVar = BoolVariableExpression;

VM_NOINLINE bool ExpressionEvaluator::GenericBool(Expression* expr) { return Value(expr).ToBool(); }
VM_NOINLINE UObject* ExpressionEvaluator::GenericObject(Expression* expr) { return Value(expr).ToObject(); }
VM_NOINLINE NameString ExpressionEvaluator::GenericName(Expression* expr) { return Value(expr).ToName(); }
template<> VM_NOINLINE uint8_t ExpressionEvaluator::GenericNumber(Expression* expr) { return Value(expr).ToByte(); }
template<> VM_NOINLINE int32_t ExpressionEvaluator::GenericNumber(Expression* expr) { return Value(expr).ToInt(); }
template<> VM_NOINLINE float ExpressionEvaluator::GenericNumber(Expression* expr) { return Value(expr).ToFloat(); }

bool ExpressionEvaluator::EvalBool(Expression* expr, UObject* context)
{
	UObject* oldContext = Context;
	Context = context;
	bool value = EvalBool(expr);
	Context = oldContext;
	return value;
}

bool ExpressionEvaluator::EvalBool(Expression* expr)
{
	switch (expr->Typed)
	{
	default: break;
	case TypedKind::Unknown: Classify(expr); return EvalBool(expr);
	case TypedKind::LocalBool: return GetBool<LocalVar>(Local<LocalVar>(expr), expr);
	case TypedKind::InstanceBool: return GetBool<InstanceVar>(Instance<InstanceVar>(expr), expr);
	case TypedKind::LocalBoolVariable: return GetBool<BoolVar>(Local<BoolVar>(expr), expr);
	case TypedKind::InstanceBoolVariable: return GetBool<BoolVar>(Instance<BoolVar>(expr), expr);
	case TypedKind::True: return true;
	case TypedKind::False: return false;
	case TypedKind::Skip: return EvalBool(Operand<SkipExpression>(expr));
	case TypedKind::ByteToBool: return EvalNumber<uint8_t>(Operand<ByteToBoolExpression>(expr)) != 0;
	case TypedKind::IntToBool: return EvalNumber<int32_t>(Operand<IntToBoolExpression>(expr)) != 0;
	case TypedKind::FloatToBool: return (bool)EvalNumber<float>(Operand<FloatToBoolExpression>(expr));
	case TypedKind::ObjectToBool: return EvalObject(Operand<ObjectToBoolExpression>(expr)) != nullptr;
	case TypedKind::NameToBool: return EvalName(Operand<NameToBoolExpression>(expr)).IsNone() == false;
	case TypedKind::AndAnd: return EvalBool(OperandsOf(expr)[0], Self) && EvalBool(OperandsOf(expr)[1], Self);
	case TypedKind::OrOr: return EvalBool(OperandsOf(expr)[0], Self) || EvalBool(OperandsOf(expr)[1], Self);
	case TypedKind::Not_PreBool: if (!Context) break; return !EvalBool(OperandsOf(expr)[0], Self);
	case TypedKind::NotEqual_ObjectObject: if (!Context) break; { UObject* a; UObject* b; ObjectOperands(expr, a, b); return a != b; }
	case TypedKind::EqualEqual_ObjectObject: if (!Context) break; { UObject* a; UObject* b; ObjectOperands(expr, a, b); return a == b; }
	case TypedKind::EqualEqual_NameName: if (!Context) break; { NameString a, b; NameOperands(expr, a, b); return a == b; }
	case TypedKind::NotEqual_NameName: if (!Context) break; { NameString a, b; NameOperands(expr, a, b); return a != b; }
	case TypedKind::Less_IntInt: if (!Context) break; { int32_t a, b; Operands(expr, a, b); return a < b; }
	case TypedKind::LessEqual_IntInt: if (!Context) break; { int32_t a, b; Operands(expr, a, b); return a <= b; }
	case TypedKind::Greater_IntInt: if (!Context) break; { int32_t a, b; Operands(expr, a, b); return a > b; }
	case TypedKind::GreaterEqual_IntInt: if (!Context) break; { int32_t a, b; Operands(expr, a, b); return a >= b; }
	case TypedKind::EqualEqual_IntInt: if (!Context) break; { int32_t a, b; Operands(expr, a, b); return a == b; }
	case TypedKind::NotEqual_IntInt: if (!Context) break; { int32_t a, b; Operands(expr, a, b); return a != b; }
	case TypedKind::Less_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return a < b; }
	case TypedKind::LessEqual_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return a <= b; }
	case TypedKind::Greater_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return a > b; }
	case TypedKind::GreaterEqual_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return a >= b; }
	case TypedKind::NotEqual_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return a != b; }
	}
	return GenericBool(expr);
}

template<typename T> T ExpressionEvaluator::EvalNumber(Expression* expr)
{
	// Each value is made in its own type, as Value makes it, then converted
	// with a C++ cast, as ToByte, ToInt and ToFloat convert
	switch (expr->Typed)
	{
	default: break;
	case TypedKind::Unknown: Classify(expr); return EvalNumber<T>(expr);
	case TypedKind::LocalByte: return (T)*Local<LocalVar>(expr);
	case TypedKind::LocalInt: return (T)*reinterpret_cast<int32_t*>(Local<LocalVar>(expr));
	case TypedKind::LocalFloat: return (T)*reinterpret_cast<float*>(Local<LocalVar>(expr));
	case TypedKind::InstanceByte: return (T)*Instance<InstanceVar>(expr);
	case TypedKind::InstanceInt: return (T)*reinterpret_cast<int32_t*>(Instance<InstanceVar>(expr));
	case TypedKind::InstanceFloat: return (T)*reinterpret_cast<float*>(Instance<InstanceVar>(expr));
	case TypedKind::IntConst: return (T)(int32_t)static_cast<IntConstExpression*>(expr)->Value;
	case TypedKind::IntZero: return (T)(int32_t)0;
	case TypedKind::IntOne: return (T)(int32_t)1;
	case TypedKind::IntConstByte: return (T)static_cast<IntConstByteExpression*>(expr)->Value;
	case TypedKind::ByteConst: return (T)static_cast<ByteConstExpression*>(expr)->Value;
	case TypedKind::FloatConst: return (T)static_cast<FloatConstExpression*>(expr)->Value;
	case TypedKind::Skip: return EvalNumber<T>(Operand<SkipExpression>(expr));
	case TypedKind::ByteToInt: return (T)(int32_t)EvalNumber<uint8_t>(Operand<ByteToIntExpression>(expr));
	case TypedKind::ByteToFloat: return (T)(float)EvalNumber<uint8_t>(Operand<ByteToFloatExpression>(expr));
	case TypedKind::IntToByte: return (T)(uint8_t)EvalNumber<int32_t>(Operand<IntToByteExpression>(expr));
	case TypedKind::IntToFloat: return (T)(float)EvalNumber<int32_t>(Operand<IntToFloatExpression>(expr));
	case TypedKind::BoolToByte: return (T)(uint8_t)EvalBool(Operand<BoolToByteExpression>(expr));
	case TypedKind::BoolToInt: return (T)(int32_t)EvalBool(Operand<BoolToIntExpression>(expr));
	case TypedKind::BoolToFloat: return (T)(float)EvalBool(Operand<BoolToFloatExpression>(expr));
	// ftol, as the generic path's: the original's _ftol wraps past an int's
	// range and gives 0 for NaN, where a C++ cast is undefined there.
	case TypedKind::FloatToByte: return (T)(uint8_t)ftol(EvalNumber<float>(Operand<FloatToByteExpression>(expr)));
	case TypedKind::FloatToInt: return (T)ftol(EvalNumber<float>(Operand<FloatToIntExpression>(expr)));
	case TypedKind::Add_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return (T)(a + b); }
	case TypedKind::Subtract_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return (T)(a - b); }
	case TypedKind::Multiply_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return (T)(a * b); }
	case TypedKind::Divide_FloatFloat: if (!Context) break; { float a, b; Operands(expr, a, b); return (T)(a / b); }
	case TypedKind::Subtract_IntInt: if (!Context) break; { int32_t a, b; Operands(expr, a, b); return (T)(a - b); }
	case TypedKind::AddAdd_Int: if (!Context) break; { int32_t& a = Target<int32_t>(expr); return (T)(a++); }
	case TypedKind::AddEqual_IntInt:
		if (!Context) break;
		{
			UObject* context = Context;
			Context = Self;
			int32_t b = EvalNumber<int32_t>(OperandsOf(expr)[1]);
			Context = context;
			int32_t& a = Target<int32_t>(expr);
			return (T)(a += b);
		}
	case TypedKind::AddEqual_FloatFloat:
	case TypedKind::SubtractEqual_FloatFloat:
		if (!Context) break;
		{
			UObject* context = Context;
			Context = Self;
			float b = EvalNumber<float>(OperandsOf(expr)[1]);
			Context = context;
			float& a = Target<float>(expr);
			return (T)(expr->Typed == TypedKind::AddEqual_FloatFloat ? (a += b) : (a -= b));
		}
	}
	return GenericNumber<T>(expr);
}

UObject* ExpressionEvaluator::EvalObject(Expression* expr)
{
	switch (expr->Typed)
	{
	default: break;
	case TypedKind::Unknown: Classify(expr); return EvalObject(expr);
	case TypedKind::LocalObject: return *reinterpret_cast<UObject**>(Local<LocalVar>(expr));
	case TypedKind::InstanceObject: return *reinterpret_cast<UObject**>(Instance<InstanceVar>(expr));
	case TypedKind::Self: return Self;
	case TypedKind::NoObject: return nullptr;
	case TypedKind::ObjectConst: return static_cast<ObjectConstExpression*>(expr)->Object;
	case TypedKind::Skip: return EvalObject(Operand<SkipExpression>(expr));
	}
	return GenericObject(expr);
}

NameString ExpressionEvaluator::EvalName(Expression* expr)
{
	switch (expr->Typed)
	{
	default: break;
	case TypedKind::Unknown: Classify(expr); return EvalName(expr);
	case TypedKind::LocalName: return *reinterpret_cast<NameString*>(Local<LocalVar>(expr));
	case TypedKind::InstanceName: return *reinterpret_cast<NameString*>(Instance<InstanceVar>(expr));
	case TypedKind::NameConst: return static_cast<NameConstExpression*>(expr)->Value;
	case TypedKind::Skip: return EvalName(Operand<SkipExpression>(expr));
	}
	return GenericName(expr);
}

template<typename T> void ExpressionEvaluator::Operands(Expression* expr, T& a, T& b)
{
	Expression* const* args = OperandsOf(expr);
	UObject* context = Context;
	Context = Self;
	a = EvalNumber<T>(args[0]);
	b = EvalNumber<T>(args[1]);
	Context = context;
}

void ExpressionEvaluator::ObjectOperands(Expression* expr, UObject*& a, UObject*& b)
{
	Expression* const* args = OperandsOf(expr);
	UObject* context = Context;
	Context = Self;
	a = EvalObject(args[0]);
	b = EvalObject(args[1]);
	Context = context;
}

void ExpressionEvaluator::NameOperands(Expression* expr, NameString& a, NameString& b)
{
	Expression* const* args = OperandsOf(expr);
	UObject* context = Context;
	Context = Self;
	a = EvalName(args[0]);
	b = EvalName(args[1]);
	Context = context;
}

template<typename T> T& ExpressionEvaluator::Target(Expression* expr)
{
	// Classify only lets a Local or Instance variable of exactly T through
	Expression* variable = OperandsOf(expr)[0];
	UObject* context = Context;
	Context = Self;
	uint8_t* ptr = variable->Typed < TypedKind::InstanceByte ? Local<LocalVar>(variable) : Instance<InstanceVar>(variable);
	Context = context;
	return *reinterpret_cast<T*>(ptr);
}

ExpressionValue ExpressionEvaluator::OperatorValue(Expression* expr)
{
	switch (TypedValueType(expr->Typed))
	{
	case ExpressionValueType::ValueBool: return ExpressionValue::BoolValue(EvalBool(expr));
	case ExpressionValueType::ValueInt: return ExpressionValue::IntValue(EvalNumber<int32_t>(expr));
	default: return ExpressionValue::FloatValue(EvalNumber<float>(expr));
	}
}

bool ExpressionEvaluator::Assign(Expression* lhs, Expression* rhs)
{
	if (lhs->Typed == TypedKind::Unknown)
		Classify(lhs);
	if (!IsVariable(lhs->Typed))
		return false;
	if (rhs->Typed == TypedKind::Unknown)
		Classify(rhs);

	// A context's value (`X = A.B`, `X = A.B[i]`) is taken whole: through
	// None it may leave the variable as it was, which only the value tells;
	// so is any other's but a variable's, a constant's or an operator's here,
	// a call's among them: one held off, or ended without a value, leaves it
	// too, and a script function's writes it itself (LetValue). Not a bool:
	// LetBool clears it, and sets it once the call has copied its out
	// parameters back (Expr(LetBoolExpression*)).
	if (rhs->Typed == TypedKind::Context || rhs->Typed == TypedKind::Generic)
	{
		ExpressionValue variable = IsBoolVariable(lhs->Typed) ? ExpressionValue() : Value(lhs);
		ExpressionValue value = LetValue(rhs, variable);
		if (!value.KeepsVariable || IsBoolVariable(lhs->Typed))
			StoreTyped(lhs, value);
		return true;
	}

	// The right side first, then the store: the variable's place is the same
	// either side of it, as the context is. But for the post-increment, which
	// writes the old value into the variable before it steps its own (IsPostStep)
	if (rhs->Typed == TypedKind::AddAdd_Int && Context)
	{
		int32_t& a = Target<int32_t>(rhs);
		int32_t old = a;
		StoreTyped(lhs, ExpressionValue::IntValue(old));
		a = (int32_t)((uint32_t)old + 1u);
		return true;
	}
	switch (lhs->Typed)
	{
	default: return false;
	case TypedKind::LocalByte: { uint8_t v = EvalNumber<uint8_t>(rhs); *Local<LocalVar>(lhs) = v; return true; }
	case TypedKind::LocalInt: { int32_t v = EvalNumber<int32_t>(rhs); *reinterpret_cast<int32_t*>(Local<LocalVar>(lhs)) = v; return true; }
	case TypedKind::LocalFloat: { float v = EvalNumber<float>(rhs); *reinterpret_cast<float*>(Local<LocalVar>(lhs)) = v; return true; }
	case TypedKind::LocalObject: { UObject* v = EvalObject(rhs); *reinterpret_cast<UObject**>(Local<LocalVar>(lhs)) = v; return true; }
	case TypedKind::LocalName: { NameString v = EvalName(rhs); *reinterpret_cast<NameString*>(Local<LocalVar>(lhs)) = v; return true; }
	case TypedKind::LocalBool: { bool v = EvalBool(rhs); SetBool<LocalVar>(Local<LocalVar>(lhs), lhs, v); return true; }
	case TypedKind::InstanceByte: { uint8_t v = EvalNumber<uint8_t>(rhs); *Instance<InstanceVar>(lhs) = v; return true; }
	case TypedKind::InstanceInt: { int32_t v = EvalNumber<int32_t>(rhs); *reinterpret_cast<int32_t*>(Instance<InstanceVar>(lhs)) = v; return true; }
	case TypedKind::InstanceFloat: { float v = EvalNumber<float>(rhs); *reinterpret_cast<float*>(Instance<InstanceVar>(lhs)) = v; return true; }
	case TypedKind::InstanceObject: { UObject* v = EvalObject(rhs); *reinterpret_cast<UObject**>(Instance<InstanceVar>(lhs)) = v; return true; }
	case TypedKind::InstanceName: { NameString v = EvalName(rhs); *reinterpret_cast<NameString*>(Instance<InstanceVar>(lhs)) = v; return true; }
	case TypedKind::InstanceBool: { bool v = EvalBool(rhs); SetBool<InstanceVar>(Instance<InstanceVar>(lhs), lhs, v); return true; }
	case TypedKind::LocalBoolVariable: { bool v = EvalBool(rhs); SetBool<BoolVar>(Local<BoolVar>(lhs), lhs, v); return true; }
	case TypedKind::InstanceBoolVariable: { bool v = EvalBool(rhs); SetBool<BoolVar>(Instance<BoolVar>(lhs), lhs, v); return true; }
	}
}

// Assign's store of a value already made, by the variable kind's type, with
// Store's conversions
void ExpressionEvaluator::StoreTyped(Expression* lhs, const ExpressionValue& value)
{
	switch (lhs->Typed)
	{
	default: break;
	case TypedKind::LocalByte: *Local<LocalVar>(lhs) = value.ToByte(); break;
	case TypedKind::LocalInt: *reinterpret_cast<int32_t*>(Local<LocalVar>(lhs)) = value.ToInt(); break;
	case TypedKind::LocalFloat: *reinterpret_cast<float*>(Local<LocalVar>(lhs)) = value.ToFloat(); break;
	case TypedKind::LocalObject: *reinterpret_cast<UObject**>(Local<LocalVar>(lhs)) = value.ToObject(); break;
	case TypedKind::LocalName: *reinterpret_cast<NameString*>(Local<LocalVar>(lhs)) = value.ToName(); break;
	case TypedKind::LocalBool: SetBool<LocalVar>(Local<LocalVar>(lhs), lhs, value.ToBool()); break;
	case TypedKind::InstanceByte: *Instance<InstanceVar>(lhs) = value.ToByte(); break;
	case TypedKind::InstanceInt: *reinterpret_cast<int32_t*>(Instance<InstanceVar>(lhs)) = value.ToInt(); break;
	case TypedKind::InstanceFloat: *reinterpret_cast<float*>(Instance<InstanceVar>(lhs)) = value.ToFloat(); break;
	case TypedKind::InstanceObject: *reinterpret_cast<UObject**>(Instance<InstanceVar>(lhs)) = value.ToObject(); break;
	case TypedKind::InstanceName: *reinterpret_cast<NameString*>(Instance<InstanceVar>(lhs)) = value.ToName(); break;
	case TypedKind::InstanceBool: SetBool<InstanceVar>(Instance<InstanceVar>(lhs), lhs, value.ToBool()); break;
	case TypedKind::LocalBoolVariable: SetBool<BoolVar>(Local<BoolVar>(lhs), lhs, value.ToBool()); break;
	case TypedKind::InstanceBoolVariable: SetBool<BoolVar>(Instance<BoolVar>(lhs), lhs, value.ToBool()); break;
	}
}
