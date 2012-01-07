// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// TODO: reference additional headers your program requires here
#include <Ogre.h>    // Brings in almost everything

using namespace Ogre;

#include <CEGUI.h>
#include <CEGUISystem.h>
#include <CEGUISchemeManager.h>
#include <OgreCEGUIRenderer.h>

// Etm includes

#include "ETTerrainManager.h"
#include "ETTerrainInfo.h"
#include "ETBrush.h"
#include "ETSplattingManager.h"

// Particle Universe
#include "ParticleUniverseSystemManager.h"

// Include PagedGeometry headers that will be needed
#include "PagedGeometry.h"
#include "GrassLoader.h"
#include "BatchPage.h"
#include "ImpostorPage.h"
#include "TreeLoader3D.h"
#include "TreeLoader2D.h"

// PagedGeometry's classes and functions are under the "Forests" namespace
using namespace Forests;

// Include Caelum, our cloud and atmospheric rendering engine
#include "Caelum.h"

// Fmod
#include <fmod.hpp>
#include <fmod_errors.h>

// Application specific
//#include "BaseCommandFrameListener.h"
//#include "Vehicle.h"
#include "SoundManager.h"

// Include mysql lib
#include <mysql++.h>

extern ET::TerrainManager		*gTerrainMgr;
extern ET::SplattingManager		*gSplatMgr;

#define USE_PARTICLEUNIVERSE

#ifdef USE_PARTICLEUNIVERSE
extern ParticleUniverse::ParticleSystemManager	*gParticleSystemMgr;
extern ParticleUniverse::ParticleSystem			*gExplosionParticleSystem;
extern ParticleUniverse::ParticleSystem			*gFireParticleSystem;
#endif

extern Caelum::CaelumSystem						*gCaelumSystem;

extern PagedGeometry *gTrees, *gGrass, *gBushes;
extern TreeLoader3D *gTreeLoader;
extern GrassLoader *gGrassLoader;

extern LogManager *gLogMgr;

class Vehicle;

extern std::list< Vehicle *> gVehicles;

extern SoundManager *gSoundMgr;
extern int gExplosionSoundId;
extern int gAirRaidSoundId;

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif 