//===========================================================================
// DD_TYPES.H
//===========================================================================
#ifndef __DOOMSDAY_TYPES_H__
#define __DOOMSDAY_TYPES_H__

// The C_DECL macro, used with functions.
#ifndef C_DECL
#	if defined(WIN32)
#		define C_DECL __cdecl
#	elif defined(UNIX)
#		define C_DECL
#	endif
#endif

#ifndef UNIX
typedef unsigned int		uint;
typedef unsigned short		ushort;
typedef unsigned int		size_t;
#endif

typedef int					spritenum_t;
typedef unsigned int		ident_t;
typedef unsigned short		nodeindex_t;
typedef unsigned short		thid_t;
typedef unsigned char		byte;
typedef char				filename_t[256];

typedef struct directory_s {
	int drive;
	filename_t path;
} directory_t;

#ifdef __cplusplus
#	define boolean			int
#else // Plain C.
#	ifndef __BYTEBOOL__
#		define __BYTEBOOL__
#	endif
	typedef enum ddboolean_e { false, true } ddboolean_t;
#	define boolean			ddboolean_t
#endif

#define BAMS_BITS	16

#if BAMS_BITS == 32
	typedef unsigned long	binangle_t;
#elif BAMS_BITS == 16
	typedef unsigned short	binangle_t;
#else
	typedef unsigned char	binangle_t;
#endif

#define DDMAXCHAR	((char)0x7f)
#define DDMAXSHORT	((short)0x7fff)
#define DDMAXINT	((int)0x7fffffff)	// max pos 32-bit int
#define DDMAXLONG	((long)0x7fffffff)

#define DDMINCHAR	((char)0x80)
#define DDMINSHORT	((short)0x8000)
#define DDMININT 	((int)0x80000000)	// max negative 32-bit integer
#define DDMINLONG	((long)0x80000000)

#endif 
