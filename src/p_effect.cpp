/*
** p_effect.cpp
** Particle effects
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
** If particles used real sprites instead of blocks, they could be much
** more useful.
*/

#include "doomtype.h"
#include "doomstat.h"
#include "i_system.h"
#include "c_cvars.h"
#include "actor.h"
#include "m_argv.h"
#include "p_effect.h"
#include "p_local.h"
#include "g_level.h"
#include "v_video.h"
#include "m_random.h"
#include "r_defs.h"
#include "s_sound.h"
#include "templates.h"
#include "gi.h"
#include "v_palette.h"
#include "colormatcher.h"
#include "d_player.h"
#include "r_utility.h"

CVAR (Int, cl_rockettrails, 1, CVAR_ARCHIVE);
CVAR (Bool, r_rail_smartspiral, 0, CVAR_ARCHIVE);
CVAR (Int, r_rail_spiralsparsity, 1, CVAR_ARCHIVE);
CVAR (Int, r_rail_trailsparsity, 1, CVAR_ARCHIVE);
CVAR (Bool, r_particles, true, 0);

FRandom pr_railtrail("RailTrail");

#define FADEFROMTTL(a)	(255/(a))

// [RH] particle globals
WORD			NumParticles;
WORD			ActiveParticles;
WORD			InactiveParticles;
particle_t		*Particles;
TArray<WORD>	ParticlesInSubsec;

static int grey1, grey2, grey3, grey4, red, green, blue, yellow, black,
		   red1, green1, blue1, yellow1, purple, purple1, white,
		   rblue1, rblue2, rblue3, rblue4, orange, yorange, dred, grey5,
		   maroon1, maroon2, blood1, blood2;

static const struct ColorList {
	int *color;
	BYTE r, g, b;
} Colors[] = {
	{&grey1,	85,  85,  85 },
	{&grey2,	171, 171, 171},
	{&grey3,	50,  50,  50 },
	{&grey4,	210, 210, 210},
	{&grey5,	128, 128, 128},
	{&red,		255, 0,   0  },  
	{&green,	0,   200, 0  },  
	{&blue,		0,   0,   255},
	{&yellow,	255, 255, 0  },  
	{&black,	0,   0,   0  },  
	{&red1,		255, 127, 127},
	{&green1,	127, 255, 127},
	{&blue1,	127, 127, 255},
	{&yellow1,	255, 255, 180},
	{&purple,	120, 0,   160},
	{&purple1,	200, 30,  255},
	{&white, 	255, 255, 255},
	{&rblue1,	81,  81,  255},
	{&rblue2,	0,   0,   227},
	{&rblue3,	0,   0,   130},
	{&rblue4,	0,   0,   80 },
	{&orange,	255, 120, 0  },
	{&yorange,	255, 170, 0  },
	{&dred,		80,  0,   0  },
	{&maroon1,	154, 49,  49 },
	{&maroon2,	125, 24,  24 },
	{NULL, 0, 0, 0 }
};

inline particle_t *NewParticle (void)
{
	particle_t *result = NULL;
	if (InactiveParticles != NO_PARTICLE)
	{
		result = Particles + InactiveParticles;
		InactiveParticles = result->tnext;
		result->tnext = ActiveParticles;
		ActiveParticles = WORD(result - Particles);
	}
	return result;
}

//
// [RH] Particle functions
//
void P_InitParticles ();
void P_DeinitParticles ();

// [BC] Allow the maximum number of particles to be specified by a cvar (so people
// with lots of nice hardware can have lots of particles!).
CUSTOM_CVAR( Int, r_maxparticles, 4000, CVAR_ARCHIVE )
{
	if ( self == 0 )
		self = 4000;
	else if (self > 65535)
		self = 65535;
	else if (self < 100)
		self = 100;

	if ( gamestate != GS_STARTUP )
	{
		P_DeinitParticles( );
		P_InitParticles( );
	}
}

void P_InitParticles ()
{
	const char *i;
	int num;

	if ((i = Args->CheckValue ("-numparticles")))
		num = atoi (i);
	// [BC] Use r_maxparticles now.
	else
		num = r_maxparticles;

	// This should be good, but eh...
	NumParticles = (WORD)clamp<int>(num, 100, 65535);

	P_DeinitParticles();
	Particles = new particle_t[NumParticles];
	P_ClearParticles ();
	atterm (P_DeinitParticles);
}

