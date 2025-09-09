#include "hlmov.h"
#include "hlmov_bridge.h"

#include "functions.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct playermove_s pmove_s;
struct movevars_s movevars_singleton;

struct playermove_s *pmove = NULL;
struct movevars_s *movevars = NULL;

// TEMP
PlayState* play = NULL;
Player *player = NULL;

int g_onladder = 0;

vec3_t vec3_origin = {0,0,0};
int nanmask = 255<<23;

static void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross.x = v1.y*v2.z - v1.z*v2.y;
	cross.y = v1.z*v2.x - v1.x*v2.z;
	cross.z = v1.x*v2.y - v1.y*v2.x;
}

static void SinCos( float radians, float *sine, float *cosine )
{
	*sine = sin( radians );
	*cosine = cos( radians );
}

float Length(vec3_t v)
{
	int		i;
	float	length;
	
	length = 0;
	length += v.x*v.x;
    length += v.y*v.y;
    length += v.z*v.z;
	length = sqrt (length);		// FIXME

	return length;
}

static void AngleVectors( const vec3_t angles, vec3_t *forward, vec3_t *right, vec3_t *up )
{
	float	sr, sp, sy, cr, cp, cy;

	SinCos( DEG2RAD( angles.YAW ), &sy, &cy );
	SinCos( DEG2RAD( angles.PITCH ), &sp, &cp );
	SinCos( DEG2RAD( angles.ROLL ), &sr, &cr );

	if( forward )
	{
		forward->x = cp * cy;
		forward->y = cp * sy;
		forward->z = -sp;
	}

	if( right )
	{
		right->x = (-1.0f * sr * sp * cy + -1.0f * cr * -sy );
		right->y = (-1.0f * sr * sp * sy + -1.0f * cr * cy );
		right->z = (-1.0f * sr * cp);
	}

	if( up )
	{
		up->x = (cr * sp * cy + -sr * -sy );
		up->y = (cr * sp * sy + -sr * cy );
		up->z = (cr * cp);
	}
}

static void VectorMA (vec3_t va, double scale, vec3_t vb, vec3_t vc)
{
	vc.x = va.x + scale*vb.x;
	vc.y = va.y + scale*vb.y;
	vc.z = va.z + scale*vb.z;
}

float VectorNormalize(vec3_t *v)
{
    float ilength = (float)sqrt(DotProduct(*v, *v));
    float magnitude = ilength;
    if (ilength)
    {
        ilength = 1.0f / ilength;
        v->x *= ilength;
        v->y *= ilength;
        v->z *= ilength;
    }
    return magnitude;
}

float PM_CalcRoll (vec3_t angles, vec3_t velocity, float rollangle, float rollspeed )
{
    float   sign;
    float   side;
    float   value;
	vec3_t  forward, right, up;
    
	AngleVectors (angles, &forward, &right, &up);
    
	side = DotProduct (velocity, right);
    
	sign = side < 0 ? -1 : 1;
    
	side = fabs(side);
    
	value = rollangle;
    
	if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
    else
	{
		side = value;
	}
  
	return side * sign;
}

int PM_ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;
	
	angle = normal.z;

	blocked = 0x00;            // Assume unblocked.
	if (angle > 0)      // If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;		// 
	if (!angle)         // If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;		// 
	
	// Determine how far along plane to slide based on incoming direction.
	// Scale by overbounce factor.
	backoff = DotProduct (in, normal) * overbounce;

    change = normal.x*backoff;
    out.x = in.x - change;
    // If out velocity is too small, zero it out.
    if (out.x > -STOP_EPSILON && out.x < STOP_EPSILON)
        out.x = 0;
    change = normal.y*backoff;
    out.y = in.y - change;
    // If out velocity is too small, zero it out.
    if (out.y > -STOP_EPSILON && out.y < STOP_EPSILON)
        out.y = 0;
    change = normal.z*backoff;
    out.z = in.z - change;
    // If out velocity is too small, zero it out.
    if (out.z > -STOP_EPSILON && out.z < STOP_EPSILON)
        out.z = 0;
	
	// Return blocking flags.
	return blocked;
}

qboolean PM_AddToTouched(pmtrace_t tr, vec3_t impactvelocity)
{
	int i;

	for (i = 0; i < pmove->numtouch; i++)
	{
		if (pmove->touchindex[i].ent == tr.ent)
			break;
	}
	if (i != pmove->numtouch)  // Already in list.
		return false;

	VectorCopy( impactvelocity, tr.deltavelocity );

	if (pmove->numtouch >= MAX_PHYSENTS)
		pmove->Con_DPrintf("Too many entities were touched!\n");

	pmove->touchindex[pmove->numtouch++] = tr;
	return true;
}

