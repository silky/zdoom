/*
** r_interpolate.cpp
**
** Movement interpolation
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

#include "p_3dmidtex.h"
#include "stats.h"
#include "r_data/r_interpolate.h"
#include "p_local.h"
#include "i_system.h"
#include "po_man.h"
#include "farchive.h"

//==========================================================================
//
//
//
//==========================================================================

class DSectorPlaneInterpolation : public DInterpolation
{
	DECLARE_CLASS(DSectorPlaneInterpolation, DInterpolation)

	sector_t *sector;
	double oldheight, oldtexz;
	double bakheight, baktexz;
	bool ceiling;
	TArray<DInterpolation *> attached;


public:

	DSectorPlaneInterpolation() {}
	DSectorPlaneInterpolation(sector_t *sector, bool plane, bool attach);
	void Destroy();
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	void Serialize(FArchive &arc);
	size_t PointerSubstitution (DObject *old, DObject *notOld);
	size_t PropagateMark();
};

//==========================================================================
//
//
//
//==========================================================================

class DSectorScrollInterpolation : public DInterpolation
{
	DECLARE_CLASS(DSectorScrollInterpolation, DInterpolation)

	sector_t *sector;
	double oldx, oldy;
	double bakx, baky;
	bool ceiling;

public:

	DSectorScrollInterpolation() {}
	DSectorScrollInterpolation(sector_t *sector, bool plane);
	void Destroy();
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	void Serialize(FArchive &arc);
};


//==========================================================================
//
//
//
//==========================================================================

class DWallScrollInterpolation : public DInterpolation
{
	DECLARE_CLASS(DWallScrollInterpolation, DInterpolation)

	side_t *side;
	int part;
	double oldx, oldy;
	double bakx, baky;

public:

	DWallScrollInterpolation() {}
	DWallScrollInterpolation(side_t *side, int part);
	void Destroy();
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	void Serialize(FArchive &arc);
};

//==========================================================================
//
//
//
//==========================================================================

class DPolyobjInterpolation : public DInterpolation
{
	DECLARE_CLASS(DPolyobjInterpolation, DInterpolation)

	FPolyObj *poly;
	TArray<double> oldverts, bakverts;
	double oldcx, oldcy;
	double bakcx, bakcy;

public:

	DPolyobjInterpolation() {}
	DPolyobjInterpolation(FPolyObj *poly);
	void Destroy();
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	void Serialize(FArchive &arc);
};


//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_ABSTRACT_POINTY_CLASS(DInterpolation)
DECLARE_POINTER(Next)
DECLARE_POINTER(Prev)
END_POINTERS
IMPLEMENT_CLASS(DSectorPlaneInterpolation)
IMPLEMENT_CLASS(DSectorScrollInterpolation)
IMPLEMENT_CLASS(DWallScrollInterpolation)
IMPLEMENT_CLASS(DPolyobjInterpolation)

//==========================================================================
//
// Important note:
// The linked list of interpolations and the pointers in the interpolated
// objects are not processed by the garbage collector. This is intentional!
//
// If an interpolation is no longer owned by any thinker it should
// be destroyed even if the interpolator still has a link to it.
//
// When such an interpolation is destroyed by the garbage collector it
// will automatically be unlinked from the list.
//
//==========================================================================

FInterpolator interpolator;

//==========================================================================
//
//
//
//==========================================================================

int FInterpolator::CountInterpolations ()
{
	return count;
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::UpdateInterpolations()
{
	for (DInterpolation *probe = Head; probe != NULL; probe = probe->Next)
	{
		probe->UpdateInterpolation ();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::AddInterpolation(DInterpolation *interp)
{
	interp->Next = Head;
	if (Head != NULL) Head->Prev = interp;
	interp->Prev = NULL;
	Head = interp;
	count++;
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::RemoveInterpolation(DInterpolation *interp)
{
	if (Head == interp)
	{
		Head = interp->Next;
		if (Head != NULL) Head->Prev = NULL;
	}
	else
	{
		if (interp->Prev != NULL) interp->Prev->Next = interp->Next;
		if (interp->Next != NULL) interp->Next->Prev = interp->Prev;
	}
	interp->Next = NULL;
	interp->Prev = NULL;
	count--;
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::DoInterpolations(double smoothratio)
{
	if (smoothratio >= 1.)
	{
		didInterp = false;
		return;
	}

	didInterp = true;

	DInterpolation *probe = Head;
	while (probe != NULL)
	{
		DInterpolation *next = probe->Next;
		probe->Interpolate(smoothratio);
		probe = next;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::RestoreInterpolations()
{
	if (didInterp)
	{
		didInterp = false;
		for (DInterpolation *probe = Head; probe != NULL; probe = probe->Next)
		{
			probe->Restore();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::ClearInterpolations()
{
	DInterpolation *probe = Head;
	Head = NULL;
	while (probe != NULL)
	{
		DInterpolation *next = probe->Next;
		probe->Next = probe->Prev = NULL;
		probe->Destroy();
		probe = next;
	}
}


//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DInterpolation::DInterpolation()
{
	Next = NULL;
	Prev = NULL;
	refcount = 0;
}

//==========================================================================
//
//
//
//==========================================================================

int DInterpolation::AddRef()
{
	return ++refcount;
}

//==========================================================================
//
//
//
//==========================================================================

int DInterpolation::DelRef(bool force)
{
	if (refcount > 0) --refcount;
	if (force && refcount == 0) Destroy();
	return refcount;
}

//==========================================================================
//
//
//
//==========================================================================

void DInterpolation::Destroy()
{
	interpolator.RemoveInterpolation(this);
	refcount = 0;
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << refcount;
	if (arc.IsLoading())
	{
		interpolator.AddInterpolation(this);
	}
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DSectorPlaneInterpolation::DSectorPlaneInterpolation(sector_t *_sector, bool _plane, bool attach)
{
	sector = _sector;
	ceiling = _plane;
	UpdateInterpolation ();

	if (attach)
	{
		P_Start3dMidtexInterpolations(attached, sector, ceiling);
		P_StartLinkedSectorInterpolations(attached, sector, ceiling);
	}
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Destroy()
{
	if (ceiling) 
	{
		sector->interpolations[sector_t::CeilingMove] = NULL;
	}
	else 
	{
		sector->interpolations[sector_t::FloorMove] = NULL;
	}

	for(unsigned i=0; i<attached.Size(); i++)
	{
		attached[i]->DelRef();
	}
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::UpdateInterpolation()
{
	if (!ceiling)
	{
		oldheight = sector->floorplane.fD();
		oldtexz = sector->GetPlaneTexZF(sector_t::floor);
	}
	else
	{
		oldheight = sector->ceilingplane.fD();
		oldtexz = sector->GetPlaneTexZF(sector_t::ceiling);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Restore()
{
	if (!ceiling)
	{
		sector->floorplane.setD(bakheight);
		sector->SetPlaneTexZ(sector_t::floor, baktexz);
	}
	else
	{
		sector->ceilingplane.setD(bakheight);
		sector->SetPlaneTexZ(sector_t::ceiling, baktexz);
	}
	P_RecalculateAttached3DFloors(sector);
	sector->CheckPortalPlane(ceiling? sector_t::ceiling : sector_t::floor);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Interpolate(double smoothratio)
{
	secplane_t *pplane;
	int pos;

	if (!ceiling)
	{
		pplane = &sector->floorplane;
		pos = sector_t::floor;
	}
	else
	{
		pplane = &sector->ceilingplane;
		pos = sector_t::ceiling;
	}

	bakheight = pplane->fD();
	baktexz = sector->GetPlaneTexZF(pos);

	if (refcount == 0 && oldheight == bakheight)
	{
		Destroy();
	}
	else
	{
		pplane->setD(oldheight + (bakheight - oldheight) * smoothratio);
		sector->SetPlaneTexZ(pos, oldtexz + (baktexz - oldtexz) * smoothratio);
		P_RecalculateAttached3DFloors(sector);
		sector->CheckPortalPlane(pos);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << sector << ceiling << oldheight << oldtexz << attached;
}

//==========================================================================
//
//
//
//==========================================================================

size_t DSectorPlaneInterpolation::PointerSubstitution (DObject *old, DObject *notOld)
{
	int subst = 0;
	for(unsigned i=0; i<attached.Size(); i++)
	{
		if (attached[i] == old) 
		{
			attached[i] = (DInterpolation*)notOld;
			subst++;
		}
	}
	return subst;
}

//==========================================================================
//
//
//
//==========================================================================

size_t DSectorPlaneInterpolation::PropagateMark()
{
	for(unsigned i=0; i<attached.Size(); i++)
	{
		GC::Mark(attached[i]);
	}
	return Super::PropagateMark();
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DSectorScrollInterpolation::DSectorScrollInterpolation(sector_t *_sector, bool _plane)
{
	sector = _sector;
	ceiling = _plane;
	UpdateInterpolation ();
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Destroy()
{
	if (ceiling) 
	{
		sector->interpolations[sector_t::CeilingScroll] = NULL;
	}
	else 
	{
		sector->interpolations[sector_t::FloorScroll] = NULL;
	}
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::UpdateInterpolation()
{
	oldx = sector->GetXOffsetF(ceiling);
	oldy = sector->GetYOffsetF(ceiling, false);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Restore()
{
	sector->SetXOffset(ceiling, bakx);
	sector->SetYOffset(ceiling, baky);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Interpolate(double smoothratio)
{
	bakx = sector->GetXOffsetF(ceiling);
	baky = sector->GetYOffsetF(ceiling, false);

	if (refcount == 0 && oldx == bakx && oldy == baky)
	{
		Destroy();
	}
	else
	{
		sector->SetXOffset(ceiling, oldx + (bakx - oldx) * smoothratio);
		sector->SetYOffset(ceiling, oldy + (baky - oldy) * smoothratio);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << sector << ceiling << oldx << oldy;
}


//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DWallScrollInterpolation::DWallScrollInterpolation(side_t *_side, int _part)
{
	side = _side;
	part = _part;
	UpdateInterpolation ();
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Destroy()
{
	side->textures[part].interpolation = NULL;
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::UpdateInterpolation()
{
	oldx = side->GetTextureXOffsetF(part);
	oldy = side->GetTextureYOffsetF(part);
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Restore()
{
	side->SetTextureXOffset(part, bakx);
	side->SetTextureYOffset(part, baky);
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Interpolate(double smoothratio)
{
	bakx = side->GetTextureXOffsetF(part);
	baky = side->GetTextureYOffsetF(part);

	if (refcount == 0 && oldx == bakx && oldy == baky)
	{
		Destroy();
	}
	else
	{
		side->SetTextureXOffset(part, oldx + (bakx - oldx) * smoothratio);
		side->SetTextureYOffset(part, oldy + (baky - oldy) * smoothratio);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << side << part << oldx << oldy;
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DPolyobjInterpolation::DPolyobjInterpolation(FPolyObj *po)
{
	poly = po;
	oldverts.Resize(po->Vertices.Size() << 1);
	bakverts.Resize(po->Vertices.Size() << 1);
	UpdateInterpolation ();
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Destroy()
{
	poly->interpolation = NULL;
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::UpdateInterpolation()
{
	for(unsigned int i = 0; i < poly->Vertices.Size(); i++)
	{
		oldverts[i*2  ] = poly->Vertices[i]->fX();
		oldverts[i*2+1] = poly->Vertices[i]->fY();
	}
	oldcx = poly->CenterSpot.pos.X;
	oldcy = poly->CenterSpot.pos.Y;
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Restore()
{
	for(unsigned int i = 0; i < poly->Vertices.Size(); i++)
	{
		poly->Vertices[i]->set(bakverts[i*2  ], bakverts[i*2+1]);
	}
	poly->CenterSpot.pos.X = bakcx;
	poly->CenterSpot.pos.Y = bakcy;
	poly->ClearSubsectorLinks();
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Interpolate(double smoothratio)
{
	bool changed = false;
	for(unsigned int i = 0; i < poly->Vertices.Size(); i++)
	{
		bakverts[i*2  ] = poly->Vertices[i]->fX();
		bakverts[i*2+1] = poly->Vertices[i]->fY();

		if (bakverts[i * 2] != oldverts[i * 2] || bakverts[i * 2 + 1] != oldverts[i * 2 + 1])
		{
			changed = true;
			poly->Vertices[i]->set(
				oldverts[i * 2] + (bakverts[i * 2] - oldverts[i * 2]) * smoothratio,
				oldverts[i * 2 + 1] + (bakverts[i * 2 + 1] - oldverts[i * 2 + 1]) * smoothratio);
		}
	}
	if (refcount == 0 && !changed)
	{
		Destroy();
	}
	else
	{
		bakcx = poly->CenterSpot.pos.X;
		bakcy = poly->CenterSpot.pos.Y;
		poly->CenterSpot.pos.X = bakcx + (bakcx - oldcx) * smoothratio;
		poly->CenterSpot.pos.Y = bakcy + (bakcy - oldcy) * smoothratio;

		poly->ClearSubsectorLinks();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	int po = int(poly - polyobjs);
	arc << po << oldverts;
	poly = polyobjs + po;

	arc << oldcx << oldcy;
	if (arc.IsLoading()) bakverts.Resize(oldverts.Size());
}


//==========================================================================
//
//
//
//==========================================================================

DInterpolation *side_t::SetInterpolation(int position)
{
	if (textures[position].interpolation == NULL)
	{
		textures[position].interpolation = new DWallScrollInterpolation(this, position);
	}
	textures[position].interpolation->AddRef();
	GC::WriteBarrier(textures[position].interpolation);
	return textures[position].interpolation;
}

//==========================================================================
//
//
//
//==========================================================================

void side_t::StopInterpolation(int position)
{
	if (textures[position].interpolation != NULL)
	{
		textures[position].interpolation->DelRef();
	}
}

//==========================================================================
//
//
//
//==========================================================================

DInterpolation *sector_t::SetInterpolation(int position, bool attach)
{
	if (interpolations[position] == NULL)
	{
		DInterpolation *interp;
		switch (position)
		{
		case sector_t::CeilingMove:
			interp = new DSectorPlaneInterpolation(this, true, attach);
			break;

		case sector_t::FloorMove:
			interp = new DSectorPlaneInterpolation(this, false, attach);
			break;

		case sector_t::CeilingScroll:
			interp = new DSectorScrollInterpolation(this, true);
			break;

		case sector_t::FloorScroll:
			interp = new DSectorScrollInterpolation(this, false);
			break;

		default:
			return NULL;
		}
		interpolations[position] = interp;
	}
	interpolations[position]->AddRef();
	GC::WriteBarrier(interpolations[position]);
	return interpolations[position];
}

//==========================================================================
//
//
//
//==========================================================================

DInterpolation *FPolyObj::SetInterpolation()
{
	if (interpolation != NULL)
	{
		interpolation->AddRef();
	}
	else
	{
		interpolation = new DPolyobjInterpolation(this);
		interpolation->AddRef();
	}
	GC::WriteBarrier(interpolation);
	return interpolation;
}

//==========================================================================
//
//
//
//==========================================================================

void FPolyObj::StopInterpolation()
{
	if (interpolation != NULL)
	{
		interpolation->DelRef();
	}
}


ADD_STAT (interpolations)
{
	FString out;
	out.Format ("%d interpolations", interpolator.CountInterpolations ());
	return out;
}