void P_DeinitParticles()
{
	if (Particles != NULL)
	{
		delete[] Particles;
		Particles = NULL;
	}
}

void P_ClearParticles ()
{
	int i;

	memset (Particles, 0, NumParticles * sizeof(particle_t));
	ActiveParticles = NO_PARTICLE;
	InactiveParticles = 0;
	for (i = 0; i < NumParticles-1; i++)
		Particles[i].tnext = i + 1;
	Particles[i].tnext = NO_PARTICLE;
}

// Group particles by subsectors. Because particles are always
// in motion, there is little benefit to caching this information
// from one frame to the next.

void P_FindParticleSubsectors ()
{
	if (ParticlesInSubsec.Size() < (size_t)numsubsectors)
	{
		ParticlesInSubsec.Reserve (numsubsectors - ParticlesInSubsec.Size());
	}

	clearbufshort (&ParticlesInSubsec[0], numsubsectors, NO_PARTICLE);

	if (!r_particles)
	{
		return;
	}
	for (WORD i = ActiveParticles; i != NO_PARTICLE; i = Particles[i].tnext)
	{
		 // Try to reuse the subsector from the last portal check, if still valid.
		if (Particles[i].subsector == NULL) Particles[i].subsector = R_PointInSubsector(Particles[i].Pos);
		int ssnum = int(Particles[i].subsector - subsectors);
		Particles[i].snext = ParticlesInSubsec[ssnum];
		ParticlesInSubsec[ssnum] = i;
	}
}

static TMap<int, int> ColorSaver;

static uint32 ParticleColor(int rgb)
{
	int *val;
	int stuff;

	val = ColorSaver.CheckKey(rgb);
	if (val != NULL)
	{
		return *val;
	}
	stuff = rgb | (ColorMatcher.Pick(RPART(rgb), GPART(rgb), BPART(rgb)) << 24);
	ColorSaver[rgb] = stuff;
	return stuff;
}

static uint32 ParticleColor(int r, int g, int b)
{
	return ParticleColor(MAKERGB(r, g, b));
}

void P_InitEffects ()
{
	const struct ColorList *color = Colors;

	P_InitParticles();
	while (color->color)
	{
		*(color->color) = ParticleColor(color->r, color->g, color->b);
		color++;
	}

	int kind = gameinfo.defaultbloodparticlecolor;
	blood1 = ParticleColor(kind);
	blood2 = ParticleColor(RPART(kind)/3, GPART(kind)/3, BPART(kind)/3);
}


void P_ThinkParticles ()
{
	int i;
	particle_t *particle, *prev;

	i = ActiveParticles;
	prev = NULL;
	while (i != NO_PARTICLE)
	{
		BYTE oldtrans;

		particle = Particles + i;
		i = particle->tnext;
		oldtrans = particle->trans;
		particle->trans -= particle->fade;
		if (oldtrans < particle->trans || --particle->ttl == 0)
		{ // The particle has expired, so free it
			memset (particle, 0, sizeof(particle_t));
			if (prev)
				prev->tnext = i;
			else
				ActiveParticles = i;
			particle->tnext = InactiveParticles;
			InactiveParticles = (int)(particle - Particles);
			continue;
		}

		// Handle crossing a line portal
		DVector2 newxy = P_GetOffsetPosition(particle->Pos.X, particle->Pos.Y, particle->Vel.X, particle->Vel.Y);
		particle->Pos.X = newxy.X;
		particle->Pos.Y = newxy.Y;
		particle->Pos.Z += particle->Vel.Z;
		particle->Vel += particle->Acc;
		particle->subsector = R_PointInSubsector(particle->Pos);
		// Handle crossing a sector portal.
		if (!particle->subsector->sector->PortalBlocksMovement(sector_t::ceiling))
		{
			AActor *skybox = particle->subsector->sector->SkyBoxes[sector_t::ceiling];
			if (particle->Pos.Z > skybox->specialf1)
			{
				particle->Pos += skybox->Scale;
				particle->subsector = NULL;
			}
		}
		else if (!particle->subsector->sector->PortalBlocksMovement(sector_t::floor))
		{
			AActor *skybox = particle->subsector->sector->SkyBoxes[sector_t::floor];
			if (particle->Pos.Z < skybox->specialf1)
			{
				particle->Pos += skybox->Scale;
				particle->subsector = NULL;
			}
		}
		prev = particle;
	}
}


