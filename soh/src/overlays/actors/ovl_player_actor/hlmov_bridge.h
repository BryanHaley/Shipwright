#pragma once

#include "z64.h"

#ifdef __cplusplus
#define this thisx
extern "C"
{
#endif

// Z64 -> HL types
typedef Vec3f vec3_t;
typedef int qboolean;
typedef int byte;

extern struct playermove_s *pmove;

void PM_Init ();
void PM_Move ( struct playermove_s *ppmove, int server, PlayState* playState, vec3_t playerPos );
char PM_FindTextureType( char *name ); // Might not need this

#ifdef __cplusplus
#undef this
};
#undef this
#endif