//===========================================================================
// DD_DEF.H
//	Internal Doomsday definitions.
//===========================================================================
#ifndef __DOOMSDAY_DEFS_H__
#define __DOOMSDAY_DEFS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "dd_types.h"
#include "dd_api.h"

#ifdef WIN32
// Disable annoying MSVC warnings.
// 4761: integral size mismatch in argument 
// 4244: conversion from 'type1' to 'type2', possible loss of data
#pragma warning (disable:4761 4244)
#endif

// Important definitions.
#define MAXPLAYERS			DDMAXPLAYERS
#define players				ddplayers

// if rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#ifndef NORANGECHECKING
#	define RANGECHECK
#endif

#ifndef DOOMSDAY_VER_ID
#	ifdef _DEBUG
#		define DOOMSDAY_VER_ID "+D DGL"
#	else
#		define DOOMSDAY_VER_ID "DGL"
#	endif
#endif

#ifdef RANGECHECK
#	define DOOMSDAY_VERSIONTEXT "Version "DOOMSDAY_VERSION_TEXT" +R "__DATE__" ("DOOMSDAY_VER_ID")"
#else
#	define DOOMSDAY_VERSIONTEXT "Version "DOOMSDAY_VERSION_TEXT" "__DATE__" ("DOOMSDAY_VER_ID")"
#endif

#define SAFEDIV(x,y)	(!(y) || !((x)/(y))? 1 : (x)/(y))
#define ORDER(x,y,a,b)	( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

#define MAXEVENTS		64
#define PI				3.141592657
#define	SBARHEIGHT		39			// status bar height at bottom of screen

// Heap relations.
#define HEAP_PARENT(i)	(((i) + 1)/2 - 1)
#define HEAP_LEFT(i)	(2*(i) + 1)
#define HEAP_RIGHT(i)	(2*(i) + 2)

enum { BLEFT, BTOP, BRIGHT, BBOTTOM, BFLOOR, BCEILING };
enum { VX, VY, VZ };				// Vertex indices.
enum { CR, CG, CB, CA };			// Color indices.

// dd_pinit.c
extern game_export_t __gx;
extern game_import_t __gi;
#define gx __gx
#define gi __gi

// tab_video.c
extern byte gammatable[5][256];
extern int usegamma;

// tab_tables.c
extern fixed_t finesine[5*FINEANGLES/4];
extern fixed_t *finecosine;

#endif