void P_SpawnParticle(const DVector3 &pos, const DVector3 &vel, const DVector3 &accel, PalEntry color, bool fullbright, double startalpha, int lifetime, WORD size, double fadestep)
{
	particle_t *particle = NewParticle();

	if (particle)
	{
		particle->Pos = pos;
		particle->Vel = vel;
		particle->Acc = accel;
		particle->color = ParticleColor(color);
		particle->trans = BYTE(startalpha*255);
		if (fadestep < 0) particle->fade = FADEFROMTTL(lifetime);
		else particle->fade = int(fadestep * 255);
		particle->ttl = lifetime;
		particle->bright = fullbright;
		particle->size = (WORD)size;
	}
}

//
// P_RunEffects
//
// Run effects on all actors in the world
//
void P_RunEffects ()
{
	if (players[consoleplayer].camera == NULL) return;

	int	pnum = int(players[consoleplayer].camera->Sector - sectors) * numsectors;

	AActor *actor;
	TThinkerIterator<AActor> iterator;

	while ( (actor = iterator.Next ()) )
	{
		if (actor->effects)
		{
			// Only run the effect if the actor is potentially visible
			int rnum = pnum + int(actor->Sector - sectors);
			if (rejectmatrix == NULL || !(rejectmatrix[rnum>>3] & (1 << (rnum & 7))))
				P_RunEffect (actor, actor->effects);
		}
	}
}

//
// JitterParticle
//
// Creates a particle with "jitter"
//
particle_t *JitterParticle (int ttl)
{
	return JitterParticle (ttl, 1.0);
}
// [XA] Added "drift speed" multiplier setting for enhanced railgun stuffs.
particle_t *JitterParticle (int ttl, double drift)
{
	particle_t *particle = NewParticle ();

	if (particle) {
		int i;

		// Set initial velocities
		for (i = 3; i; i--)
			particle->Vel[i] = ((1./4096) * (M_Random () - 128) * drift);
		// Set initial accelerations
		for (i = 3; i; i--)
			particle->Acc[i] = ((1./16384) * (M_Random () - 128) * drift);

		particle->trans = 255;	// fully opaque
		particle->ttl = ttl;
		particle->fade = FADEFROMTTL(ttl);
	}
	return particle;
}

static void MakeFountain (AActor *actor, int color1, int color2)
{
	particle_t *particle;

	if (!(level.time & 1))
		return;

	particle = JitterParticle (51);

	if (particle)
	{
		DAngle an = M_Random() * (360. / 256);
		double out = actor->radius * M_Random() / 256.;

		particle->Pos = actor->Vec3Angle(out, an, actor->Height + 1);
		if (out < actor->radius/8)
			particle->Vel.Z += 10./3;
		else
			particle->Vel.Z += 3;
		particle->Acc.Z -= 1./11;
		if (M_Random() < 30) {
			particle->size = 4;
			particle->color = color2;
		} else {
			particle->size = 6;
			particle->color = color1;
		}
	}
}

