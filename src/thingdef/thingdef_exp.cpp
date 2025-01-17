/*
** thingdef_exp.cpp
**
** Expression parsing for DECORATE
**
**---------------------------------------------------------------------------
** Copyright 2005 Jan Cholasta
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

#include "actor.h"
#include "sc_man.h"
#include "tarray.h"
#include "templates.h"
#include "cmdlib.h"
#include "i_system.h"
#include "m_random.h"
#include "a_pickups.h"
#include "thingdef.h"
#include "p_lnspec.h"
#include "doomstat.h"
#include "thingdef_exp.h"

FRandom pr_exrandom ("EX_Random");

static FxExpression *ParseRandom(FScanner &sc, FName identifier, PClassActor *cls);
static FxExpression *ParseRandomPick(FScanner &sc, FName identifier, PClassActor *cls);
static FxExpression *ParseRandom2(FScanner &sc, PClassActor *cls);
static FxExpression *ParseAbs(FScanner &sc, PClassActor *cls);
static FxExpression *ParseMinMax(FScanner &sc, FName identifier, PClassActor *cls);
static FxExpression *ParseClamp(FScanner &sc, PClassActor *cls);

//
// ParseExpression
// [GRB] Parses an expression and stores it into Expression array
//

static FxExpression *ParseExpressionM (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionL (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionK (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionJ (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionI (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionH (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionG (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionF (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionE (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionD (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionC (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionB (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpressionA (FScanner &sc, PClassActor *cls);
static FxExpression *ParseExpression0 (FScanner &sc, PClassActor *cls);

FxExpression *ParseExpression (FScanner &sc, PClassActor *cls, bool mustresolve)
{
	FxExpression *data = ParseExpressionM (sc, cls);

	if (mustresolve)
	{
		FCompileContext ctx(cls);
		data = data->Resolve(ctx);
	}

	return data;
}

static FxExpression *ParseExpressionM (FScanner &sc, PClassActor *cls)
{
	FxExpression *condition = ParseExpressionL (sc, cls);

	if (sc.CheckToken('?'))
	{
		FxExpression *truex = ParseExpressionM (sc, cls);
		sc.MustGetToken(':');
		FxExpression *falsex = ParseExpressionM (sc, cls);
		return new FxConditional(condition, truex, falsex);
	}
	else
	{
		return condition;
	}
}

static FxExpression *ParseExpressionL (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionK (sc, cls);

	while (sc.CheckToken(TK_OrOr))
	{
		FxExpression *right = ParseExpressionK (sc, cls);
		tmp = new FxBinaryLogical(TK_OrOr, tmp, right);
	}
	return tmp;
}

static FxExpression *ParseExpressionK (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionJ (sc, cls);

	while (sc.CheckToken(TK_AndAnd))
	{
		FxExpression *right = ParseExpressionJ (sc, cls);
		tmp = new FxBinaryLogical(TK_AndAnd, tmp, right);
	}
	return tmp;
}

static FxExpression *ParseExpressionJ (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionI (sc, cls);

	while (sc.CheckToken('|'))
	{
		FxExpression *right = ParseExpressionI (sc, cls);
		tmp = new FxBinaryInt('|', tmp, right);
	}
	return tmp;
}

static FxExpression *ParseExpressionI (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionH (sc, cls);

	while (sc.CheckToken('^'))
	{
		FxExpression *right = ParseExpressionH (sc, cls);
		tmp = new FxBinaryInt('^', tmp, right);
	}
	return tmp;
}

static FxExpression *ParseExpressionH (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionG (sc, cls);

	while (sc.CheckToken('&'))
	{
		FxExpression *right = ParseExpressionG (sc, cls);
		tmp = new FxBinaryInt('&', tmp, right);
	}
	return tmp;
}

static FxExpression *ParseExpressionG (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionF (sc, cls);

	while (sc.GetToken() && (sc.TokenType == TK_Eq || sc.TokenType == TK_Neq))
	{
		int token = sc.TokenType;
		FxExpression *right = ParseExpressionF (sc, cls);
		tmp = new FxCompareEq(token, tmp, right);
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static FxExpression *ParseExpressionF (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionE (sc, cls);

	while (sc.GetToken() && (sc.TokenType == '<' || sc.TokenType == '>' || sc.TokenType == TK_Leq || sc.TokenType == TK_Geq))
	{
		int token = sc.TokenType;
		FxExpression *right = ParseExpressionE (sc, cls);
		tmp = new FxCompareRel(token, tmp, right);
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static FxExpression *ParseExpressionE (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionD (sc, cls);

	while (sc.GetToken() && (sc.TokenType == TK_LShift || sc.TokenType == TK_RShift || sc.TokenType == TK_URShift))
	{
		int token = sc.TokenType;
		FxExpression *right = ParseExpressionD (sc, cls);
		tmp = new FxBinaryInt(token, tmp, right);
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static FxExpression *ParseExpressionD (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionC (sc, cls);

	while (sc.GetToken() && (sc.TokenType == '+' || sc.TokenType == '-'))
	{
		int token = sc.TokenType;
		FxExpression *right = ParseExpressionC (sc, cls);
		tmp = new FxAddSub(token, tmp, right);
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static FxExpression *ParseExpressionC (FScanner &sc, PClassActor *cls)
{
	FxExpression *tmp = ParseExpressionB (sc, cls);

	while (sc.GetToken() && (sc.TokenType == '*' || sc.TokenType == '/' || sc.TokenType == '%'))
	{
		int token = sc.TokenType;
		FxExpression *right = ParseExpressionB (sc, cls);
		tmp = new FxMulDiv(token, tmp, right);
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static FxExpression *ParseExpressionB (FScanner &sc, PClassActor *cls)
{
	sc.GetToken();
	switch(sc.TokenType)
	{
	case '~':
		return new FxUnaryNotBitwise(ParseExpressionA (sc, cls));

	case '!':
		return new FxUnaryNotBoolean(ParseExpressionA (sc, cls));

	case '-':
		return new FxMinusSign(ParseExpressionA (sc, cls));

	case '+':
		return new FxPlusSign(ParseExpressionA (sc, cls));

	default:
		sc.UnGet();
		return ParseExpressionA (sc, cls);
	}
}

//==========================================================================
//
//	ParseExpressionB
//
//==========================================================================

static FxExpression *ParseExpressionA (FScanner &sc, PClassActor *cls)
{
	FxExpression *base_expr = ParseExpression0 (sc, cls);

	while(1)
	{
		FScriptPosition pos(sc);

#if 0
		if (sc.CheckToken('.'))
		{
			if (sc.CheckToken(TK_Default))
			{
				sc.MustGetToken('.');
				base_expr = new FxClassDefaults(base_expr, pos);
			}
			sc.MustGetToken(TK_Identifier);

			FName FieldName = sc.String;
			pos = sc;
			/* later!
			if (SC_CheckToken('('))
			{
				if (base_expr->IsDefaultObject())
				{
					SC_ScriptError("Cannot call methods for default.");
				}
				base_expr = ParseFunctionCall(base_expr, FieldName, false, false, pos);
			}
			else
			*/
			{
				base_expr = new FxDotIdentifier(base_expr, FieldName, pos);
			}
		}
		else 