pmtrace_t PM_PlayerTrace(vec3_t start, vec3_t end, int traceFlags, int ignore_pe)
{
    pmtrace_t tr;
    memset(&tr, 0, sizeof(pmtrace_t));

    vec3_t hitPos;
    CollisionPoly* poly = NULL;
    int bgId = -1;

    vec3_t start_oot = v_goldsrc_to_oot(start);
    vec3_t end_oot = v_goldsrc_to_oot(end);

    // Perform the line trace (check wall, floor, and ceiling)
    int hit = BgCheck_EntityLineTest1(&play->colCtx, &start_oot, &end_oot, &hitPos, &poly, 1, 0, 1, 1, &bgId);

    vec3_t hitPos_gs = v_oot_to_goldsrc(hitPos);

	/*vec3_t zero = {0,0,0};
	EffectSsKiraKira_SpawnSmallYellow(play, &end_oot, &zero, &zero);*/

    if (hit && poly != NULL) {
        tr.fraction = sqrtf(
            (hitPos_gs.x - start.x) * (hitPos_gs.x - start.x) +
            (hitPos_gs.y - start.y) * (hitPos_gs.y - start.y) +
            (hitPos_gs.z - start.z) * (hitPos_gs.z - start.z)
        ) / sqrtf(
            (end.x - start.x) * (end.x - start.x) +
            (end.y - start.y) * (end.y - start.y) +
            (end.z - start.z) * (end.z - start.z)
        );
        tr.endpos.x = hitPos_gs.x;
        tr.endpos.y = hitPos_gs.y;
        tr.endpos.z = hitPos_gs.z;

        // Fill in the plane normal from the poly
        tr.plane.normal.x = COLPOLY_GET_NORMAL(poly->normal.x);
        tr.plane.normal.y = COLPOLY_GET_NORMAL(poly->normal.y);
        tr.plane.normal.z = COLPOLY_GET_NORMAL(poly->normal.z);

        tr.plane.normal = v_oot_to_goldsrc(tr.plane.normal);
        VectorNormalize(&tr.plane.normal);

        // Check if start or end is inside solid
        int startSolid = BgCheck_PosInStaticBoundingBox(&play->colCtx, &start_oot);
        int endSolid = BgCheck_PosInStaticBoundingBox(&play->colCtx, &end_oot);
        tr.startsolid = (startSolid != 0) ? 1 : 0;
        tr.allsolid = (tr.startsolid && (endSolid != 0)) ? 1 : 0;
        //tr.startsolid = 0;
        //tr.allsolid = 0;

        tr.ent = bgId;     // Set to bgId or 0 for world
    } else {
        // No hit, fraction is 1, endpos is end
        tr.fraction = 1.0f;
        tr.endpos = end;
        tr.plane.normal.x = 0.0f;
        tr.plane.normal.y = 0.0f;
        tr.plane.normal.z = 1.0f;
        tr.startsolid = 0;
        tr.allsolid = 0;
        tr.ent = 0;
		tr.inopen = 1;
    }

    return tr;
}

qboolean PM_CheckWater ()
{
    return 0; /// for now, no water

	vec3_t	point;
	int		cont;
	int		truecont;
	float     height;
	float		heightover2;

	// Pick a spot just above the players feet.
	point.x = pmove->origin.x + (pmove->player_mins[pmove->usehull].x + pmove->player_maxs[pmove->usehull].x) * 0.5;
	point.y = pmove->origin.y + (pmove->player_mins[pmove->usehull].y + pmove->player_maxs[pmove->usehull].y) * 0.5;
	point.z = pmove->origin.z + pmove->player_mins[pmove->usehull].z + 1;
	
	// Assume that we are not in water at all.
	pmove->waterlevel = 0;
	pmove->watertype = CONTENTS_EMPTY;

	// Grab point contents.
	cont = pmove->PM_PointContents (point, &truecont );
	// Are we under water? (not solid and not empty?)
	if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		// Set water type
		pmove->watertype = cont;

		// We are at least at level one
		pmove->waterlevel = 1;

		height = (pmove->player_mins[pmove->usehull].z + pmove->player_maxs[pmove->usehull].z);
		heightover2 = height * 0.5;

		// Now check a point that is at the player hull midpoint.
		point.z = pmove->origin.z + heightover2;
		cont = pmove->PM_PointContents (point, NULL );
		// If that point is also under water...
		if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
		{
			// Set a higher water level.
			pmove->waterlevel = 2;

			// Now check the eye position.  (view_ofs is relative to the origin)
			point.z = pmove->origin.z + pmove->view_ofs.z;

			cont = pmove->PM_PointContents (point, NULL );
			if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT ) 
				pmove->waterlevel = 3;  // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if ( ( truecont <= CONTENTS_CURRENT_0 ) &&
			 ( truecont >= CONTENTS_CURRENT_DOWN ) )
		{
			// The deeper we are, the stronger the current.
			static vec3_t current_table[] =
			{
				{1, 0, 0}, {0, 1, 0}, {-1, 0, 0},
				{0, -1, 0}, {0, 0, 1}, {0, 0, -1}
			};

			VectorMA (pmove->basevelocity, 50.0*pmove->waterlevel, current_table[CONTENTS_CURRENT_0 - truecont], pmove->basevelocity);
		}
	}

	return pmove->waterlevel > 1;
}

/*void PM_CatagorizePosition (void)
{
	vec3_t		point;
	pmtrace_t		tr;

// if the player hull point one unit down is solid, the player
// is on ground

// see if standing on something solid	

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	PM_CheckWater();

	vec3_t origin = pmove->origin;
	origin.z += 1;
	point.x = pmove->origin.x;
	point.y = pmove->origin.y;
	point.z = pmove->origin.z - 1;

	if (pmove->velocity.z > 180)   // Shooting up really fast.  Definitely not on ground.
	{
		pmove->onground = -1;
	}
	else
	{
		// Try and move down.
		tr = pmove->PM_PlayerTrace (origin, point, PM_NORMAL, -1 );

		if (tr.ent == player->actor.id)
		{	// Don't clip against yourself.
			pmove->onground = -1;
			return;
		}

		// If we hit a steep plane, we are not on ground
		if (tr.plane.normal.z < 0.7)
			pmove->onground = -1;	// too steep
		else
			pmove->onground = tr.ent;  // Otherwise, point to index of ent under us.

		// If we are on something...
		if (pmove->onground != -1)
		{
			// Then we are not in water jump sequence
			pmove->waterjumptime = 0;
			// If we could make the move, drop us down that 1 pixel
			if (pmove->waterlevel < 2 && !tr.startsolid && !tr.allsolid)
				VectorCopy (tr.endpos, pmove->origin);
		}
	}
}*/