void P_RunEffect (AActor *actor, int effects)
{
	DAngle moveangle = actor->Vel.Angle();

	particle_t *particle;
	int i;

	if ((effects & FX_ROCKET) && (cl_rockettrails & 1))
	{
		// Rocket trail
		double backx = -actor->radius * 2 * moveangle.Cos();
		double backy = -actor->radius * 2 * moveangle.Sin();
		double backz = actor->Height * ((2. / 3) - actor->Vel.Z / 8);

		DAngle an = moveangle + 90.;
		double speed;

		particle = JitterParticle (3 + (M_Random() & 31));
		if (particle) {
			double pathdist = M_Random() / 256.;
			DVector3 pos = actor->Vec3Offset(
				backx - actor->Vel.X * pathdist,
				backy - actor->Vel.Y * pathdist,
				backz - actor->Vel.Z * pathdist);
			particle->Pos = pos;
			speed = (M_Random () - 128) * (1./200);
			particle->Vel.X += speed * an.Cos();
			particle->Vel.Y += speed * an.Sin();
			particle->Vel.Z -= 1./36;
			particle->Acc.Z -= 1./20;
			particle->color = yellow;
			particle->size = 2;
		}
		for (i = 6; i; i--) {
			particle_t *particle = JitterParticle (3 + (M_Random() & 31));
			if (particle) {
				double pathdist = M_Random() / 256.;
				DVector3 pos = actor->Vec3Offset(
					backx - actor->Vel.X * pathdist,
					backy - actor->Vel.Y * pathdist,
					backz - actor->Vel.Z * pathdist + (M_Random() / 64.));
				particle->Pos = pos;

				speed = (M_Random () - 128) * (1./200);
				particle->Vel.X += speed * an.Cos();
				particle->Vel.Y += speed * an.Sin();
				particle->Vel.Z -= 1. / 80;
				particle->Acc.Z -= 1. / 40;
				if (M_Random () & 7)
					particle->color = grey2;
				else
					particle->color = grey1;
				particle->size = 3;
			} else
				break;
		}
	}
	if ((effects & FX_GRENADE) && (cl_rockettrails & 1))
	{
		// Grenade trail

		DVector3 pos = actor->Vec3Angle(-actor->radius * 2, moveangle, -actor->Height * actor->Vel.Z / 8 + actor->Height * (2. / 3));

		P_DrawSplash2 (6, pos, moveangle + 180, 2, 2);
	}
	if (effects & FX_FOUNTAINMASK)
	{
		// Particle fountain

		static const int *fountainColors[16] = 
			{ &black,	&black,
			  &red,		&red1,
			  &green,	&green1,
			  &blue,	&blue1,
			  &yellow,	&yellow1,
			  &purple,	&purple1,
			  &black,	&grey3,
			  &grey4,	&white
			};
		int color = (effects & FX_FOUNTAINMASK) >> 15;
		MakeFountain (actor, *fountainColors[color], *fountainColors[color+1]);
	}
	if (effects & FX_RESPAWNINVUL)
	{
		// Respawn protection

		static const int *protectColors[2] = { &yellow1, &white };

		for (i = 3; i > 0; i--)
		{
			particle = JitterParticle (16);
			if (particle != NULL)
			{
				DAngle ang = M_Random() * (360 / 256.);
				DVector3 pos = actor->Vec3Angle(actor->radius, ang, 0);
				particle->Pos = pos;
				particle->color = *protectColors[M_Random() & 1];
				particle->Vel.Z = 1;
				particle->Acc.Z = M_Random () / 512.;
				particle->size = 1;
				if (M_Random () < 128)
				{ // make particle fall from top of actor
					particle->Pos.Z += actor->Height;
					particle->Vel.Z = -particle->Vel.Z;
					particle->Acc.Z = -particle->Acc.Z;
				}
			}
		}
	}
}

void P_DrawSplash (int count, const DVector3 &pos, DAngle angle, int kind)
{
	int color1, color2;

	switch (kind)
	{
	case 1:		// Spark
		color1 = orange;
		color2 = yorange;
		break;
	default:
		return;
	}

	for (; count; count--)
	{
		particle_t *p = JitterParticle (10);

		if (!p)
			break;

		p->size = 2;
		p->color = M_Random() & 0x80 ? color1 : color2;
		p->Vel.Z -= M_Random () / 128.;
		p->Acc.Z -= 1./8;
		p->Acc.X += (M_Random () - 128) / 8192.;
		p->Acc.Y += (M_Random () - 128) / 8192.;
		p->Pos.Z = pos.Z - M_Random () / 64.;
		angle += M_Random() * (45./256);
		p->Pos.X = pos.X + (M_Random() & 15)*angle.Cos();
		p->Pos.Y = pos.Y + (M_Random() & 15)*angle.Sin();
	}
}

