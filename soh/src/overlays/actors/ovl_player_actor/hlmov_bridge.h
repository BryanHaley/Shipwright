#pragma once

#include "z64.h"

#ifdef __cplusplus
#define this thisx
extern "C"
{
#endif

#define IN_ATTACK	(1 << 0)
#define IN_JUMP		(1 << 1)
#define IN_DUCK		(1 << 2)
#define IN_FORWARD	(1 << 3)
#define IN_BACK		(1 << 4)
#define IN_USE		(1 << 5)
#define IN_CANCEL	(1 << 6)
#define IN_LEFT		(1 << 7)
#define IN_RIGHT	(1 << 8)
#define IN_MOVELEFT	(1 << 9)
#define IN_MOVERIGHT (1 << 10)
#define IN_ATTACK2	(1 << 11)
#define IN_RUN      (1 << 12)
#define IN_RELOAD	(1 << 13)
#define IN_ALT1		(1 << 14)
#define IN_SCORE	(1 << 15)   // Used by client.dll for when scoreboard is held down

// Z64 -> HL types
typedef Vec3f vec3_t;
typedef int qboolean;
typedef int byte;
typedef struct { float m[3][3]; } mat3;

extern struct playermove_s *pmove;

typedef struct usercmd_s
{
	short	lerp_msec;      // Interpolation time on client
	byte	msec;           // Duration in ms of command
	vec3_t	viewangles;     // Command view angles.

// intended velocities
	float	forwardmove;    // Forward velocity.
	float	sidemove;       // Sideways velocity.
	float	upmove;         // Upward velocity.
	byte	lightlevel;     // Light level at spot where we are standing.
	unsigned short  buttons;  // Attack buttons
	byte    impulse;          // Impulse command issued.
	byte	weaponselect;	// Current weapon id

// Experimental player impact stuff.
	int		impact_index;
	vec3_t	impact_position;
} usercmd_t;

void PM_Init ();
vec3_t PM_Move ( struct playermove_s *ppmove, int server, PlayState* playState, vec3_t playerPos, usercmd_t cmd );
char PM_FindTextureType( char *name ); // Might not need this

vec3_t pos_goldsrc_to_oot(vec3_t pg);
void goldsrc_angle_vectors(float pitch_deg, float yaw_deg, float roll_deg,
                           vec3_t* fwd, vec3_t* right, vec3_t* up);
vec3_t v_goldsrc_to_oot(vec3_t vg);
mat3 oot_matrix_from_basis(vec3_t right, vec3_t up, vec3_t fwd);
void oot_euler_yxz_from_matrix(mat3 R, float* yaw_deg, float* pitch_deg, float* roll_deg);
void goldsrc_pose_to_oot(vec3_t pos_g,
                         float pitch_g_deg, float yaw_g_deg, float roll_g_deg,
                         vec3_t* pos_o, float* yaw_o_deg, float* pitch_o_deg, float* roll_o_deg);
int16_t oot_deg_to_s16(float degrees);
vec3_t pos_oot_to_goldsrc(vec3_t po);
mat3 oot_matrix_from_euler(float yaw_deg, float pitch_deg, float roll_deg);
vec3_t v_oot_to_goldsrc(vec3_t vo);
void goldsrc_angles_from_basis(vec3_t fwd, vec3_t right, vec3_t up,
                               float* pitch_deg, float* yaw_deg, float* roll_deg);
void oot_pose_to_goldsrc(vec3_t pos_o,
                         float yaw_o_deg, float pitch_o_deg, float roll_o_deg,
                         vec3_t* pos_g, float* pitch_g_deg, float* yaw_g_deg, float* roll_g_deg);

#ifdef __cplusplus
#undef this
};
#undef this
#endif