void PM_CatagorizePosition (void)
{
    if (pmove->velocity.z > 180)   // Shooting up really fast.  Definitely not on ground.
	{
		pmove->onground = -1;
	}
	else
	{
		CollisionPoly *poly = NULL;
		int bgId = -1;
		vec3_t checkFrom = v_goldsrc_to_oot(pmove->origin);
		checkFrom.y += 60 * GOLDSRC_UNIT_SCALE;
		float floorHeight = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &player->actor, &checkFrom);
		float floorHeightDiff = floorHeight - v_goldsrc_to_oot(pmove->origin).y;

		printf("Floor height: %f, diff: %f\n", floorHeight, floorHeightDiff);

		if (floorHeight != BGCHECK_Y_MIN && floorHeightDiff >= 0.0f && floorHeightDiff <= sv_stepsize) 
		{
			vec3_t new_origin = v_goldsrc_to_oot(pmove->origin);
			new_origin.y = floorHeight;
			pmove->origin = v_oot_to_goldsrc(new_origin);
			
			vec3_t plane_normal;
			plane_normal.x = COLPOLY_GET_NORMAL(poly->normal.x);
			plane_normal.y = COLPOLY_GET_NORMAL(poly->normal.y);
			plane_normal.z = COLPOLY_GET_NORMAL(poly->normal.z);

			plane_normal = v_oot_to_goldsrc(plane_normal);
			VectorNormalize(&plane_normal);

			if (plane_normal.z < 0.7f) {
				pmove->onground = -1;	// too steep
			} else {
				pmove->onground = bgId;
			}
		}
		else
		{
			pmove->onground = -1;
		}
	}
}

/*void PM_CatagorizePosition (void)
{
    // testing
    pmove->onground = 0;
    pmove->waterjumptime = 0;
    pmove->waterlevel = 0;
    return;

    vec3_t point;
    pmtrace_t tr;

    // Check water level/type first
    PM_CheckWater();

    if (pmove->velocity.z > 180)   // Shooting up really fast.  Definitely not on ground.
    {
        pmove->onground = -1;
    }
    else
    {
        // Use BgCheck_EntityRaycastFloor3 to find the floor directly below the player
        CollisionPoly* poly = NULL;
        int bgId = -1;
        vec3_t oot_origin = v_goldsrc_to_oot(pmove->origin);
        float floorZ = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &oot_origin);

        if (poly != NULL && floorZ != BGCHECK_Y_MIN && (pmove->origin.z - floorZ) <= 2.0f) {
            // If the floor is too steep, not on ground
            if (COLPOLY_GET_NORMAL(poly->normal.y) < 0.7f)
                pmove->onground = -1;
            else
                pmove->onground = bgId;
            // Snap to floor if not in deep water and not starting in solid
            if (pmove->onground != -1) {
                pmove->waterjumptime = 0;
                if (pmove->waterlevel < 2 && !tr.startsolid && !tr.allsolid)
                    pmove->origin.y = floorZ;
            }
        } else {
            pmove->onground = -1;
        }
    }
}*/

void PM_DropPunchAngle ( vec3_t punchangle )
{
	float	len;
	
	len = VectorNormalize ( &punchangle );
	len -= (10.0 + len * 0.5) * pmove->frametime;
	len = max( len, 0.0 );
	VectorScale ( punchangle, len, punchangle);
}

/*
==============
PM_CheckParamters

==============
*/
void PM_CheckParamters( void )
{
	float spd;
	float maxspeed;
	vec3_t	v_angle;

	spd = ( pmove->cmd.forwardmove * pmove->cmd.forwardmove ) +
		  ( pmove->cmd.sidemove * pmove->cmd.sidemove ) +
		  ( pmove->cmd.upmove * pmove->cmd.upmove );
	spd = sqrt( spd );

	maxspeed = pmove->clientmaxspeed; //atof( pmove->PM_Info_ValueForKey( pmove->physinfo, "maxspd" ) );
	if ( maxspeed != 0.0 )
	{
		pmove->maxspeed = min( maxspeed, pmove->maxspeed );
	}

	if ( ( spd != 0.0 ) &&
		 ( spd > pmove->maxspeed ) )
	{
		float fRatio = pmove->maxspeed / spd;
		pmove->cmd.forwardmove *= fRatio;
		pmove->cmd.sidemove    *= fRatio;
		pmove->cmd.upmove      *= fRatio;
	}

	if ( pmove->flags & FL_FROZEN ||
		 pmove->flags & FL_ONTRAIN || 
		 pmove->dead )
	{
		pmove->cmd.forwardmove = 0;
		pmove->cmd.sidemove    = 0;
		pmove->cmd.upmove      = 0;
	}


	PM_DropPunchAngle( pmove->punchangle );

	// Take angles from command.
	if ( !pmove->dead )
	{
		VectorCopy ( pmove->cmd.viewangles, v_angle );         
		VectorAdd( v_angle, pmove->punchangle, v_angle );

		// Set up view angles.
		pmove->angles.ROLL	=	PM_CalcRoll ( v_angle, pmove->velocity, pmove->movevars->rollangle, pmove->movevars->rollspeed )*4;
		pmove->angles.PITCH =	v_angle.PITCH;
		pmove->angles.YAW   =	v_angle.YAW;
	}
	else
	{
		VectorCopy( pmove->oldangles, pmove->angles );
	}

	// Set dead player view_offset
	if ( pmove->dead )
	{
		pmove->view_ofs.z = PM_DEAD_VIEWHEIGHT;
	}

	// Adjust client view angles to match values used on server.
	if (pmove->angles.YAW > 180.0f)
	{
		pmove->angles.YAW -= 360.0f;
	}

}

void PM_ReduceTimers( void )
{
	if ( pmove->flTimeStepSound > 0 )
	{
		pmove->flTimeStepSound -= pmove->cmd.msec;
		if ( pmove->flTimeStepSound < 0 )
		{
			pmove->flTimeStepSound = 0;
		}
	}
	if ( pmove->flDuckTime > 0 )
	{
		pmove->flDuckTime -= pmove->cmd.msec;
		if ( pmove->flDuckTime < 0 )
		{
			pmove->flDuckTime = 0;
		}
	}
	if ( pmove->flSwimTime > 0 )
	{
		pmove->flSwimTime -= pmove->cmd.msec;
		if ( pmove->flSwimTime < 0 )
		{
			pmove->flSwimTime = 0;
		}
	}
}

