#ifndef HLMOV_H
#define HLMOV_H

#include "hlmov_bridge.h"
#include <math.h>

#ifdef __cplusplus
#define this thisx
extern "C"
{
#endif

#define	MAX_PHYSENTS 600 		  // Must have room for all entities in the world.
#define MAX_MOVEENTS 64
#define	MAX_CLIP_PLANES	5

#define PM_NORMAL			0x00000000
#define PM_STUDIO_IGNORE	0x00000001		// Skip studio models
#define PM_STUDIO_BOX		0x00000002		// Use boxes for non-complex studio models (even in traceline)
#define PM_GLASS_IGNORE		0x00000004		// Ignore entities with non-normal rendermode
#define PM_WORLD_ONLY		0x00000008		// Only trace against the world

// Values for flags parameter of PM_TraceLine
#define PM_TRACELINE_ANYVISIBLE		0
#define PM_TRACELINE_PHYSENTSONLY	1

#define MAX_PHYSINFO_STRING 256

#define	FL_FLY					(1<<0)	// Changes the SV_Movestep() behavior to not need to be on ground
#define	FL_SWIM					(1<<1)	// Changes the SV_Movestep() behavior to not need to be on ground (but stay in water)
#define	FL_CONVEYOR				(1<<2)
#define	FL_CLIENT				(1<<3)
#define	FL_INWATER				(1<<4)
#define	FL_MONSTER				(1<<5)
#define	FL_GODMODE				(1<<6)
#define	FL_NOTARGET				(1<<7)
#define	FL_SKIPLOCALHOST		(1<<8)	// Don't send entity to local host, it's predicting this entity itself
#define	FL_ONGROUND				(1<<9)	// At rest / on the ground
#define	FL_PARTIALGROUND		(1<<10)	// not all corners are valid
#define	FL_WATERJUMP			(1<<11)	// player jumping out of water
#define FL_FROZEN				(1<<12) // Player is frozen for 3rd person camera
#define FL_FAKECLIENT			(1<<13)	// JAC: fake client, simulated server side; don't send network messages to them
#define FL_DUCKING				(1<<14)	// Player flag -- Player is fully crouched
#define FL_FLOAT				(1<<15)	// Apply floating force to this entity when in water
#define FL_GRAPHED				(1<<16) // worldgraph has this ent listed as something that blocks a connection

// edict->movetype values
#define	MOVETYPE_NONE			0		// never moves
//#define	MOVETYPE_ANGLENOCLIP	1
//#define	MOVETYPE_ANGLECLIP		2
#define	MOVETYPE_WALK			3		// Player only - moving on the ground
#define	MOVETYPE_STEP			4		// gravity, special edge handling -- monsters use this
#define	MOVETYPE_FLY			5		// No gravity, but still collides with stuff
#define	MOVETYPE_TOSS			6		// gravity/collisions
#define	MOVETYPE_PUSH			7		// no clip to world, push and crush
#define	MOVETYPE_NOCLIP			8		// No gravity, no collisions, still do velocity/avelocity
#define	MOVETYPE_FLYMISSILE		9		// extra size to monsters
#define	MOVETYPE_BOUNCE			10		// Just like Toss, but reflect velocity when contacting surfaces
#define MOVETYPE_BOUNCEMISSILE	11		// bounce w/o gravity
#define MOVETYPE_FOLLOW			12		// track movement of aiment
#define	MOVETYPE_PUSHSTEP		13		// BSP model that needs physics/world collisions (uses nearest hull for world collision)

//#define FL_ARCHIVE_OVERRIDE		(1<<20)	// NOT USED
#define FL_ALWAYSTHINK			(1<<21)	// Brush model flag -- call think every frame regardless of nextthink - ltime (for constantly changing velocity/path)
#define FL_BASEVELOCITY			(1<<22)	// Base velocity has been applied this frame (used to convert base velocity into momentum)
#define FL_MONSTERCLIP			(1<<23)	// Only collide in with monsters who have FL_MONSTERCLIP set
#define FL_ONTRAIN				(1<<24) // Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_WORLDBRUSH			(1<<25)	// Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
#define FL_SPECTATOR            (1<<26) // This client is a spectator, don't run touch functions, etc.
#define FL_CUSTOMENTITY			(1<<29)	// This is a custom entity
#define FL_KILLME				(1<<30)	// This entity is marked for death -- This allows the engine to kill ents at the appropriate time
#define FL_DORMANT				(1<<31)	// Entity is dormant, no updates to client

#define	CONTENTS_EMPTY		-1
#define	CONTENTS_SOLID		-2
#define	CONTENTS_WATER		-3
#define	CONTENTS_SLIME		-4
#define	CONTENTS_LAVA		-5
#define	CONTENTS_SKY		-6

#define	CONTENTS_CURRENT_0		-9
#define	CONTENTS_CURRENT_90		-10
#define	CONTENTS_CURRENT_180	-11
#define	CONTENTS_CURRENT_270	-12
#define	CONTENTS_CURRENT_UP		-13
#define	CONTENTS_CURRENT_DOWN	-14

#define CONTENTS_TRANSLUCENT	-15

#define TIME_TO_DUCK	0.4
#define VEC_DUCK_HULL_MIN	-18
#define VEC_DUCK_HULL_MAX	18
#define VEC_DUCK_VIEW		12
#define PM_DEAD_VIEWHEIGHT	-8
#define MAX_CLIMB_SPEED	200
#define STUCK_MOVEUP 1
#define STUCK_MOVEDOWN -1
#define VEC_HULL_MIN		-36
#define VEC_HULL_MAX		36
#define VEC_VIEW			28
#define	STOP_EPSILON	0.1

#define	PLAYER_WALLJUMP_SPEED 300 // how fast we can spring off walls
#define PLAYER_LONGJUMP_SPEED 350 // how fast we longjump

#ifndef M_PI
#define M_PI    (double)3.14159265358979323846
#endif

#define M_PI2   ((double)(M_PI * 2))
#define M_PI_F  ((float)(M_PI))
#define M_PI2_F ((float)(M_PI2))

#define RAD2DEG( x ) ((double)(x) * (double)(180.0 / M_PI))
#define DEG2RAD( x ) ((double)(x) * (double)(M_PI / 180.0))

#define DotProduct(a,b) ((a).x*(b).x+(a).y*(b).y+(a).z*(b).z)
#define VectorSubtract(a,b,c) {(c).x=(a).x-(b).x;(c).y=(a).y-(b).y;(c).z=(a).z-(b).z;}
#define VectorAdd(a,b,c) {(c).x=(a).x+(b).x;(c).y=(a).y+(b).y;(c).z=(a).z+(b).z;}
#define VectorCopy(a,b) {(b).x=(a).x;(b).y=(a).y;(b).z=(a).z;}
#define VectorScale(in, scale, out) ((out).x = (in).x * (scale),(out).y = (in).y * (scale),(out).z = (in).z * (scale))
#define VectorClear(a) { a.x=0.0;a.y=0.0;a.z=0.0;}
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

extern int nanmask;
#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

// up / down
#define	PITCH	x
// left / right
#define	YAW		y
// fall over
#define	ROLL	z 

typedef struct
{
	vec3_t	normal;
	float	dist;
} plane_t;

typedef struct
{
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float	fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t	endpos;			// final position
	plane_t	plane;			// surface normal at impact
	/// edict_t	*ent;			// entity the surface is on
	int		hitgroup;		// 0 == generic, non zero is specific body part
} trace_t;

typedef struct
{
	vec3_t	normal;
	float	dist;
} pmplane_t;

typedef struct pmtrace_s pmtrace_t;

struct pmtrace_s
{
	qboolean	allsolid;	      // if true, plane is not valid
	qboolean	startsolid;	      // if true, the initial point was in a solid area
	qboolean	inopen, inwater;  // End point is in empty space or in water
	float		fraction;		  // time completed, 1.0 = didn't hit anything
	vec3_t		endpos;			  // final position
	pmplane_t	plane;		      // surface normal at impact
	int			ent;			  // entity at impact
	vec3_t      deltavelocity;    // Change in player's velocity caused by impact.  
								  // Only run on server.
	int         hitgroup;
};

typedef struct physent_s physent_t;

typedef struct physent_s
{
	char			name[32];             // Name of model, or "player" or "world".
	int				player;
	vec3_t			origin;               // Model's origin in world coordinates.
	struct model_s	*model;		          // only for bsp models
	struct model_s	*studiomodel;         // SOLID_BBOX, but studio clip intersections.
	vec3_t			mins, maxs;	          // only for non-bsp models
	int				info;		          // For client or server to use to identify (index into edicts or cl_entities)
	vec3_t			angles;               // rotated entities need this info for hull testing to work.

	int				solid;				  // Triggers and func_door type WATER brushes are SOLID_NOT
	int				skin;                 // BSP Contents for such things like fun_door water brushes.
	int				rendermode;			  // So we can ignore glass
	
	// Complex collision detection.
	float			frame;
	int				sequence;
	byte			controller[4];
	byte			blending[2];

	int				movetype;
	int				takedamage;
	int				blooddecal;
	int				team;
	int				classnumber;

	// For mods
	int				iuser1;
	int				iuser2;
	int				iuser3;
	int				iuser4;
	float			fuser1;
	float			fuser2;
	float			fuser3;
	float			fuser4;
	vec3_t			vuser1;
	vec3_t			vuser2;
	vec3_t			vuser3;
	vec3_t			vuser4;
} physent_t;

typedef struct movevars_s movevars_t;

struct movevars_s
{
	float	gravity;           // Gravity for map
	float	stopspeed;         // Deceleration when not moving
	float	maxspeed;          // Max allowed speed
	float	spectatormaxspeed;
	float	accelerate;        // Acceleration factor
	float	airaccelerate;     // Same for when in open air
	float	wateraccelerate;   // Same for when in water
	float	friction;          
	float   edgefriction;	   // Extra friction near dropofs 
	float	waterfriction;     // Less in water
	float	entgravity;        // 1.0
	float   bounce;            // Wall bounce value. 1.0
	float   stepsize;          // sv_stepsize;
	float   maxvelocity;       // maximum server velocity.
	float	zmax;			   // Max z-buffer range (for GL)
	float	waveHeight;		   // Water wave height (for GL)
	qboolean footsteps;        // Play footstep sounds
	char	skyName[32];	   // Name of the sky map
	float	rollangle;
	float	rollspeed;
	float	skycolor_r;			// Sky color
	float	skycolor_g;			// 
	float	skycolor_b;			//
	float	skyvec_x;			// Sky vector
	float	skyvec_y;			// 
	float	skyvec_z;			// 
};

typedef struct playermove_s playermove_t;

struct playermove_s
{
	int				player_index;  // So we don't try to run the PM_CheckStuck nudging too quickly.
	qboolean		server;        // For debugging, are we running physics code on server side?

	qboolean		multiplayer;   // 1 == multiplayer server
	float			time;          // realtime on host, for reckoning duck timing
	float			frametime;	   // Duration of this frame

	vec3_t			forward, right, up; // Vectors for angles
	// player state
	vec3_t			origin;        // Movement origin.
	vec3_t			angles;        // Movement view angles.
	vec3_t			oldangles;     // Angles before movement view angles were looked at.
	vec3_t			velocity;      // Current movement direction.
	vec3_t			movedir;       // For waterjumping, a forced forward velocity so we can fly over lip of ledge.
	vec3_t			basevelocity;  // Velocity of the conveyor we are standing, e.g.
	
	// For ducking/dead
	vec3_t			view_ofs;      // Our eye position.
	float			flDuckTime;    // Time we started duck
	qboolean		bInDuck;       // In process of ducking or ducked already?
	
	// For walking/falling
	int				flTimeStepSound;  // Next time we can play a step sound
	int				iStepLeft;

	float			flFallVelocity;
	vec3_t			punchangle;

	float			flSwimTime;

	float			flNextPrimaryAttack;

	int				effects;		// MUZZLE FLASH, e.g.

	int				flags;         // FL_ONGROUND, FL_DUCKING, etc.
	int				usehull;       // 0 = regular player hull, 1 = ducked player hull, 2 = point hull
	float			gravity;       // Our current gravity and friction.
	float			friction;
	int				oldbuttons;    // Buttons last usercmd
	float			waterjumptime; // Amount of time left in jumping out of water cycle.
	qboolean		dead;          // Are we a dead player?
	int				deadflag;
	int				spectator;     // Should we use spectator physics model?
	int				movetype;      // Our movement type, NOCLIP, WALK, FLY

	int				onground;
	int				waterlevel;
	int				watertype;
	int				oldwaterlevel;

	char			sztexturename[256];
	char			chtexturetype;

	float			maxspeed;
	float			clientmaxspeed; // Player specific maxspeed

	// For mods
	int				iuser1;
	int				iuser2;
	int				iuser3;
	int				iuser4;
	float			fuser1;
	float			fuser2;
	float			fuser3;
	float			fuser4;
	vec3_t			vuser1;
	vec3_t			vuser2;
	vec3_t			vuser3;
	vec3_t			vuser4;
	// world state
	// Number of entities to clip against.
	int				numphysent;    
	physent_t		physents[MAX_PHYSENTS];
	// Number of momvement entities (ladders)
	int				nummoveent;
	// just a list of ladders
	physent_t		moveents[MAX_MOVEENTS];	

	// All things being rendered, for tracing against things you don't actually collide with
	int				numvisent;
	physent_t		visents[ MAX_PHYSENTS ];

	// input to run through physics.
	usercmd_t		cmd;

	// Trace results for objects we collided with.
	int				numtouch;
	pmtrace_t		touchindex[MAX_PHYSENTS];

	char			physinfo[ MAX_PHYSINFO_STRING ]; // Physics info string

	struct movevars_s *movevars;
	vec3_t player_mins[ 4 ];
	vec3_t player_maxs[ 4 ];
	
	// Common functions
	const char		*(*PM_Info_ValueForKey) ( const char *s, const char *key );
	void			(*PM_Particle)( vec3_t origin, int color, float life, int zpos, int zvel);
	int				(*PM_TestPlayerPosition) (vec3_t pos, pmtrace_t *ptrace );
	void			(*Con_NPrintf)( int idx, char *fmt, ... );
	void			(*Con_DPrintf)( char *fmt, ... );
	void			(*Con_Printf)( char *fmt, ... );
	double			(*Sys_FloatTime)( void );
	void			(*PM_StuckTouch)( int hitent, pmtrace_t *ptraceresult );
	int				(*PM_PointContents) (vec3_t p, int *truecontents /*filled in if this is non-null*/ );
	int				(*PM_TruePointContents) (vec3_t p);
	int				(*PM_HullPointContents) ( struct hull_s *hull, int num, vec3_t p);   
	pmtrace_t		(*PM_PlayerTrace) (vec3_t start, vec3_t end, int traceFlags, int ignore_pe );
	struct pmtrace_s *(*PM_TraceLine)( float *start, float *end, int flags, int usehull, int ignore_pe );
	long			(*RandomLong)( long lLow, long lHigh );
	float			(*RandomFloat)( float flLow, float flHigh );
	int				(*PM_GetModelType)( struct model_s *mod );
	void			(*PM_GetModelBounds)( struct model_s *mod, vec3_t mins, vec3_t maxs );
	void			*(*PM_HullForBsp)( physent_t *pe, vec3_t offset );
	float			(*PM_TraceModel)( physent_t *pEnt, vec3_t start, vec3_t end, trace_t *trace );
	int				(*COM_FileSize)(char *filename);
	byte			*(*COM_LoadFile) (char *path, int usehunk, int *pLength);
	void			(*COM_FreeFile) ( void *buffer );
	char			*(*memfgets)( byte *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufferSize );

	// Functions
	// Run functions for this frame?
	qboolean		runfuncs;      
	void			(*PM_PlaySound) ( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch );
	const char		*(*PM_TraceTexture) ( int ground, vec3_t vstart, vec3_t vend );
	void			(*PM_PlaybackEventFull) ( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
};

#ifdef __cplusplus
#undef this
};
#undef this
#endif

#endif // HLMOV_H