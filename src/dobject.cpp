/*
** dobject.cpp
** Implements the base class DObject, which most other classes derive from
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <stdlib.h>
#include <string.h>

#include "cmdlib.h"
#include "actor.h"
#include "dobject.h"
#include "doomstat.h"		// Ideally, DObjects can be used independant of Doom.
#include "d_player.h"		// See p_user.cpp to find out why this doesn't work.
#include "g_game.h"			// Needed for bodyque.
#include "c_dispatch.h"
#include "i_system.h"
#include "r_state.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "dsectoreffect.h"
#include "farchive.h"

ClassReg DObject::RegistrationInfo =
{
	NULL,							// MyClass
	"DObject",						// Name
	NULL,							// ParentType
	NULL,							// Pointers
	&DObject::InPlaceConstructor,	// ConstructNative
	sizeof(DObject),				// SizeOf
	CLASSREG_PClass,				// MetaClassNum
};
_DECLARE_TI(DObject)

CCMD (dumpactors)
{
	const char *const filters[32] =
	{
		"0:All", "1:Doom", "2:Heretic", "3:DoomHeretic", "4:Hexen", "5:DoomHexen", "6:Raven", "7:IdRaven",
		"8:Strife", "9:DoomStrife", "10:HereticStrife", "11:DoomHereticStrife", "12:HexenStrife", 
		"13:DoomHexenStrife", "14:RavenStrife", "15:NotChex", "16:Chex", "17:DoomChex", "18:HereticChex",
		"19:DoomHereticChex", "20:HexenChex", "21:DoomHexenChex", "22:RavenChex", "23:NotStrife", "24:StrifeChex",
		"25:DoomStrifeChex", "26:HereticStrifeChex", "27:NotHexen",	"28:HexenStrifeChex", "29:NotHeretic",
		"30:NotDoom", "31:All",
	};
	Printf("%i object class types total\nActor\tEd Num\tSpawnID\tFilter\tSource\n", PClass::AllClasses.Size());
	for (unsigned int i = 0; i < PClass::AllClasses.Size(); i++)
	{
		PClass *cls = PClass::AllClasses[i];
		PClassActor *acls = dyn_cast<PClassActor>(cls);
		if (acls != NULL)
		{
			Printf("%s\t%i\t%i\t%s\t%s\n",
				acls->TypeName.GetChars(), acls->DoomEdNum,
				acls->SpawnID, filters[acls->GameFilter & 31],
				acls->SourceLumpName.GetChars());
		}
		else if (cls != NULL)
		{
			Printf("%s\tn/a\tn/a\tn/a\tEngine (not an actor type)\n", cls->TypeName.GetChars());
		}
		else
		{
			Printf("Type %i is not an object class\n", i);
		}
	}
}

CCMD (dumpclasses)
{
	// This is by no means speed-optimized. But it's an informational console
	// command that will be executed infrequently, so I don't mind.
	struct DumpInfo
	{
		const PClass *Type;
		DumpInfo *Next;
		DumpInfo *Children;

		static DumpInfo *FindType (DumpInfo *root, const PClass *type)
		{
			if (root == NULL)
			{
				return root;
			}
			if (root->Type == type)
			{
				return root;
			}
			if (root->Next != NULL)
			{
				return FindType (root->Next, type);
			}
			if (root->Children != NULL)
			{
				return FindType (root->Children, type);
			}
			return NULL;
		}

		static DumpInfo *AddType (DumpInfo **root, const PClass *type)
		{
			DumpInfo *info, *parentInfo;

			if (*root == NULL)
			{
				info = new DumpInfo;
				info->Type = type;
				info->Next = NULL;
				info->Children = *root;
				*root = info;
				return info;
			}
			if (type->ParentClass == (*root)->Type)
			{
				parentInfo = *root;
			}
			else if (type == (*root)->Type)
			{
				return *root;
			}
			else
			{
				parentInfo = FindType (*root, type->ParentClass);
				if (parentInfo == NULL)
				{
					parentInfo = AddType (root, type->ParentClass);
				}
			}
			// Has this type already been added?
			for (info = parentInfo->Children; info != NULL; info = info->Next)
			{
				if (info->Type == type)
				{
					return info;
				}
			}
			info = new DumpInfo;
			info->Type = type;
			info->Next = parentInfo->Children;
			info->Children = NULL;
			parentInfo->Children = info;
			return info;
		}

		static void PrintTree (DumpInfo *root, int level)
		{
			Printf ("%*c%s\n", level, ' ', root->Type->TypeName.GetChars());
			if (root->Children != NULL)
			{
				PrintTree (root->Children, level + 2);
			}
			if (root->Next != NULL)
			{
				PrintTree (root->Next, level);
			}
		}

		static void FreeTree (DumpInfo *root)
		{
			if (root->Children != NULL)
			{
				FreeTree (root->Children);
			}
			if (root->Next != NULL)
			{
				FreeTree (root->Next);
			}
			delete root;
		}
	};

	unsigned int i;
	int shown, omitted;
	DumpInfo *tree = NULL;
	const PClass *root = NULL;

	if (argv.argc() > 1)
	{
		root = PClass::FindClass (argv[1]);
		if (root == NULL)
		{
			Printf ("Class '%s' not found\n", argv[1]);
			return;
		}
	}

	shown = omitted = 0;
	DumpInfo::AddType (&tree, root != NULL ? root : RUNTIME_CLASS(DObject));
	for (i = 0; i < PClass::AllClasses.Size(); i++)
	{
		PClass *cls = PClass::AllClasses[i];
		if (root == NULL || cls == root || cls->IsDescendantOf(root))
		{
			DumpInfo::AddType (&tree, cls);
//			Printf (" %s\n", PClass::m_Types[i]->Name + 1);
			shown++;
		}
		else
		{
			omitted++;
		}
	}
	DumpInfo::PrintTree (tree, 2);
	DumpInfo::FreeTree (tree);
	Printf ("%d classes shown, %d omitted\n", shown, omitted);
}

void DObject::InPlaceConstructor (void *mem)
{
	new ((EInPlace *)mem) DObject;
}

DObject::DObject ()
: Class(0), ObjectFlags(0)
{
	ObjectFlags = GC::CurrentWhite & OF_WhiteBits;
	ObjNext = GC::Root;
	GC::Root = this;
}

DObject::DObject (PClass *inClass)
: Class(inClass), ObjectFlags(0)
{
	ObjectFlags = GC::CurrentWhite & OF_WhiteBits;
	ObjNext = GC::Root;
	GC::Root = this;
}

DObject::~DObject ()
{
	if (!PClass::bShutdown)
	{
		PClass *type = GetClass();
		if (!(ObjectFlags & OF_Cleanup) && !PClass::bShutdown)
		{
			DObject **probe;

			if (!(ObjectFlags & OF_YesReallyDelete))
			{
				Printf("Warning: '%s' is freed outside the GC process.\n",
					type != NULL ? type->TypeName.GetChars() : "==some object==");
			}

			// Find all pointers that reference this object and NULL them.
			StaticPointerSubstitution(this, NULL);

			// Now unlink this object from the GC list.
			for (probe = &GC::Root; *probe != NULL; probe = &((*probe)->ObjNext))
			{
				if (*probe == this)
				{
					*probe = ObjNext;
					if (&ObjNext == GC::SweepPos)
					{
						GC::SweepPos = probe;
					}
					break;
				}
			}

			// If it's gray, also unlink it from the gray list.
			if (this->IsGray())
			{
				for (probe = &GC::Gray; *probe != NULL; probe = &((*probe)->GCNext))
				{
					if (*probe == this)
					{
						*probe = GCNext;
						break;
					}
				}
			}
		}
		type->DestroySpecials(this);
	}
}

void DObject::Destroy ()
{
	ObjectFlags = (ObjectFlags & ~OF_Fixed) | OF_EuthanizeMe;
}

size_t DObject::PropagateMark()
{
	const PClass *info = GetClass();
	if (!PClass::bShutdown)
	{
		const size_t *offsets = info->FlatPointers;
		if (offsets == NULL)
		{
			const_cast<PClass *>(info)->BuildFlatPointers();
			offsets = info->FlatPointers;
		}
		while (*offsets != ~(size_t)0)
		{
			GC::Mark((DObject **)((BYTE *)this + *offsets));
			offsets++;
		}
		return info->Size;
	}
	return 0;
}

size_t DObject::PointerSubstitution (DObject *old, DObject *notOld)
{
	const PClass *info = GetClass();
	const size_t *offsets = info->FlatPointers;
	size_t changed = 0;
	if (offsets == NULL)
	{
		const_cast<PClass *>(info)->BuildFlatPointers();
		offsets = info->FlatPointers;
	}
	while (*offsets != ~(size_t)0)
	{
		if (*(DObject **)((BYTE *)this + *offsets) == old)
		{
			*(DObject **)((BYTE *)this + *offsets) = notOld;
			changed++;
		}
		offsets++;
	}
	return changed;
}

size_t DObject::StaticPointerSubstitution (DObject *old, DObject *notOld)
{
	DObject *probe;
	size_t changed = 0;
	int i;

	// Go through all objects.
	i = 0;DObject *last=0;
	for (probe = GC::Root; probe != NULL; probe = probe->ObjNext)
	{
		i++;
		changed += probe->PointerSubstitution(old, notOld);
		last = probe;
	}

	// Go through the bodyque.
	for (i = 0; i < BODYQUESIZE; ++i)
	{
		if (bodyque[i] == old)
		{
			bodyque[i] = static_cast<AActor *>(notOld);
			changed++;
		}
	}

	// Go through players.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
			changed += players[i].FixPointers (old, notOld);
	}

	// Go through sectors.
	if (sectors != NULL)
	{
		for (i = 0; i < numsectors; ++i)
		{
#define SECTOR_CHECK(f,t) \
	if (sectors[i].f.p == static_cast<t *>(old)) { sectors[i].f = static_cast<t *>(notOld); changed++; }
			SECTOR_CHECK( SoundTarget, AActor );
			SECTOR_CHECK( SkyBoxes[sector_t::ceiling], ASkyViewpoint );
			SECTOR_CHECK( SkyBoxes[sector_t::floor], ASkyViewpoint );
			SECTOR_CHECK( SecActTarget, ASectorAction );
			SECTOR_CHECK( floordata, DSectorEffect );
			SECTOR_CHECK( ceilingdata, DSectorEffect );
			SECTOR_CHECK( lightingdata, DSectorEffect );
#undef SECTOR_CHECK
		}
	}

	// Go through bot stuff.
	if (bglobal.firstthing.p == (AActor *)old)		bglobal.firstthing = (AActor *)notOld, ++changed;
	if (bglobal.body1.p == (AActor *)old)			bglobal.body1 = (AActor *)notOld, ++changed;
	if (bglobal.body2.p == (AActor *)old)			bglobal.body2 = (AActor *)notOld, ++changed;

	return changed;
}

void DObject::SerializeUserVars(FArchive &arc)
{
	PSymbolTable *symt;
	FName varname;
	DWORD count, j;
	int *varloc = NULL;

	symt = &GetClass()->Symbols;

	if (arc.IsStoring())
	{
		// Write all fields that aren't serialized by native code.
		GetClass()->WriteValue(arc, this);
	}
	else if (SaveVersion >= 4535)
	{
		GetClass()->ReadValue(arc, this);
	}
	else
	{ // Old version that only deals with ints
		// Read user variables until 'None' is encountered.
		arc << varname;
		while (varname != NAME_None)
		{
			PField *var = dyn_cast<PField>(symt->FindSymbol(varname, true));
			DWORD wanted = 0;

			if (var != NULL && !(var->Flags & VARF_Native))
			{
				PType *type = var->Type;
				PArray *arraytype = dyn_cast<PArray>(type);
				if (arraytype != NULL)
				{
					wanted = arraytype->ElementCount;
					type = arraytype->ElementType;
				}
				else
				{
					wanted = 1;
				}
				assert(type == TypeSInt32);
				varloc = (int *)(reinterpret_cast<BYTE *>(this) + var->Offset);
			}
			count = arc.ReadCount();
			for (j = 0; j < MIN(wanted, count); ++j)
			{
				arc << varloc[j];
			}
			if (wanted < count)
			{
				// Ignore remaining values from archive.
				for (; j < count; ++j)
				{
					int foo;
					arc << foo;
				}
			}
			arc << varname;
		}
	}
}

void DObject::Serialize (FArchive &arc)
{
	ObjectFlags |= OF_SerialSuccess;
}

void DObject::CheckIfSerialized () const
{
	if (!(ObjectFlags & OF_SerialSuccess))
	{
		I_Error (
			"BUG: %s::Serialize\n"
			"(or one of its superclasses) needs to call\n"
			"Super::Serialize\n",
			StaticType()->TypeName.GetChars());
	}
}