qboolean PM_InWater( void )
{
    return 0; // TODO
	return ( pmove->waterlevel > 1 );
}

void PM_CheckVelocity ()
{
	// See if it's bogus.
    if (IS_NAN(pmove->velocity.x))
    {
        pmove->Con_Printf ("PM  Got a NaN velocity x\n");
        pmove->velocity.x = 0;
    }
    if (IS_NAN(pmove->origin.x))
    {
        pmove->Con_Printf ("PM  Got a NaN origin on x\n");
        pmove->origin.x = 0;
    }

    // Bound it.
    if (pmove->velocity.x > pmove->movevars->maxvelocity) 
    {
        pmove->Con_DPrintf ("PM  Got a velocity too high on x\n");
        pmove->velocity.x = pmove->movevars->maxvelocity;
    }
    else if (pmove->velocity.x < -pmove->movevars->maxvelocity)
    {
        pmove->Con_DPrintf ("PM  Got a velocity too low on x\n");
        pmove->velocity.x = -pmove->movevars->maxvelocity;
    }

    if (IS_NAN(pmove->velocity.y))
    {
        pmove->Con_Printf ("PM  Got a NaN velocity y\n");
        pmove->velocity.y = 0;
    }
    if (IS_NAN(pmove->origin.y))
    {
        pmove->Con_Printf ("PM  Got a NaN origin on y\n");
        pmove->origin.y = 0;
    }

    if (pmove->velocity.y > pmove->movevars->maxvelocity) 
    {
        pmove->Con_DPrintf ("PM  Got a velocity too high on y\n");
        pmove->velocity.y = pmove->movevars->maxvelocity;
    }
    else if (pmove->velocity.y < -pmove->movevars->maxvelocity)
    {
        pmove->Con_DPrintf ("PM  Got a velocity too low on y\n");
        pmove->velocity.y = -pmove->movevars->maxvelocity;
    }

    if (IS_NAN(pmove->velocity.z))
    {
        pmove->Con_Printf ("PM  Got a NaN velocity z\n");
        pmove->velocity.z = 0;
    }
    if (IS_NAN(pmove->origin.z))
    {
        pmove->Con_Printf ("PM  Got a NaN origin on z\n");
        pmove->origin.z = 0;
    }

    if (pmove->velocity.z > pmove->movevars->maxvelocity) 
    {
        pmove->Con_DPrintf ("PM  Got a velocity too high on z\n");
        pmove->velocity.z = pmove->movevars->maxvelocity;
    }
    else if (pmove->velocity.z < -pmove->movevars->maxvelocity)
    {
        pmove->Con_DPrintf ("PM  Got a velocity too low on z\n");
        pmove->velocity.z = -pmove->movevars->maxvelocity;
    }
}

void PM_AddCorrectGravity ()
{
	float	ent_gravity;

	if ( pmove->waterjumptime )
		return;

	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.  
	pmove->velocity.z -= (ent_gravity * pmove->movevars->gravity * 0.5 * pmove->frametime );
	pmove->velocity.z += pmove->basevelocity.z * pmove->frametime;
	pmove->basevelocity.z = 0;

	PM_CheckVelocity();
}

void PM_FixupGravityVelocity ()
{
	float	ent_gravity;

	if ( pmove->waterjumptime )
		return;

	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt 
  	pmove->velocity.z -= (ent_gravity * pmove->movevars->gravity * pmove->frametime * 0.5 );

	PM_CheckVelocity();
}

void PM_Jump (void)
{
	int i;
	qboolean tfc = false;

	qboolean cansuperjump = false;

	if (pmove->dead)
	{
		pmove->oldbuttons |= IN_JUMP ;	// don't jump again until released
		return;
	}

	// tfc = atoi( pmove->PM_Info_ValueForKey( pmove->physinfo, "tfc" ) ) == 1 ? true : false;

	// Spy that's feigning death cannot jump
	/*if ( tfc && 
		( pmove->deadflag == ( DEAD_DISCARDBODY + 1 ) ) )
	{
		return;
	}*/

	// See if we are waterjumping.  If so, decrement count and return.
	if ( pmove->waterjumptime )
	{
		pmove->waterjumptime -= pmove->cmd.msec;
		if (pmove->waterjumptime < 0)
		{
			pmove->waterjumptime = 0;
		}
		return;
	}

	// If we are in the water most of the way...
	if (pmove->waterlevel >= 2)
	{	// swimming, not jumping
		pmove->onground = -1;

		if (pmove->watertype == CONTENTS_WATER)    // We move up a certain amount
			pmove->velocity.z = 100;
		else if (pmove->watertype == CONTENTS_SLIME)
			pmove->velocity.z = 80;
		else  // LAVA
			pmove->velocity.z = 50;

		// play swiming sound
		if ( pmove->flSwimTime <= 0 )
		{
			// Don't play sound again for 1 second
			pmove->flSwimTime = 1000;
			switch ( pmove->RandomLong( 0, 3 ) )
			{ 
			case 0:
				/// pmove->PM_PlaySound( CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); // TODO
				break;
			case 1:
				/// pmove->PM_PlaySound( CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, 0, PITCH_NORM ); // TODO
				break;
			case 2:
				/// pmove->PM_PlaySound( CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, 0, PITCH_NORM ); // TODO
				break;
			case 3:
				/// pmove->PM_PlaySound( CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, 0, PITCH_NORM ); // TODO
				break;
			}
		}

		return;
	}

	// No more effect
 	if ( pmove->onground == -1 )
	{
		// Flag that we jumped.
		// HACK HACK HACK
		// Remove this when the game .dll no longer does physics code!!!!
		pmove->oldbuttons |= IN_JUMP;	// don't jump again until released
		return;		// in air, so no effect
	}

	if ( pmove->oldbuttons & IN_JUMP && !sv_autohop )
		return;		// don't pogo stick

	// In the air now.
    pmove->onground = -1;

	if ( tfc )
	{
		/// pmove->PM_PlaySound( CHAN_BODY, "player/plyrjmp8.wav", 0.5, ATTN_NORM, 0, PITCH_NORM );
	}
	else
	{
		/// PM_PlayStepSound( PM_MapTextureTypeStepType( pmove->chtexturetype ), 1.0 ); // TODO
	}

	// See if user can super long jump?
	// cansuperjump = atoi( pmove->PM_Info_ValueForKey( pmove->physinfo, "slj" ) ) == 1 ? true : false; /// TODO: hover boots? item?

	// Acclerate upward
	// If we are ducking...
	if ( ( pmove->bInDuck ) || ( pmove->flags & FL_DUCKING ) )
	{
		// Adjust for super long jump module
		// UNDONE -- note this should be based on forward angles, not current velocity.
		if ( cansuperjump &&
			( pmove->cmd.buttons & IN_DUCK ) &&
			( pmove->flDuckTime > 0 ) &&
			Length( pmove->velocity ) > 50 )
		{
			pmove->punchangle.x = -5;

            pmove->velocity.x = pmove->forward.x * PLAYER_LONGJUMP_SPEED * 1.6;
            pmove->velocity.y = pmove->forward.y * PLAYER_LONGJUMP_SPEED * 1.6;
		
			pmove->velocity.z = sqrt(2 * sv_jumpspeed * 56.0);
		}
		else
		{
			pmove->velocity.z = sqrt(2 * sv_jumpspeed * 45.0);
		}
	}
	else
	{
		pmove->velocity.z = sqrt(2 * sv_jumpspeed * 45.0);
	}

	// Decay it for simulation
	PM_FixupGravityVelocity();

	// Flag that we jumped.
	pmove->oldbuttons |= IN_JUMP;	// don't jump again until released
}

