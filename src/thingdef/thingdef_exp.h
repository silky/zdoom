#ifndef THINGDEF_EXP_H
#define THINGDEF_EXP_H

/*
** thingdef_exp.h
**
** Expression evaluation
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "m_random.h"


#define CHECKRESOLVED() if (isresolved) return this; isresolved=true;
#define SAFE_DELETE(p) if (p!=NULL) { delete p; p=NULL; }
#define RESOLVE(p,c) if (p!=NULL) p = p->Resolve(c)
#define ABORT(p) if (!(p)) { delete this; return NULL; }
#define SAFE_RESOLVE(p,c) RESOLVE(p,c); ABORT(p) 

class VMFunctionBuilder;

//==========================================================================
//
//
//
//==========================================================================

struct FCompileContext
{
	PClassActor *cls;

	FCompileContext(PClassActor *_cls = NULL)
	{
		cls = _cls;
	}

	PSymbol *FindInClass(FName identifier)
	{
		return cls ? cls->Symbols.FindSymbol(identifier, true) : NULL;
	}
	PSymbol *FindGlobal(FName identifier)
	{
		return GlobalSymbols.FindSymbol(identifier, true);
	}
};

//==========================================================================
//
//
//
//==========================================================================

struct ExpVal
{
	PType *Type;
	union
	{
		int Int;
		double Float;
		void *pointer;
	};

	ExpVal()
	{
		Type = TypeSInt32;
		Int = 0;
	}

	~ExpVal()
	{
		if (Type == TypeString)
		{
			((FString *)&pointer)->~FString();
		}
	}

	ExpVal(const FString &str)
	{
		Type = TypeString;
		::new(&pointer) FString(str);
	}

	ExpVal(const ExpVal &o)
	{
		Type = o.Type;
		if (o.Type == TypeString)
		{
			::new(&pointer) FString(*(FString *)&o.pointer);
		}
		else
		{
			memcpy(&Float, &o.Float, 8);
		}
	}

	ExpVal &operator=(const ExpVal &o)
	{
		if (Type == TypeString)
		{
			((FString *)&pointer)->~FString();
		}
		Type = o.Type;
		if (o.Type == TypeString)
		{
			::new(&pointer) FString(*(FString *)&o.pointer);
		}
		else
		{
			memcpy(&Float, &o.Float, 8);
		}
		return *this;
	}

	int GetInt() const
	{
		int regtype = Type->GetRegType();
		return regtype == REGT_INT ? Int : regtype == REGT_FLOAT ? int(Float) : 0;
	}

	double GetFloat() const
	{
		int regtype = Type->GetRegType();
		return regtype == REGT_INT ? double(Int) : regtype == REGT_FLOAT ? Float : 0;
	}

	const FString GetString() const
	{
		return Type == TypeString ? *(FString *)&pointer : Type == TypeName ? FString(FName(ENamedName(Int)).GetChars()) : "";
	}

	bool GetBool() const
	{
		int regtype = Type->GetRegType();
		return regtype == REGT_INT ? !!Int : regtype == REGT_FLOAT ? Float!=0. : false;
	}
	
	FName GetName() const
	{
		return Type == TypeName ? ENamedName(Int) : NAME_None;
	}
};

struct ExpEmit
{
	ExpEmit() : RegNum(0), RegType(REGT_NIL), Konst(false), Fixed(false) {}
	ExpEmit(int reg, int type) : RegNum(reg), RegType(type), Konst(false), Fixed(false) {}
	ExpEmit(int reg, int type, bool konst)  : RegNum(reg), RegType(type), Konst(konst), Fixed(false) {}
	ExpEmit(VMFunctionBuilder *build, int type);
	void Free(VMFunctionBuilder *build);
	void Reuse(VMFunctionBuilder *build);

	BYTE RegNum, RegType, Konst:1, Fixed:1;
};

//==========================================================================
//
//
//
//==========================================================================

class FxExpression
{
protected:
	FxExpression(const FScriptPosition &pos)
	: ScriptPosition(pos)
	{
		isresolved = false;
		ScriptPosition = pos;
		ValueType = NULL;
	}
public:
	virtual ~FxExpression() {}
	virtual FxExpression *Resolve(FCompileContext &ctx);
	FxExpression *ResolveAsBoolean(FCompileContext &ctx);
	
	virtual bool isConstant() const;
	virtual void RequestAddress();
	virtual VMFunction *GetDirectFunction();
	bool IsNumeric() const { return ValueType->GetRegType() == REGT_INT || ValueType->GetRegType() == REGT_FLOAT; }
	bool IsPointer() const { return ValueType->GetRegType() == REGT_POINTER; }

	virtual ExpEmit Emit(VMFunctionBuilder *build);

	FScriptPosition ScriptPosition;
	PType *ValueType;

	bool isresolved;
};

//==========================================================================
//
//	FxIdentifier
//
//==========================================================================

class FxIdentifier : public FxExpression
{
	FName Identifier;

public:
	FxIdentifier(FName i, const FScriptPosition &p);
	FxExpression *Resolve(FCompileContext&);
};


//==========================================================================
//
//	FxDotIdentifier
//
//==========================================================================

class FxDotIdentifier : public FxExpression
{
	FxExpression *container;
	FName Identifier;

public:
	FxDotIdentifier(FxExpression*, FName, const FScriptPosition &);
	~FxDotIdentifier();
	FxExpression *Resolve(FCompileContext&);
};

//==========================================================================
//
//	FxClassDefaults
//
//==========================================================================

class FxClassDefaults : public FxExpression
{
	FxExpression *obj;

public:
	FxClassDefaults(FxExpression*, const FScriptPosition &);
	~FxClassDefaults();
	FxExpression *Resolve(FCompileContext&);
	bool IsDefaultObject() const;
};

//==========================================================================
//
//	FxConstant
//
//==========================================================================

class FxConstant : public FxExpression
{
	ExpVal value;

public:
	FxConstant(int val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = TypeSInt32;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(double val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = TypeFloat64;
		value.Float = val;
		isresolved = true;
	}

	FxConstant(FSoundID val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = TypeSound;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(FName val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = TypeName;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(const FString &str, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = TypeString;
		value = ExpVal(str);
		isresolved = true;
	}

	FxConstant(ExpVal cv, const FScriptPosition &pos) : FxExpression(pos)
	{
		value = cv;
		ValueType = cv.Type;
		isresolved = true;
	}

	FxConstant(PClass *val, const FScriptPosition &pos) : FxExpression(pos)
	{
		value.pointer = (void*)val;
		ValueType = val;
		value.Type = NewClassPointer(RUNTIME_CLASS(AActor));
		isresolved = true;
	}

	FxConstant(FState *state, const FScriptPosition &pos) : FxExpression(pos)
	{
		value.pointer = state;
		ValueType = value.Type = TypeState;
		isresolved = true;
	}
	
	static FxExpression *MakeConstant(PSymbol *sym, const FScriptPosition &pos);

	bool isConstant() const
	{
		return true;
	}

	ExpVal GetValue() const
	{
		return value;
	}
	ExpEmit Emit(VMFunctionBuilder *build);
};


//==========================================================================
//
//
//
//==========================================================================

class FxIntCast : public FxExpression
{
	FxExpression *basex;

public:

	FxIntCast(FxExpression *x);
	~FxIntCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxFloatCast : public FxExpression
{
	FxExpression *basex;

public:

	FxFloatCast(FxExpression *x);
	~FxFloatCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxCastStateToBool : public FxExpression
{
	FxExpression *basex;

public:
	FxCastStateToBool(FxExpression *x);
	~FxCastStateToBool();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxSign
//
//==========================================================================

class FxPlusSign : public FxExpression
{
	FxExpression *Operand;

public:
	FxPlusSign(FxExpression*);
	~FxPlusSign();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxSign
//
//==========================================================================

class FxMinusSign : public FxExpression
{
	FxExpression *Operand;

public:
	FxMinusSign(FxExpression*);
	~FxMinusSign();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxUnaryNot
//
//==========================================================================

class FxUnaryNotBitwise : public FxExpression
{
	FxExpression *Operand;

public:
	FxUnaryNotBitwise(FxExpression*);
	~FxUnaryNotBitwise();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxUnaryNot
//
//==========================================================================

class FxUnaryNotBoolean : public FxExpression
{
	FxExpression *Operand;

public:
	FxUnaryNotBoolean(FxExpression*);
	~FxUnaryNotBoolean();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxBinary : public FxExpression
{
public:
	int				Operator;
	FxExpression		*left;
	FxExpression		*right;

	FxBinary(int, FxExpression*, FxExpression*);
	~FxBinary();
	bool ResolveLR(FCompileContext& ctx, bool castnumeric);
	void Promote(FCompileContext &ctx);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxAddSub : public FxBinary
{
public:

	FxAddSub(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxMulDiv : public FxBinary
{
public:

	FxMulDiv(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxCompareRel : public FxBinary
{
public:

	FxCompareRel(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxCompareEq : public FxBinary
{
public:

	FxCompareEq(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxBinaryInt : public FxBinary
{
public:

	FxBinaryInt(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinaryLogical
//
//==========================================================================

class FxBinaryLogical : public FxExpression
{
public:
	int				Operator;
	FxExpression		*left;
	FxExpression		*right;

	FxBinaryLogical(int, FxExpression*, FxExpression*);
	~FxBinaryLogical();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxConditional
//
//==========================================================================

class FxConditional : public FxExpression
{
public:
	FxExpression *condition;
	FxExpression *truex;
	FxExpression *falsex;

	FxConditional(FxExpression*, FxExpression*, FxExpression*);
	~FxConditional();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxAbs : public FxExpression
{
	FxExpression *val;

public:

	FxAbs(FxExpression *v);
	~FxAbs();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxMinMax : public FxExpression
{
	TDeletingArray<FxExpression *> choices;
	FName Type;

public:
	FxMinMax(TArray<FxExpression*> &expr, FName type, const FScriptPosition &pos);
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandom : public FxExpression
{
protected:
	FRandom *rng;
	FxExpression *min, *max;

public:

	FxRandom(FRandom *, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos);
	~FxRandom();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandomPick : public FxExpression
{
protected:
	FRandom *rng;
	TDeletingArray<FxExpression*> choices;

public:

	FxRandomPick(FRandom *, TArray<FxExpression*> &expr, bool floaty, const FScriptPosition &pos);
	~FxRandomPick();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxFRandom : public FxRandom
{
public:
	FxFRandom(FRandom *, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandom2 : public FxExpression
{
	FRandom * rng;
	FxExpression *mask;

public:

	FxRandom2(FRandom *, FxExpression *m, const FScriptPosition &pos);
	~FxRandom2();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};


//==========================================================================
//
//	FxClassMember
//
//==========================================================================

class FxClassMember : public FxExpression
{
public:
	FxExpression *classx;
	PField *membervar;
	bool AddressRequested;

	FxClassMember(FxExpression*, PField*, const FScriptPosition&);
	~FxClassMember();
	FxExpression *Resolve(FCompileContext&);
	void RequestAddress();
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxSelf
//
//==========================================================================

class FxSelf : public FxExpression
{
public:
	FxSelf(const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxDamage
//
//==========================================================================

class FxDamage : public FxExpression
{
public:
	FxDamage(const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxArrayElement
//
//==========================================================================

class FxArrayElement : public FxExpression
{
public:
	FxExpression *Array;
	FxExpression *index;
	//bool AddressRequested;

	FxArrayElement(FxExpression*, FxExpression*);
	~FxArrayElement();
	FxExpression *Resolve(FCompileContext&);
	//void RequestAddress();
	ExpEmit Emit(VMFunctionBuilder *build);
};


//==========================================================================
//
//	FxFunctionCall
//
//==========================================================================

typedef TDeletingArray<FxExpression*> FArgumentList;

class FxFunctionCall : public FxExpression
{
	FxExpression *Self;
	FName MethodName;
	FArgumentList *ArgList;

public:

	FxFunctionCall(FxExpression *self, FName methodname, FArgumentList *args, const FScriptPosition &pos);
	~FxFunctionCall();
	FxExpression *Resolve(FCompileContext&);
};


//==========================================================================
//
//	FxActionSpecialCall
//
//==========================================================================

class FxActionSpecialCall : public FxExpression
{
	FxExpression *Self;
	int Special;
	FArgumentList *ArgList;

public:

	FxActionSpecialCall(FxExpression *self, int special, FArgumentList *args, const FScriptPosition &pos);
	~FxActionSpecialCall();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxFlopFunctionCall
//
//==========================================================================

class FxFlopFunctionCall : public FxExpression
{
	int Index;
	FArgumentList *ArgList;

public:

	FxFlopFunctionCall(size_t index, FArgumentList *args, const FScriptPosition &pos);
	~FxFlopFunctionCall();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
// FxVMFunctionCall
//
//==========================================================================

class FxVMFunctionCall : public FxExpression
{
	PFunction *Function;
	FArgumentList *ArgList;

public:
	FxVMFunctionCall(PFunction *func, FArgumentList *args, const FScriptPosition &pos);
	~FxVMFunctionCall();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	ExpEmit Emit(VMFunctionBuilder *build, bool tailcall);
	bool CheckEmitCast(VMFunctionBuilder *build, bool returnit, ExpEmit &reg);
	unsigned GetArgCount() const { return ArgList == NULL ? 0 : ArgList->Size(); }
	VMFunction *GetVMFunction() const { return Function->Variants[0].Implementation; }
	bool IsDirectFunction();
};

//==========================================================================
//
// FxSequence
//
//==========================================================================

class FxSequence : public FxExpression
{
	TDeletingArray<FxExpression *> Expressions;

public:
	FxSequence(const FScriptPosition &pos) : FxExpression(pos) {}
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	void Add(FxExpression *expr) { if (expr != NULL) Expressions.Push(expr); }
	VMFunction *GetDirectFunction();
};

//==========================================================================
//
// FxIfStatement
//
//==========================================================================

class FxIfStatement : public FxExpression
{
	FxExpression *Condition;
	FxExpression *WhenTrue;
	FxExpression *WhenFalse;

public:
	FxIfStatement(FxExpression *cond, FxExpression *true_part, FxExpression *false_part, const FScriptPosition &pos);
	~FxIfStatement();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
// FxReturnStatement
//
//==========================================================================

class FxReturnStatement : public FxExpression
{
	FxVMFunctionCall *Call;

public:
	FxReturnStatement(FxVMFunctionCall *call, const FScriptPosition &pos);
	~FxReturnStatement();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	VMFunction *GetDirectFunction();
};

//==========================================================================
//
//
//
//==========================================================================

class FxClassTypeCast : public FxExpression
{
	PClass *desttype;
	FxExpression *basex;

public:

	FxClassTypeCast(PClass *dtype, FxExpression *x);
	~FxClassTypeCast();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
// Only used to resolve the old jump by index feature of DECORATE
//
//==========================================================================

class FxStateByIndex : public FxExpression
{
	int index;

public:

	FxStateByIndex(int i, const FScriptPosition &pos) : FxExpression(pos)
	{
		index = i;
	}
	FxExpression *Resolve(FCompileContext&);
};

//==========================================================================
//
//
//
//==========================================================================

class FxMultiNameState : public FxExpression
{
	PClassActor *scope;
	TArray<FName> names;
public:

	FxMultiNameState(const char *statestring, const FScriptPosition &pos);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxDamageValue : public FxExpression
{
	FxExpression *val;
	bool Calculated;
	VMScriptFunction *MyFunction;

public:

	FxDamageValue(FxExpression *v, bool calc);
	~FxDamageValue();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
	VMScriptFunction *GetFunction() const { return MyFunction; }
	void SetFunction(VMScriptFunction *func) { MyFunction = func; }
};

//==========================================================================
//
//
//
//==========================================================================

class FxNop : public FxExpression
{
public:
	FxNop(const FScriptPosition &p)
		: FxExpression(p)
	{
		isresolved = true;
	}
	ExpEmit Emit(VMFunctionBuilder *build)
	{
		return ExpEmit();
	}
};

FxExpression *ParseExpression (FScanner &sc, PClassActor *cls, bool mustresolve = false);


#endif