#endif
			if (sc.CheckToken('['))
		{
			FxExpression *index = ParseExpressionM(sc, cls);
			sc.MustGetToken(']');
			base_expr = new FxArrayElement(base_expr, index);
		}
		else break;
	} 

	return base_expr;
}



static FxExpression *ParseExpression0 (FScanner &sc, PClassActor *cls)
{
	FScriptPosition scpos(sc);
	if (sc.CheckToken('('))
	{
		FxExpression *data = ParseExpressionM (sc, cls);
		sc.MustGetToken(')');
		return data;
	}
	else if (sc.CheckToken(TK_True))
	{
		return new FxConstant(1, scpos);
	}
	else if (sc.CheckToken(TK_False))
	{
		return new FxConstant(0, scpos);
	}
	else if (sc.CheckToken(TK_IntConst))
	{
		return new FxConstant(sc.Number, scpos);
	}
	else if (sc.CheckToken(TK_FloatConst))
	{
		return new FxConstant(sc.Float, scpos);
	}
	else if (sc.CheckToken(TK_NameConst))
	{
		return new FxConstant(sc.Name, scpos);
	}
	else if (sc.CheckToken(TK_StringConst))
	{
		// String parameters are converted to names. Technically, this should be
		// done at a higher level, as needed, but since no functions take string
		// arguments and ACS_NamedExecuteWithResult/CallACS need names, this is
		// a cheap way to get them working when people use "name" instead of 'name'.
		return new FxConstant(FName(sc.String), scpos);
	}
	else if (sc.CheckToken(TK_Identifier))
	{
		FName identifier = FName(sc.String);
		FArgumentList *args;
		PFunction *func;

		switch (identifier)
		{
		case NAME_Random:
		case NAME_FRandom:
			return ParseRandom(sc, identifier, cls);
		case NAME_RandomPick:
		case NAME_FRandomPick:
			return ParseRandomPick(sc, identifier, cls);
		case NAME_Random2:
			return ParseRandom2(sc, cls);
		default:
			break;
		}
		if (sc.CheckToken('('))
		{
			switch (identifier)
			{
			case NAME_Min:
			case NAME_Max:
				return ParseMinMax(sc, identifier, cls);
			case NAME_Clamp:
				return ParseClamp(sc, cls);
			case NAME_Abs:
				return ParseAbs(sc, cls);
			default:
				args = new FArgumentList;
				func = dyn_cast<PFunction>(cls->Symbols.FindSymbol(identifier, true));
				try
				{
					// There is an action function ACS_NamedExecuteWithResult which must be ignored here for this to work.
					if (func != NULL && identifier != NAME_ACS_NamedExecuteWithResult)
					{
						sc.UnGet();
						ParseFunctionParameters(sc, cls, *args, func, "", NULL);
						return new FxVMFunctionCall(func, args, sc);
					}
					else if (!sc.CheckToken(')'))
					{
						do
						{
							args->Push(ParseExpressionM (sc, cls));
						}
						while (sc.CheckToken(','));
						sc.MustGetToken(')');
					}
					return new FxFunctionCall(NULL, identifier, args, sc);
				}
				catch (...)
				{
					delete args;
					throw;
				}
				break;
			}
		}	
		else
		{
			return new FxIdentifier(identifier, sc);
		}
	}
	else
	{
		FString tokname = sc.TokenName(sc.TokenType, sc.String);
		sc.ScriptError ("Unexpected token %s", tokname.GetChars());
	}
	return NULL;
}