void PM_Friction (void)
{
	vec3_t	vel;
	float	speed, newspeed, control;
	float	friction;
	float	drop;
	vec3_t newvel;
	
	// If we are in water jump cycle, don't apply friction
	if (pmove->waterjumptime)
		return;

	// Get velocity
	vel = pmove->velocity;
	
	// Calculate speed
	speed = sqrt(vel.x*vel.x +vel.y*vel.y + vel.z*vel.z);
	
	// If too slow, return
	if (speed < 0.1f)
	{
		return;
	}

	drop = 0;

// apply ground friction
	if (pmove->onground != -1)  // On an entity that is the ground
	{
		vec3_t start, stop;
		pmtrace_t trace;

		start.x = stop.x = pmove->origin.x + vel.x/speed*16;
		start.y = stop.y = pmove->origin.y + vel.y/speed*16;
		start.z = pmove->origin.z + pmove->player_mins[pmove->usehull].z;
		stop.z = start.z - 34;

		trace = pmove->PM_PlayerTrace (start, stop, PM_NORMAL, -1 );

		if (trace.fraction == 1.0)
			friction = pmove->movevars->friction*pmove->movevars->edgefriction;
		else
			friction = pmove->movevars->friction;
		
		// Grab friction value.
		//friction = pmove->movevars->friction;      

		friction *= pmove->friction;  // player friction?

		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		control = (speed < pmove->movevars->stopspeed) ?
			pmove->movevars->stopspeed : speed;
		// Add the amount to t'he drop amount.
		drop += control*friction*pmove->frametime;
	}

// apply water friction
//	if (pmove->waterlevel)
//		drop += speed * pmove->movevars->waterfriction * waterlevel * pmove->frametime;

// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= speed;

	// Adjust velocity according to proportion.
	newvel.x = vel.x * newspeed;
	newvel.y = vel.y * newspeed;
	newvel.z = vel.z * newspeed;

	VectorCopy( newvel, pmove->velocity );
}

int PM_FlyMove (void)
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity;
	vec3_t      new_velocity;
	int			i, j;
	pmtrace_t	trace;
	vec3_t		end;
	float		time_left, allFraction;
	int			blocked;
		
	numbumps  = 4;           // Bump up to four times
	
	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes
	VectorCopy (pmove->velocity, original_velocity);  // Store original velocity
	VectorCopy (pmove->velocity, primal_velocity);
	
	allFraction = 0;
	time_left = pmove->frametime;   // Total time for this movement operation.

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		if (!pmove->velocity.x && !pmove->velocity.y && !pmove->velocity.z)
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
        end.x = pmove->origin.x + time_left * pmove->velocity.x;
        end.y = pmove->origin.y + time_left * pmove->velocity.y;
        end.z = pmove->origin.z + time_left * pmove->velocity.z;

		/*VectorCopy (end, pmove->origin);
		return; // testing*/

		// See if we can make it from origin to end point.
		trace = pmove->PM_PlayerTrace (pmove->origin, end, PM_NORMAL, -1 );

#if 0 // Sneaky: slip in slopefix here. See if speedrunners notice. /// TODO
		// Check if we are stuck on the surface (HACKHACK: this solves precision error in the engine for small movements)
		if (trace.fraction == 0.0)
		{
			// Move end point to a thousandth fraction of the unit vector away from the wall and try to move again
            end.x += trace.plane.normal.x * 0.001;
            end.y += trace.plane.normal.y * 0.001;
            end.z += trace.plane.normal.z * 0.001;
			trace = pmove->PM_PlayerTrace(pmove->origin, end, PM_NORMAL, -1);
		}