void P_DrawSplash2 (int count, const DVector3 &pos, DAngle angle, int updown, int kind)
{
	int color1, color2, zadd;
	double zvel, zspread;

	switch (kind)
	{
	case 0:		// Blood
		color1 = blood1;
		color2 = blood2;
		break;
	case 1:		// Gunshot
		color1 = grey3;
		color2 = grey5;
		break;
	case 2:		// Smoke
		color1 = grey3;
		color2 = grey1;
		break;
	default:	// colorized blood
		color1 = ParticleColor(kind);
		color2 = ParticleColor(RPART(kind)/3, GPART(kind)/3, BPART(kind)/3);
		break;
	}

	zvel = -1./512.;
	zspread = updown ? -6000 / 65536. : 6000 / 65536.;
	zadd = (updown == 2) ? 128 : 0;

	for (; count; count--)
	{
		particle_t *p = NewParticle ();
		DAngle an;

		if (!p)
			break;

		p->ttl = 12;
		p->fade = FADEFROMTTL(12);
		p->trans = 255;
		p->size = 4;
		p->color = M_Random() & 0x80 ? color1 : color2;
		p->Vel.Z = M_Random() * zvel;
		p->Acc.Z = -1 / 22.;
		if (kind) 
		{
			an = angle + ((M_Random() - 128) * (180 / 256.));
			p->Vel.X = M_Random() * an.Cos() / 2048.;
			p->Vel.Y = M_Random() * an.Sin() / 2048.;
			p->Acc.X = p->Vel.X / 16.;
			p->Acc.Y = p->Vel.Y / 16.;
		}
		an = angle + ((M_Random() - 128) * (90 / 256.));
		p->Pos.X = pos.X + ((M_Random() & 31) - 15) * an.Cos();
		p->Pos.Y = pos.Y + ((M_Random() & 31) - 15) * an.Sin();
		p->Pos.Z = pos.Z + (M_Random() + zadd - 128) * zspread;
	}
}

