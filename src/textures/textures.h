#ifndef __TEXTURES_H
#define __TEXTURES_H

#include "doomtype.h"
#include "vectors.h"

class FBitmap;
struct FRemapTable;
struct FCopyInfo;
class FScanner;
class PClassInventory;
class FArchive;

// Texture IDs
class FTextureManager;
class FTerrainTypeArray;

class FNullTextureID : public FTextureID
{
public:
	FNullTextureID() : FTextureID(0) {}
};

FArchive &operator<< (FArchive &arc, FTextureID &tex);

//
// Animating textures and planes
//
// [RH] Expanded to work with a Hexen ANIMDEFS lump
//

struct FAnimDef
{
	FTextureID 	BasePic;
	WORD	NumFrames;
	WORD	CurFrame;
	BYTE	AnimType;
	bool	bDiscrete;			// taken out of AnimType to have better control
	DWORD	SwitchTime;			// Time to advance to next frame
	struct FAnimFrame
	{
		DWORD	SpeedMin;		// Speeds are in ms, not tics
		DWORD	SpeedRange;
		FTextureID	FramePic;
	} Frames[1];
	enum
	{
		ANIM_Forward,
		ANIM_Backward,
		ANIM_OscillateUp,
		ANIM_OscillateDown,
		ANIM_Random
	};

	void SetSwitchTime (DWORD mstime);
};

struct FSwitchDef
{
	FTextureID PreTexture;		// texture to switch from
	FSwitchDef *PairDef;		// switch def to use to return to PreTexture
	WORD NumFrames;		// # of animation frames
	bool QuestPanel;	// Special texture for Strife mission
	int Sound;			// sound to play at start of animation. Changed to int to avoiud having to include s_sound here.
	struct frame		// Array of times followed by array of textures
	{					//   actual length of each array is <NumFrames>
		WORD TimeMin;
		WORD TimeRnd;
		FTextureID Texture;
	} frames[1];
};

struct FDoorAnimation
{
	FTextureID BaseTexture;
	FTextureID *TextureFrames;
	int NumTextureFrames;
	FName OpenSound;
	FName CloseSound;
};

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures, and we compose
// textures from the TEXTURE1/2 lists of patches.
struct patch_t
{ 
	SWORD			width;			// bounding box size 
	SWORD			height; 
	SWORD			leftoffset; 	// pixels to the left of origin 
	SWORD			topoffset;		// pixels below the origin 
	DWORD 			columnofs[];	// only [width] used
	// the [0] is &columnofs[width] 
};

class FileReader;

// All FTextures present their data to the world in 8-bit format, but if
// the source data is something else, this is it.
enum FTextureFormat
{
	TEX_Pal,
	TEX_Gray,
	TEX_RGB,		// Actually ARGB
	TEX_DXT1,
	TEX_DXT2,
	TEX_DXT3,
	TEX_DXT4,
	TEX_DXT5,
};

class FNativeTexture;

// Base texture class
class FTexture
{
public:
	static FTexture *CreateTexture(const char *name, int lumpnum, int usetype);
	static FTexture *CreateTexture(int lumpnum, int usetype);
	virtual ~FTexture ();

	SWORD LeftOffset, TopOffset;

	BYTE WidthBits, HeightBits;

	DVector2 Scale;

	int SourceLump;
	FTextureID id;

	FString Name;
	BYTE UseType;	// This texture's primary purpose

	BYTE bNoDecals:1;		// Decals should not stick to texture
	BYTE bNoRemap0:1;		// Do not remap color 0 (used by front layer of parallax skies)
	BYTE bWorldPanning:1;	// Texture is panned in world units rather than texels
	BYTE bMasked:1;			// Texture (might) have holes
	BYTE bAlphaTexture:1;	// Texture is an alpha channel without color information
	BYTE bHasCanvas:1;		// Texture is based off FCanvasTexture
	BYTE bWarped:2;			// This is a warped texture. Used to avoid multiple warps on one texture
	BYTE bComplex:1;		// Will be used to mark extended MultipatchTextures that have to be
							// fully composited before subjected to any kind of postprocessing instead of
							// doing it per patch.
	BYTE bMultiPatch:1;		// This is a multipatch texture (we really could use real type info for textures...)
	BYTE bKeepAround:1;		// This texture was used as part of a multi-patch texture. Do not free it.

	WORD Rotations;
	SWORD SkyOffset;