#endif

		allFraction += trace.fraction;
		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (trace.allsolid)
		{	// entity is trapped in another solid
			VectorCopy (vec3_origin, pmove->velocity);
			//Con_DPrintf("Trapped 4\n");
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove->origin and 
		//  zero the plane counter.
		if (trace.fraction > 0)
		{	// actually covered some distance
			VectorCopy (trace.endpos, pmove->origin);
			VectorCopy (pmove->velocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (trace.fraction == 1)
			 break;		// moved the entire distance

		//if (!trace.ent)
		//	Sys_Error ("PM_PlayerTrace: !trace.ent");

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		PM_AddToTouched(trace, pmove->velocity);

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (trace.plane.normal.z > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!trace.plane.normal.z)
		{
			blocked |= 2;		// step / wall
			//Con_DPrintf("Blocked by %i\n", trace.ent);
		}

		// Reduce amount of pmove->frametime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * trace.fraction;
		
		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (vec3_origin, pmove->velocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;
//

// modify original_velocity so it parallels all of the clip planes
//
		if ( pmove->movetype == MOVETYPE_WALK &&
			((pmove->onground == -1) || (pmove->friction != 1)) )	// relfect player velocity
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i].z > 0.7  )
				{// floor or slope
					PM_ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else															
					PM_ClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + pmove->movevars->bounce * (1-pmove->friction) );
			}

			VectorCopy( new_velocity, pmove->velocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i<numplanes ; i++)
			{
				PM_ClipVelocity (
					original_velocity,
					planes[i],
					pmove->velocity,
					1);
				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (DotProduct (pmove->velocity, planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}
			
			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove->velocity is set in clipping call, no need to set again.
				;  
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					//Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
					VectorCopy (vec3_origin, pmove->velocity);
					//Con_DPrintf("Trapped 4\n");

					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				d = DotProduct (dir, pmove->velocity);
				VectorScale (dir, d, pmove->velocity );
			}

	//
	// if original velocity is against the original velocity, stop dead
	// to avoid tiny occilations in sloping corners
	//
			if (DotProduct (pmove->velocity, primal_velocity) <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy (vec3_origin, pmove->velocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		VectorCopy (vec3_origin, pmove->velocity);
		//Con_DPrintf( "Don't stick\n" );
	}

	return blocked;
}

void PM_Accelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed;

	// Dead player's don't accelerate
	if (pmove->dead)
		return;

	// If waterjumping, don't accelerate
	if (pmove->waterjumptime)
		return;

	// See if we are changing direction a bit
	currentspeed = DotProduct (pmove->velocity, wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	accelspeed = accel * pmove->frametime * wishspeed * pmove->friction;
	
	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust velocity.
	pmove->velocity.x += accelspeed * wishdir.x;
    pmove->velocity.y += accelspeed * wishdir.y;
    pmove->velocity.z += accelspeed * wishdir.z;
}

void PM_WalkMove ()
{
	int			clip;
	int			oldonground;
	int i;

	vec3_t		wishvel;
	float       spd;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;

	vec3_t dest, start;
	vec3_t original, originalvel;
	vec3_t down, downvel;
	float downdist, updist;

	pmtrace_t trace;
	
	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	// Zero out z components of movement vectors
	pmove->forward.z = 0;
	pmove->right.z   = 0;
	
	VectorNormalize (&pmove->forward);  // Normalize remainder of vectors.
	VectorNormalize (&pmove->right);    // 
    
    wishvel.x = pmove->forward.x*fmove + pmove->right.x*smove;
    wishvel.y = pmove->forward.y*fmove + pmove->right.y*smove;
	wishvel.z = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(&wishdir);

//
// Clamp to server defined max speed
//
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale (wishvel, pmove->maxspeed/wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}

	// Set pmove velocity
	pmove->velocity.z = 0;
	PM_Accelerate (wishdir, wishspeed, pmove->movevars->accelerate);
	pmove->velocity.z = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity );

	spd = Length( pmove->velocity );

	if (spd < 1.0f)
	{
		VectorClear( pmove->velocity );
		return;
	}

	// If we are not moving, do nothing
	//if (!pmove->velocity[0] && !pmove->velocity[1] && !pmove->velocity[2])
	//	return;

	oldonground = pmove->onground;

// first try just moving to the destination	
	dest.x = pmove->origin.x + pmove->velocity.x*pmove->frametime;
	dest.y = pmove->origin.y + pmove->velocity.y*pmove->frametime;	
	dest.z = pmove->origin.z;

	// first try moving directly to the next spot
	VectorCopy (dest, start);
	trace = pmove->PM_PlayerTrace (pmove->origin, dest, PM_NORMAL, -1 );
	// If we made it all the way, then copy trace end
	//  as new player position.
	if (trace.fraction == 1)
	{
		VectorCopy (trace.endpos, pmove->origin);
		return;
	}

	if (oldonground == -1 &&   // Don't walk up stairs if not on ground.
		pmove->waterlevel  == 0)
		return;

	if (pmove->waterjumptime)         // If we are jumping out of water, don't do anything more.
		return;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	VectorCopy (pmove->origin, original);       // Save out original pos &
	VectorCopy (pmove->velocity, originalvel);  //  velocity.

	// Slide move
	clip = PM_FlyMove ();

	// Copy the results out
	VectorCopy (pmove->origin  , down);
	VectorCopy (pmove->velocity, downvel);

	// Reset original values.
	VectorCopy (original, pmove->origin);

	VectorCopy (originalvel, pmove->velocity);

	// Start out up one stair height
	VectorCopy (pmove->origin, dest);
	dest.z += pmove->movevars->stepsize;
	
	trace = pmove->PM_PlayerTrace (pmove->origin, dest, PM_NORMAL, -1 );
	// If we started okay and made it part of the way at least,
	//  copy the results to the movement start position and then
	//  run another move try.
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, pmove->origin);
	}

// slide move the rest of the way.
	clip = PM_FlyMove ();