void P_DrawRailTrail(AActor *source, const DVector3 &start, const DVector3 &end, int color1, int color2, double maxdiff_d, int flags, PClassActor *spawnclass, DAngle angle, int duration, double sparsity, double drift, int SpiralOffset)
{
	double length, lengthsquared;
	int steps, i;
	TAngle<double> deg;
	DVector3 step, dir, pos, extend;
	bool fullbright;
	float maxdiff = (float)maxdiff_d;


	dir = end - start;
	lengthsquared = dir | dir;
	length = g_sqrt(lengthsquared);
	steps = xs_FloorToInt(length / 3);
	fullbright = !!(flags & RAF_FULLBRIGHT);

	if (steps)
	{
		if (!(flags & RAF_SILENT))
		{
			FSoundID sound;
			
			// Allow other sounds than 'weapons/railgf'!
			if (!source->player) sound = source->AttackSound;
			else if (source->player->ReadyWeapon) sound = source->player->ReadyWeapon->AttackSound;
			else sound = 0;
			if (!sound) sound = "weapons/railgf";

			// The railgun's sound is special. It gets played from the
			// point on the slug's trail that is closest to the hearing player.
			AActor *mo = players[consoleplayer].camera;
			DVector3 point;
			double r;
			double dirz;

			if (fabs(mo->X() - start.X) < 20
				&& fabs(mo->Y() - start.Y) < 20)
			{ // This player (probably) fired the railgun
				S_Sound (mo, CHAN_WEAPON, sound, 1, ATTN_NORM);
			}
			else
			{

				// Only consider sound in 2D (for now, anyway)
				// [BB] You have to divide by lengthsquared here, not multiply with it.

				r = ((start.Y - mo->Y()) * (-dir.Y) - (start.X - mo->X()) * (dir.X)) / lengthsquared;
				r = clamp<double>(r, 0., 1.);

				dirz = dir.Z;
				dir.Z = 0;
				point = start + r * dir;
				dir.Z = dirz;

				S_Sound (DVector3(point.X, point.Y, ViewPos.Z),	CHAN_WEAPON, sound, 1, ATTN_NORM);
			}
		}
	}
	else
	{
		// line is 0 length, so nothing to do
		return;
	}

	dir /= length;

	//Calculate PerpendicularVector (extend, dir):
	double minelem = 1;
	int epos;
	for (epos = 0, i = 0; i < 3; ++i)
	{
		if (fabs(dir[i]) < minelem)
		{
			epos = i;
			minelem = fabs(dir[i]);
		}
	}
	DVector3 tempvec(0, 0, 0);
	tempvec[epos] = 1;
	extend = tempvec - (dir | tempvec) * dir;
	//

	extend *= 3;
	step = dir * 3;

	// Create the outer spiral.
	if (color1 != -1 && (!r_rail_smartspiral || color2 == -1) && r_rail_spiralsparsity > 0 && (spawnclass == NULL))
	{
		DVector3 spiral_step = step * r_rail_spiralsparsity * sparsity;
		int spiral_steps = (int)(steps * r_rail_spiralsparsity / sparsity);
		
		color1 = color1 == 0 ? -1 : ParticleColor(color1);
		pos = start;
		deg = (double)SpiralOffset;
		for (i = spiral_steps; i; i--)
		{
			particle_t *p = NewParticle ();
			DVector3 tempvec;

			if (!p)
				return;

			int spiralduration = (duration == 0) ? 35 : duration;

			p->trans = 255;
			p->ttl = duration;
			p->fade = FADEFROMTTL(spiralduration);
			p->size = 3;
			p->bright = fullbright;

			tempvec = DMatrix3x3(dir, deg) * extend;
			p->Vel = tempvec * drift / 16.;
			p->Pos = tempvec + pos;
			pos += spiral_step;
			deg += double(r_rail_spiralsparsity * 14);

			if (color1 == -1)
			{
				int rand = M_Random();

				if (rand < 155)
					p->color = rblue2;
				else if (rand < 188)
					p->color = rblue1;
				else if (rand < 222)
					p->color = rblue3;
				else
					p->color = rblue4;
			}
			else 
			{
				p->color = color1;
			}
		}
	}

	// Create the inner trail.
	if (color2 != -1 && r_rail_trailsparsity > 0 && spawnclass == NULL)
	{
		DVector3 trail_step = step * r_rail_trailsparsity * sparsity;
		int trail_steps = xs_FloorToInt(steps * r_rail_trailsparsity / sparsity);

		color2 = color2 == 0 ? -1 : ParticleColor(color2);
		DVector3 diff(0, 0, 0);

		pos = start;
		for (i = trail_steps; i; i--)
		{
			// [XA] inner trail uses a different default duration (33).
			int innerduration = (duration == 0) ? 33 : duration;
			particle_t *p = JitterParticle (innerduration, (float)drift);

			if (!p)
				return;

			if (maxdiff > 0)
			{
				int rnd = M_Random ();
				if (rnd & 1)
					diff.X = clamp<double>(diff.X + ((rnd & 8) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 2)
					diff.Y = clamp<double>(diff.Y + ((rnd & 16) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 4)
					diff.Z = clamp<double>(diff.Z + ((rnd & 32) ? 1 : -1), -maxdiff, maxdiff);
			}

			DVector3 postmp = pos + diff;

			p->size = 2;
			p->Pos = postmp;
			if (color1 != -1)
				p->Acc.Z -= 1./4096;
			pos += trail_step;

			p->bright = fullbright;

			if (color2 == -1)
			{
				int rand = M_Random();

				if (rand < 85)
					p->color = grey4;
				else if (rand < 170)
					p->color = grey2;
				else
					p->color = grey1;
			}
			else 
			{
				p->color = color2;
			}
		}
	}
	// create actors
	if (spawnclass != NULL)
	{
		if (sparsity < 1)
			sparsity = 32;

		DVector3 trail_step = (step / 3) * sparsity;
		int trail_steps = (int)((steps * 3) / sparsity);
		DVector3 diff(0, 0, 0);

		pos = start;
		for (i = trail_steps; i; i--)
		{
			if (maxdiff > 0)
			{
				int rnd = pr_railtrail();
				if (rnd & 1)
					diff.X = clamp<double>(diff.X + ((rnd & 8) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 2)
					diff.Y = clamp<double>(diff.Y + ((rnd & 16) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 4)
					diff.Z = clamp<double>(diff.Z + ((rnd & 32) ? 1 : -1), -maxdiff, maxdiff);
			}			
			AActor *thing = Spawn (spawnclass, pos + diff, ALLOW_REPLACE);
			if (thing)
				thing->Angles.Yaw = angle;
			pos += trail_step;
		}
	}
}

void P_DisconnectEffect (AActor *actor)
{
	int i;

	if (actor == NULL)
		return;

	for (i = 64; i; i--)
	{
		particle_t *p = JitterParticle (TICRATE*2);

		if (!p)
			break;

		double xo = (M_Random() - 128)*actor->radius / 128;
		double yo = (M_Random() - 128)*actor->radius / 128;
		double zo = M_Random()*actor->Height / 256;

		DVector3 pos = actor->Vec3Offset(xo, yo, zo);
		p->Pos = pos;
		p->Acc.Z -= 1./4096;
		p->color = M_Random() < 128 ? maroon1 : maroon2;
		p->size = 4;
	}
}