	enum // UseTypes
	{
		TEX_Any,
		TEX_Wall,
		TEX_Flat,
		TEX_Sprite,
		TEX_WallPatch,
		TEX_Build,
		TEX_SkinSprite,
		TEX_Decal,
		TEX_MiscPatch,
		TEX_FontChar,
		TEX_Override,	// For patches between TX_START/TX_END
		TEX_Autopage,	// Automap background - used to enable the use of FAutomapTexture
		TEX_SkinGraphic,
		TEX_Null,
		TEX_FirstDefined,
	};

	struct Span
	{
		WORD TopOffset;
		WORD Length;	// A length of 0 terminates this column
	};

	// Returns a single column of the texture
	virtual const BYTE *GetColumn (unsigned int column, const Span **spans_out) = 0;

	// Returns the whole texture, stored in column-major order
	virtual const BYTE *GetPixels () = 0;
	
	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate=0, FCopyInfo *inf = NULL);
	int CopyTrueColorTranslated(FBitmap *bmp, int x, int y, int rotate, FRemapTable *remap, FCopyInfo *inf = NULL);
	virtual bool UseBasePalette();
	virtual int GetSourceLump() { return SourceLump; }
	virtual FTexture *GetRedirect(bool wantwarped);
	virtual FTexture *GetRawTexture();		// for FMultiPatchTexture to override

	virtual void Unload () = 0;

	// Returns the native pixel format for this image
	virtual FTextureFormat GetFormat();

	// Returns a native 3D representation of the texture
	FNativeTexture *GetNative(bool wrapping);

	// Frees the native 3D representation of the texture
	void KillNative();

	// Fill the native texture buffer with pixel data for this image
	virtual void FillBuffer(BYTE *buff, int pitch, int height, FTextureFormat fmt);

	int GetWidth () { return Width; }
	int GetHeight () { return Height; }

	int GetScaledWidth () { int foo = int((Width * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledHeight () { int foo = int((Height * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetScaledWidthDouble () { return Width / Scale.X; }
	double GetScaledHeightDouble () { return Height / Scale.Y; }
	double GetScaleY() const { return Scale.Y; }

	int GetScaledLeftOffset () { int foo = int((LeftOffset * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledTopOffset () { int foo = int((TopOffset * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetScaledLeftOffsetDouble() { return LeftOffset / Scale.X; }
	double GetScaledTopOffsetDouble() { return TopOffset / Scale.Y; }

	virtual void SetFrontSkyLayer();

	void CopyToBlock (BYTE *dest, int dwidth, int dheight, int x, int y, const BYTE *translation=NULL)
	{
		CopyToBlock(dest, dwidth, dheight, x, y, 0, translation);
	}

	void CopyToBlock (BYTE *dest, int dwidth, int dheight, int x, int y, int rotate, const BYTE *translation=NULL);

	// Returns true if the next call to GetPixels() will return an image different from the
	// last call to GetPixels(). This should be considered valid only if a call to CheckModified()
	// is immediately followed by a call to GetPixels().
	virtual bool CheckModified ();

	static void InitGrayMap();

	void CopySize(FTexture *BaseTexture)
	{
		Width = BaseTexture->GetWidth();
		Height = BaseTexture->GetHeight();
		TopOffset = BaseTexture->TopOffset;
		LeftOffset = BaseTexture->LeftOffset;
		WidthBits = BaseTexture->WidthBits;
		HeightBits = BaseTexture->HeightBits;
		Scale = BaseTexture->Scale;
		WidthMask = (1 << WidthBits) - 1;
	}

	void SetScaledSize(int fitwidth, int fitheight);

	virtual void HackHack (int newheight);	// called by FMultipatchTexture to discover corrupt patches.

protected:
	WORD Width, Height, WidthMask;
	static BYTE GrayMap[256];
	FNativeTexture *Native;

	FTexture (const char *name = NULL, int lumpnum = -1);

	Span **CreateSpans (const BYTE *pixels) const;
	void FreeSpans (Span **spans) const;
	void CalcBitSize ();
	void CopyInfo(FTexture *other)
	{
		CopySize(other);
		bNoDecals = other->bNoDecals;
		Rotations = other->Rotations;
	}

public:
	static void FlipSquareBlock (BYTE *block, int x, int y);
	static void FlipSquareBlockRemap (BYTE *block, int x, int y, const BYTE *remap);
	static void FlipNonSquareBlock (BYTE *blockto, const BYTE *blockfrom, int x, int y, int srcpitch);
	static void FlipNonSquareBlockRemap (BYTE *blockto, const BYTE *blockfrom, int x, int y, int srcpitch, const BYTE *remap);

	friend class D3DTex;
};


// Texture manager
class FTextureManager
{
public:
	FTextureManager ();
	~FTextureManager ();

	// Get texture without translation
	FTexture *operator[] (FTextureID texnum)
	{
		if ((unsigned)texnum.GetIndex() >= Textures.Size()) return NULL;
		return Textures[texnum.GetIndex()].Texture;
	}
	FTexture *operator[] (const char *texname)
	{
		FTextureID texnum = GetTexture (texname, FTexture::TEX_MiscPatch);
		if (!texnum.Exists()) return NULL;
		return Textures[texnum.GetIndex()].Texture;
	}
	FTexture *ByIndex(int i)
	{
		if (unsigned(i) >= Textures.Size()) return NULL;
		return Textures[i].Texture;
	}
	FTexture *FindTexture(const char *texname, int usetype = FTexture::TEX_MiscPatch, BITFIELD flags = TEXMAN_TryAny);

	// Get texture with translation
	FTexture *operator() (FTextureID texnum, bool withpalcheck=false)
	{
		if ((size_t)texnum.texnum >= Textures.Size()) return NULL;
		int picnum = Translation[texnum.texnum];
		if (withpalcheck)
		{
			picnum = PalCheck(picnum).GetIndex();
		}
		return Textures[picnum].Texture;
	}
	FTexture *operator() (const char *texname)
	{
		FTextureID texnum = GetTexture (texname, FTexture::TEX_MiscPatch);
		if (texnum.texnum == -1) return NULL;
		return Textures[Translation[texnum.texnum]].Texture;
	}

	FTexture *ByIndexTranslated(int i)
	{
		if (unsigned(i) >= Textures.Size()) return NULL;
		return Textures[Translation[i]].Texture;
	}

	FTextureID PalCheck(FTextureID tex);

	enum
	{
		TEXMAN_TryAny = 1,
		TEXMAN_Overridable = 2,
		TEXMAN_ReturnFirst = 4,
		TEXMAN_AllowSkins = 8,
		TEXMAN_ShortNameOnly = 16,
		TEXMAN_DontCreate = 32
	};

	enum
	{
		HIT_Wall = 1,
		HIT_Flat = 2,
		HIT_Sky = 4,
		HIT_Sprite = 8,

		HIT_Columnmode = HIT_Wall|HIT_Sky|HIT_Sprite
	};

	FTextureID CheckForTexture (const char *name, int usetype, BITFIELD flags=TEXMAN_TryAny);
	FTextureID GetTexture (const char *name, int usetype, BITFIELD flags=0);
	int ListTextures (const char *name, TArray<FTextureID> &list);

	void AddTexturesLump (const void *lumpdata, int lumpsize, int deflumpnum, int patcheslump, int firstdup=0, bool texture1=false);
	void AddTexturesLumps (int lump1, int lump2, int patcheslump);
	void AddGroup(int wadnum, int ns, int usetype);
	void AddPatches (int lumpnum);
	void AddHiresTextures (int wadnum);
	void LoadTextureDefs(int wadnum, const char *lumpname);
	void ParseXTexture(FScanner &sc, int usetype);
	void SortTexturesByType(int start, int end);
	bool AreTexturesCompatible (FTextureID picnum1, FTextureID picnum2);

	FTextureID CreateTexture (int lumpnum, int usetype=FTexture::TEX_Any);	// Also calls AddTexture
	FTextureID AddTexture (FTexture *texture);
	FTextureID GetDefaultTexture() const { return DefaultTexture; }

	void LoadTextureX(int wadnum);
	void AddTexturesForWad(int wadnum);
	void Init();
	void DeleteAll();

	// Replaces one texture with another. The new texture will be assigned
	// the same name, slot, and use type as the texture it is replacing.
	// The old texture will no longer be managed. Set free true if you want
	// the old texture to be deleted or set it false if you want it to
	// be left alone in memory. You will still need to delete it at some
	// point, because the texture manager no longer knows about it.
	// This function can be used for such things as warping textures.
	void ReplaceTexture (FTextureID picnum, FTexture *newtexture, bool free);

	void UnloadAll ();

	int NumTextures () const { return (int)Textures.Size(); }
	void PrecacheLevel (void);

	void WriteTexture (FArchive &arc, int picnum);
	int ReadTexture (FArchive &arc);

	void UpdateAnimations (DWORD mstime);
	int GuesstimateNumTextures ();

	FSwitchDef *FindSwitch (FTextureID texture);
	FDoorAnimation *FindAnimatedDoor (FTextureID picnum);

private:

	// texture counting
	int CountTexturesX ();
	int CountLumpTextures (int lumpnum);

	// Build tiles
	void AddTiles (void *tiles);
	int CountTiles (void *tiles);
	int CountBuildTiles ();
	void InitBuildTiles ();

	// Animation stuff
	FAnimDef *AddAnim (FAnimDef *anim);
	void FixAnimations ();
	void InitAnimated ();
	void InitAnimDefs ();
	FAnimDef *AddSimpleAnim (FTextureID picnum, int animcount, DWORD speedmin, DWORD speedrange=0);
	FAnimDef *AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames);
	void ParseAnim (FScanner &sc, int usetype);
	FAnimDef *ParseRangeAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing);
	void ParsePicAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames);
	void ParseWarp(FScanner &sc);
	void ParseCameraTexture(FScanner &sc);
	FTextureID ParseFramenum (FScanner &sc, FTextureID basepicnum, int usetype, bool allowMissing);
	void ParseTime (FScanner &sc, DWORD &min, DWORD &max);
	FTexture *Texture(FTextureID id) { return Textures[id.GetIndex()].Texture; }
	void SetTranslation (FTextureID fromtexnum, FTextureID totexnum);
	void ParseAnimatedDoor(FScanner &sc);

	void InitPalettedVersions();

	// Switches

	void InitSwitchList ();
	void ProcessSwitchDef (FScanner &sc);
	FSwitchDef *ParseSwitchDef (FScanner &sc, bool ignoreBad);
	void AddSwitchPair (FSwitchDef *def1, FSwitchDef *def2);

	struct TextureHash
	{
		FTexture *Texture;
		int HashNext;
	};
	enum { HASH_END = -1, HASH_SIZE = 1027 };
	TArray<TextureHash> Textures;
	TArray<int> Translation;
	int HashFirst[HASH_SIZE];
	FTextureID DefaultTexture;
	TArray<int> FirstTextureForFile;
	TMap<int,int> PalettedVersions;		// maps from normal -> paletted version

	TArray<FAnimDef *> mAnimations;
	TArray<FSwitchDef *> mSwitchDefs;
	TArray<FDoorAnimation> mAnimatedDoors;
	TArray<BYTE *> BuildTileFiles;
};

// A texture that doesn't really exist
class FDummyTexture : public FTexture
{
public:
	FDummyTexture ();
	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	void SetSize (int width, int height);
};

// A texture that returns a wiggly version of another texture.
class FWarpTexture : public FTexture
{
public:
	FWarpTexture (FTexture *source);
	~FWarpTexture ();

	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate=0, FCopyInfo *inf = NULL);
	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	bool CheckModified ();

	float GetSpeed() const { return Speed; }
	int GetSourceLump() { return SourcePic->GetSourceLump(); }
	void SetSpeed(float fac) { Speed = fac; }
	FTexture *GetRedirect(bool wantwarped);

	DWORD GenTime;
protected:
	FTexture *SourcePic;
	BYTE *Pixels;
	Span **Spans;
	float Speed;
	int WidthOffsetMultipiler, HeightOffsetMultipiler;  // [mxd]

	virtual void MakeTexture (DWORD time);
	int NextPo2 (int v); // [mxd]
	void SetupMultipliers (int width, int height); // [mxd]
};

// [GRB] Eternity-like warping
class FWarp2Texture : public FWarpTexture
{
public:
	FWarp2Texture (FTexture *source);

protected:
	void MakeTexture (DWORD time);
};

// A texture that can be drawn to.
class DSimpleCanvas;
class AActor;
class FArchive;

class FCanvasTexture : public FTexture
{
public:
	FCanvasTexture (const char *name, int width, int height);
	~FCanvasTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	bool CheckModified ();
	void NeedUpdate() { bNeedsUpdate=true; }
	void SetUpdated() { bNeedsUpdate = false; bDidUpdate = true; bFirstUpdate = false; }
	DSimpleCanvas *GetCanvas() { return Canvas; }
	void MakeTexture ();

protected:

	DSimpleCanvas *Canvas;
	BYTE *Pixels;
	Span DummySpans[2];
	bool bNeedsUpdate;
	bool bDidUpdate;
	bool bPixelsAllocated;
public:
	bool bFirstUpdate;


	friend struct FCanvasTextureInfo;
};

extern FTextureManager TexMan;

#endif