// Now try going back down from the end point
//  press down the stepheight
	VectorCopy (pmove->origin, dest);
	dest.z -= pmove->movevars->stepsize;
	
	trace = pmove->PM_PlayerTrace (pmove->origin, dest, PM_NORMAL, -1 );

	// If we are not on the ground any more then
	//  use the original movement attempt
	if ( trace.plane.normal.z < 0.7)
		goto usedown;
	// If the trace ended up in empty space, copy the end
	//  over to the origin.
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, pmove->origin);
	}
	// Copy this origion to up.
	VectorCopy (pmove->origin, pmove->up);

	// decide which one went farther
	downdist = (down.x - original.x)*(down.x - original.x)
		     + (down.y - original.y)*(down.y - original.y);
	updist   = (pmove->up.x   - original.x)*(pmove->up.x   - original.x)
		     + (pmove->up.y   - original.y)*(pmove->up.y   - original.y);

	if (downdist > updist)
	{
usedown:
		VectorCopy (down   , pmove->origin);
		VectorCopy (downvel, pmove->velocity);
	} else // copy z value from slide move
		pmove->velocity.z = downvel.z;

}

void PM_AirAccelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed, wishspd = wishspeed;
		
	if (pmove->dead)
		return;
	if (pmove->waterjumptime)
		return;

	// Cap speed
	//wishspd = VectorNormalize (&pmove->wishveloc);
	
	if (wishspd > 30)
		wishspd = 30;
	// Determine veer amount
	currentspeed = DotProduct (pmove->velocity, wishdir);
	// See how much to add
	addspeed = wishspd - currentspeed;
	// If not adding any, done.
	if (addspeed <= 0)
		return;
	// Determine acceleration speed after acceleration

	accelspeed = accel * wishspeed * pmove->frametime * pmove->friction;
	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	// Adjust pmove vel.
	pmove->velocity.x += accelspeed*wishdir.x;
    pmove->velocity.y += accelspeed*wishdir.y;
    pmove->velocity.z += accelspeed*wishdir.z;
}

void PM_AirMove (void)
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	// Zero out z components of movement vectors
	pmove->forward.z = 0;
	pmove->right.z   = 0;
	// Renormalize
	VectorNormalize (&pmove->forward);
	VectorNormalize (&pmove->right);

	// Determine x and y parts of velocity
	wishvel.x = pmove->forward.x*fmove + pmove->right.x*smove;
    wishvel.y = pmove->forward.y*fmove + pmove->right.y*smove;
	// Zero out z part of velocity
	wishvel.z = 0;             

	 // Determine maginitude of speed of move
	VectorCopy (wishvel, wishdir);  
	wishspeed = VectorNormalize(&wishdir);

	// Clamp to server defined max speed
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale (wishvel, pmove->maxspeed/wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}
	
	PM_AirAccelerate (wishdir, wishspeed, pmove->movevars->airaccelerate);

	// Add in any base velocity to the current velocity.
	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity );

	PM_FlyMove ();
}

void PM_PlayerMove ( qboolean server )
{
	physent_t *pLadder = NULL;

	// Are we running server code?
	pmove->server = server;

	// Adjust speeds etc.
	PM_CheckParamters();

	// Assume we don't touch anything
	pmove->numtouch = 0;                    

	// # of msec to apply movement
	pmove->frametime = pmove->cmd.msec * 0.001; // TODO: Grab actual frametime

	PM_ReduceTimers();

	// Convert view angles to vectors
	AngleVectors (pmove->angles, &pmove->forward, &pmove->right, &pmove->up);

	// PM_ShowClipBox();

#ifdef CLIENT_DLL
	if ( pmove->runfuncs )
	{
		iIsSpectator = false;
		iHasNewViewAngles = false;
		iHasNewViewOrigin = false;
	}
#endif

	// Special handling for spectator and observers. (iuser1 is set if the player's in observer mode)
    /* Z64 -- Don't need spectator
	if ( pmove->spectator || pmove->iuser1 > 0 )
	{
		PM_SpectatorMove();
		PM_CatagorizePosition();
		return;
	}
    */

	// Always try and unstick us unless we are in NOCLIP mode
    /* Z64 -- Might not need this
	if ( pmove->movetype != MOVETYPE_NOCLIP && pmove->movetype != MOVETYPE_NONE )
	{
		if ( PM_CheckStuck() )
		{
			return;  // Can't move, we're stuck
		}
	}
    */

	// Now that we are "unstuck", see where we are ( waterlevel and type, pmove->onground ).
	PM_CatagorizePosition();

	// Store off the starting water level
	pmove->oldwaterlevel = pmove->waterlevel;

	// If we are not on ground, store off how fast we are moving down
	if ( pmove->onground == -1 )
	{
		pmove->flFallVelocity = -pmove->velocity.z;
	}

	g_onladder = 0;
	// Don't run ladder code if dead or on a train
    /* Z64 -- TODO
	if ( !pmove->dead && !(pmove->flags & FL_ONTRAIN) )
	{
		pLadder = PM_Ladder();
		if ( pLadder )
		{
			g_onladder = 1;
		}
	}
    */

	/// PM_UpdateStepSound(); // TODO

	/// PM_Duck(); // TODO
	
	// Don't run ladder code if dead or on a train
    /* Z64 -- TODO
	if ( !pmove->dead && !(pmove->flags & FL_ONTRAIN) )
	{
		if ( pLadder )
		{
			PM_LadderMove( pLadder );
		}
		else if ( pmove->movetype != MOVETYPE_WALK &&
			      pmove->movetype != MOVETYPE_NOCLIP )
		{
			// Clear ladder stuff unless player is noclipping
			//  it will be set immediately again next frame if necessary
			pmove->movetype = MOVETYPE_WALK;
		}
	}
    */

	// Slow down, I'm pulling it! (a box maybe) but only when I'm standing on ground
	if ( ( pmove->onground != -1 ) && ( pmove->cmd.buttons & IN_USE) )
	{
		VectorScale( pmove->velocity, 0.3, pmove->velocity );
	}

	// Handle movement
	switch ( pmove->movetype )
	{
	default:
		pmove->Con_DPrintf("Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", pmove->movetype, pmove->server);
		break;

	case MOVETYPE_NONE:
		break;

	case MOVETYPE_NOCLIP:
		/// PM_NoClip(); // TODO
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		/// PM_Physics_Toss(); // TODO
		break;

    /* Z64 -- TODO
	case MOVETYPE_FLY:
	
		PM_CheckWater();

		// Was jump button pressed?
		// If so, set velocity to 270 away from ladder.  This is currently wrong.
		// Also, set MOVE_TYPE to walk, too.
		if ( pmove->cmd.buttons & IN_JUMP )
		{
			if ( !pLadder )
			{
				PM_Jump ();
			}
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}
		
		// Perform the move accounting for any base velocity.
		VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity);
		PM_FlyMove ();
		VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);
		break;
    */

	case MOVETYPE_WALK:
		if ( !PM_InWater() )
		{
			PM_AddCorrectGravity();
		}

		// If we are leaping out of the water, just update the counters.
		/* Z64 -- TODO
        if ( pmove->waterjumptime )
		{
			PM_WaterJump();
			PM_FlyMove();

			// Make sure waterlevel is set correctly
			PM_CheckWater();
			return;
		}*/

		// If we are swimming in the water, see if we are nudging against a place we can jump up out
		//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
		/* Z64 -- TODO
        if ( pmove->waterlevel >= 2 ) 
		{
			if ( pmove->waterlevel == 2 )
			{
				PM_CheckWaterJump();
			}

			// If we are falling again, then we must not trying to jump out of water any more.
			if ( pmove->velocity.z < 0 && pmove->waterjumptime )
			{
				pmove->waterjumptime = 0;
			}

			// Was jump button pressed?
			if (pmove->cmd.buttons & IN_JUMP)
			{
				PM_Jump ();
			}
			else
			{
				pmove->oldbuttons &= ~IN_JUMP;
			}

			// Perform regular water movement
			PM_WaterMove();
			
			VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);

			// Get a final position
			PM_CatagorizePosition();
		}
		else */

		// Not underwater
		{
			// Was jump button pressed?
			if ( pmove->cmd.buttons & IN_JUMP )
			{
				if ( !pLadder )
				{
					PM_Jump ();
				}
			}
			else
			{
				pmove->oldbuttons &= ~IN_JUMP;
			}

			// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor, 
			//  we don't slow when standing still, relative to the conveyor.
			if ( pmove->onground != -1 )
			{
				pmove->velocity.z = 0.0;
				PM_Friction();
			}

			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Are we on ground now
			if ( pmove->onground != -1 )
			{
				PM_WalkMove();
			}
			else
			{
				PM_AirMove();  // Take into account movement when in air.
			}

			// Set final flags.
			PM_CatagorizePosition();

			// Now pull the base velocity back out.
			// Base velocity is set if you are on a moving object, like
			//  a conveyor (or maybe another monster?)
			VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity );
				
			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Add any remaining gravitational component.
			if ( !PM_InWater() )
			{
				PM_FixupGravityVelocity();
			}

			// If we are on ground, no downward velocity.
			if ( pmove->onground != -1 )
			{
				pmove->velocity.z = 0;
			}

			// See if we landed on the ground with enough force to play
			//  a landing sound.
			/// PM_CheckFalling(); // TODO
		}

		// Did we enter or leave the water?
		/// PM_PlayWaterSounds(); // TODO
		break;
	}
}