static FRandom *ParseRNG(FScanner &sc)
{
	FRandom *rng;

	if (sc.CheckToken('['))
	{
		sc.MustGetToken(TK_Identifier);
		rng = FRandom::StaticFindRNG(sc.String);
		sc.MustGetToken(']');
	}
	else
	{
		rng = &pr_exrandom;
	}
	return rng;
}

static FxExpression *ParseRandom(FScanner &sc, FName identifier, PClassActor *cls)
{
	FRandom *rng = ParseRNG(sc);

	sc.MustGetToken('(');
	FxExpression *min = ParseExpressionM (sc, cls);
	sc.MustGetToken(',');
	FxExpression *max = ParseExpressionM (sc, cls);
	sc.MustGetToken(')');

	if (identifier == NAME_Random)
	{
		return new FxRandom(rng, min, max, sc);
	}
	else
	{
		return new FxFRandom(rng, min, max, sc);
	}
}

static FxExpression *ParseRandomPick(FScanner &sc, FName identifier, PClassActor *cls)
{
	bool floaty = identifier == NAME_FRandomPick;
	FRandom *rng;
	TArray<FxExpression*> list;
	list.Clear();
	int index = 0;

	rng = ParseRNG(sc);
	sc.MustGetToken('(');

	for (;;)
	{
		FxExpression *expr = ParseExpressionM(sc, cls);
		list.Push(expr);
		if (sc.CheckToken(')'))
			break;
		sc.MustGetToken(',');
	}
	return new FxRandomPick(rng, list, floaty, sc);
}

static FxExpression *ParseRandom2(FScanner &sc, PClassActor *cls)
{
	FRandom *rng = ParseRNG(sc);
	FxExpression *mask = NULL;

	sc.MustGetToken('(');

	if (!sc.CheckToken(')'))
	{
		mask = ParseExpressionM(sc, cls);
		sc.MustGetToken(')');
	}
	return new FxRandom2(rng, mask, sc);
}

static FxExpression *ParseAbs(FScanner &sc, PClassActor *cls)
{
	FxExpression *x = ParseExpressionM (sc, cls);
	sc.MustGetToken(')');
	return new FxAbs(x); 
}

static FxExpression *ParseMinMax(FScanner &sc, FName identifier, PClassActor *cls)
{
	TArray<FxExpression*> list;
	for (;;)
	{
		FxExpression *expr = ParseExpressionM(sc, cls);
		list.Push(expr);
		if (sc.CheckToken(')'))
			break;
		sc.MustGetToken(',');
	}
	return new FxMinMax(list, identifier, sc);
}

static FxExpression *ParseClamp(FScanner &sc, PClassActor *cls)
{
	FxExpression *src = ParseExpressionM(sc, cls);
	sc.MustGetToken(',');
	FxExpression *min = ParseExpressionM(sc, cls);
	sc.MustGetToken(',');
	FxExpression *max = ParseExpressionM(sc, cls);
	sc.MustGetToken(')');

	// Build clamp(a,x,y) as min(max(a,x),y)
	TArray<FxExpression *> list(2);
	list.Reserve(2);
	list[0] = src;
	list[1] = min;
	FxExpression *maxexpr = new FxMinMax(list, NAME_Max, sc);
	list[0] = maxexpr;
	list[1] = max;
	return new FxMinMax(list, NAME_Min, sc);
}