vec3_t PM_Move ( struct playermove_s *ppmove, int server, PlayState *playState, Player *pPlayer, vec3_t playerPos, usercmd_t cmd )
{
	/// assert( pm_shared_initialized );

	pmove = ppmove;
    pmove->cmd = cmd;

    // Set current play state
    play = playState;
	player = pPlayer;

    // Sync position with OoT
    VectorCopy( playerPos, pmove->origin );
	
	PM_PlayerMove( ( server != 0 ) ? true : false );

	if ( pmove->onground != -1 )
	{
		pmove->flags |= FL_ONGROUND;
	}
	else
	{
		pmove->flags &= ~FL_ONGROUND;
	}

	// In single player, reset friction after each movement to FrictionModifier Triggers work still.
	if ( !pmove->multiplayer && ( pmove->movetype == MOVETYPE_WALK  ) )
	{
		pmove->friction = 1.0f;
	}

	//printf( "onground: %d\n", pmove->onground );

	vec3_t start = pmove->origin;
	start.z += 24;
	vec3_t end = start;
	start.x += 100;
	pmove->PM_PlayerTrace (start, end, PM_NORMAL, -1 );

    return pmove->origin;
}

void PM_Init()
{
    if (pmove)
    {
        return;
    }
    movevars = &movevars_singleton;
    pmove = &pmove_s;
    memset(&pmove_s, 0, sizeof(pmove_s));
    memset(&movevars_singleton, 0, sizeof(movevars_singleton));

    pmove_s.movevars = movevars;
    pmove_s.PM_PlayerTrace = PM_PlayerTrace;
    pmove_s.Con_DPrintf = printf;
    pmove_s.Con_Printf = printf;

    movevars_singleton.gravity = sv_gravity;
    movevars_singleton.stopspeed = sv_stopspeed;
    movevars_singleton.maxspeed = sv_maxspeed;
    movevars_singleton.accelerate = sv_accelerate;
    movevars_singleton.airaccelerate = sv_airaccelerate;
    movevars_singleton.wateraccelerate = sv_wateraccelerate;
    movevars_singleton.friction = sv_friction;
    movevars_singleton.edgefriction = sv_edgefriction;
    movevars_singleton.waterfriction = sv_waterfriction;
    movevars_singleton.bounce = sv_bounce;
    movevars_singleton.stepsize = sv_stepsize;
    movevars_singleton.maxvelocity = sv_maxvelocity;
    movevars_singleton.rollangle = sv_rollangle;
    movevars_singleton.rollspeed = sv_rollspeed;

	pmove_s.gravity = sv_entgravity;
    pmove_s.maxspeed = sv_maxspeed;

    // testing
    pmove_s.onground = 0;
    pmove_s.movetype = MOVETYPE_WALK;
}