/*  goldsrc_to_oot.h
    Convert GoldSrc (Z-up, LH) pose -> Ocarina of Time (Y-up, RH) pose.

    Position mapping (basis swap, fixes handedness):
        GoldSrc +X (forward) -> OoT +Z (forward)
        GoldSrc +Y (left)    -> OoT -X (right)
        GoldSrc +Z (up)      -> OoT +Y (up)

    So:  [Xo, Yo, Zo] = [-Yg, Zg, Xg]

    Angle mapping:
      1) Build GoldSrc orientation matrix from (pitchX, yawZ, rollY) in degrees
         using classic Quake/GoldSrc AngleVectors (forward/right/up).
      2) Transform those basis vectors with the same basis swap above.
      3) Extract OoT yaw(Y), pitch(X), roll(Z) from the right-handed matrix
         with Y-X-Z Tait-Bryan convention: R = R_y(Y)*R_x(X)*R_z(Z).
      4) Provide both degrees and OoT s16 “binary angle” (0..65535).

    References:
      - Half-Life coordinate system (GoldSrc): advancedfx wiki
      - N64 is right-handed; OoT structs {x,y,z} for pos/rot (s16 angles)
*/

#include "hlmov_bridge.h"
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float rad(float deg){ return deg * (float)M_PI / 180.0f; }
float deg(float rad){ return rad * 180.0f / (float)M_PI; }

float cl_bob = 0.01;
float cl_bobcycle = 0.8;
float cl_bobup = 0.5;

float cl_forwardspeed = 100;
float cl_sidespeed = 100;
float cl_upspeed = 80;
float cl_movespeedkey = 0.3;

float sv_gravity = 30;  			// Gravity for map
float sv_stopspeed = 5;			    // Deceleration when not moving
float sv_maxspeed = 80; 			// Max allowed speed
float sv_accelerate = 5;			// Acceleration factor
float sv_airaccelerate = 1;		    // Same for when in open air
float sv_wateraccelerate = 5;		// Same for when in water
float sv_stepsize = 20;
float sv_jumpspeed = 40;			// default jump speed
float sv_maxvelocity = 250;         // maximum server velocity.

float sv_noclipspeed = 320; 		// Max allowed speed
float sv_friction = 4;
float sv_edgefriction = 2;			// Extra friction near dropofs
float sv_waterfriction = 1;			// Less in water
float sv_entgravity = 1.0;  		// 1.0
float sv_bounce = 1.0;      		// Wall bounce value. 1.0
//bool mp_footsteps = true;			// Play footstep sounds
float sv_rollangle = 2;
float sv_rollspeed = 200;

int sv_autohop = 1;

// Basis swap: GoldSrc <-> OoT for any vector
vec3_t v_goldsrc_to_oot(vec3_t vg){
    vec3_t vo;
    vo.x = -vg.y/GOLDSRC_UNIT_SCALE;
    vo.y =  vg.z/GOLDSRC_UNIT_SCALE;
    vo.z =  vg.x/GOLDSRC_UNIT_SCALE;
    return vo;
}

vec3_t v_oot_to_goldsrc(vec3_t vo){
    vec3_t vg;
    vg.x =  vo.z*GOLDSRC_UNIT_SCALE;
    vg.y = -vo.x*GOLDSRC_UNIT_SCALE;
    vg.z =  vo.y*GOLDSRC_UNIT_SCALE;
    return vg;
}

// Compute the (not normalized) normal of a triangle given three points
static Vec3f tri_normal(const Vec3f* a, const Vec3f* b, const Vec3f* c) {
    Vec3f ab = {b->x - a->x, b->y - a->y, b->z - a->z};
    Vec3f ac = {c->x - a->x, c->y - a->y, c->z - a->z};
    Vec3f n = {
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x
    };
    return n;
}

// Compute signed distance from point p to the plane defined by normal n and point p0
static float plane_test(const Vec3f* n, const Vec3f* p0, const Vec3f* p) {
    return n->x * (p->x - p0->x) + n->y * (p->y - p0->y) + n->z * (p->z - p0->z);
}

bool point_in_prism(const Vec3f* p, const Vec3f triA[3], const Vec3f triB[3]) {
    // The prism has 5 faces: 2 triangles, 3 quads (as 6 triangles)
    // We'll check if p is on the inside side of all 5 planes

    // Face normals (outward)
    Vec3f nA = tri_normal(&triA[0], &triA[1], &triA[2]);
    Vec3f nB = tri_normal(&triB[0], &triB[1], &triB[2]);
    // Side faces (quads split into triangles)
    Vec3f nS0 = tri_normal(&triA[0], &triA[1], &triB[1]);
    Vec3f nS1 = tri_normal(&triA[1], &triA[2], &triB[2]);
    Vec3f nS2 = tri_normal(&triA[2], &triA[0], &triB[0]);

    // Test against all 5 planes (prism is between triA and triB)
    if (plane_test(&nA, &triA[0], p) > 0.0f) return false;
    if (plane_test(&nB, &triB[0], p) < 0.0f) return false;
    if (plane_test(&nS0, &triA[0], p) > 0.0f) return false;
    if (plane_test(&nS1, &triA[1], p) > 0.0f) return false;
    if (plane_test(&nS2, &triA[2], p) > 0.0f) return false;

    return true;
}

extern void Math_Vec3s_ToVec3f(Vec3f* dest, Vec3s* src);

/**
 * Simple raycast: tests if a line from start to end intersects any polygons (static or dyna) in the scene.
 * Returns true if an intersection is found.
 * Outputs the intersection point in outPos and the polygon hit in outPoly.
 */
/**
 * Simple raycast: tests if a line from start to end intersects any polygons (static or dyna) in the scene.
 * Returns true if an intersection is found.
 * Outputs the intersection point in outPos and the polygon hit in outPoly.
 * Also returns whether the start or end point are inside any solid polygon via startSolid and endSolid.
 * Does not use any existing helpers.
 */
s32 PM_SimpleRaycast(CollisionContext* colCtx, Vec3f* start, Vec3f* end, Vec3f* outPos, CollisionPoly** outPoly, int *outBgId, s32* startSolid, s32* endSolid, int ignoreBgId) {
    f32 closestDistSq = 1.0e38f;
    s32 hit = false;
    *startSolid = false;
    *endSolid = false;

    // --- Static polys ---
    CollisionPoly* polyList = colCtx->colHeader->polyList;
    Vec3s* vtxList = colCtx->colHeader->vtxList;
    s32 numPolys = colCtx->colHeader->numPolygons;

    for (s32 i = 0; i < numPolys; i++) {
        // Get triangle vertices
        Vec3s v0_s, v1_s, v2_s;
        v0_s.x = vtxList[COLPOLY_VTX_INDEX(polyList[i].flags_vIA)].x;
        v0_s.y = vtxList[COLPOLY_VTX_INDEX(polyList[i].flags_vIA)].y;
        v0_s.z = vtxList[COLPOLY_VTX_INDEX(polyList[i].flags_vIA)].z;
        v1_s.x = vtxList[COLPOLY_VTX_INDEX(polyList[i].flags_vIB)].x;
        v1_s.y = vtxList[COLPOLY_VTX_INDEX(polyList[i].flags_vIB)].y;
        v1_s.z = vtxList[COLPOLY_VTX_INDEX(polyList[i].flags_vIB)].z;
        v2_s.x = vtxList[polyList[i].vIC].x;
        v2_s.y = vtxList[polyList[i].vIC].y;
        v2_s.z = vtxList[polyList[i].vIC].z;

        Vec3f v0, v1, v2;
        Math_Vec3s_ToVec3f(&v0, &v0_s);
        Math_Vec3s_ToVec3f(&v1, &v1_s);
        Math_Vec3s_ToVec3f(&v2, &v2_s);

        Vec3f normal = { COLPOLY_GET_NORMAL(polyList[i].normal.x), COLPOLY_GET_NORMAL(polyList[i].normal.y), COLPOLY_GET_NORMAL(polyList[i].normal.z) };

        // Plane equation: normal.x * X + normal.y * Y + normal.z * Z + d = 0
        f32 d = -(normal.x * v0.x + normal.y * v0.y + normal.z * v0.z);

        // Line direction
        Vec3f dir = { end->x - start->x, end->y - start->y, end->z - start->z };

        // Compute denominator
        f32 denom = normal.x * dir.x + normal.y * dir.y + normal.z * dir.z;
        if (fabsf(denom) < 1e-6f) {
            continue; // Parallel, no intersection
        }

        // Compute intersection t
        f32 t = -(normal.x * start->x + normal.y * start->y + normal.z * start->z + d) / denom;
        if (t < 0.0f || t > 1.0f) {
            continue; // Not within segment
        }

        // Intersection point
        Vec3f intersect = { start->x + t * dir.x, start->y + t * dir.y, start->z + t * dir.z };

        // Inside-triangle test using barycentric coordinates
        Vec3f v0v1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
        Vec3f v0v2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
        Vec3f v0p = { intersect.x - v0.x, intersect.y - v0.y, intersect.z - v0.z };

        f32 d00 = v0v1.x * v0v1.x + v0v1.y * v0v1.y + v0v1.z * v0v1.z;
        f32 d01 = v0v1.x * v0v2.x + v0v1.y * v0v2.y + v0v1.z * v0v2.z;
        f32 d11 = v0v2.x * v0v2.x + v0v2.y * v0v2.y + v0v2.z * v0v2.z;
        f32 d20 = v0p.x * v0v1.x + v0p.y * v0v1.y + v0p.z * v0v1.z;
        f32 d21 = v0p.x * v0v2.x + v0p.y * v0v2.y + v0p.z * v0v2.z;

        f32 denom2 = d00 * d11 - d01 * d01;
        if (fabsf(denom2) < 1e-6f) {
            continue;
        }
        f32 v = (d11 * d20 - d01 * d21) / denom2;
        f32 w = (d00 * d21 - d01 * d20) / denom2;
        f32 u = 1.0f - v - w;

        if (u >= 0.0f && v >= 0.0f && w >= 0.0f) {
            // Intersection is inside triangle
            f32 distSq = (intersect.x - start->x) * (intersect.x - start->x) +
                         (intersect.y - start->y) * (intersect.y - start->y) +
                         (intersect.z - start->z) * (intersect.z - start->z);
            if (distSq < closestDistSq) {
                closestDistSq = distSq;
                *outPos = intersect;
                *outPoly = &polyList[i];
                *outBgId = 0;
                hit = true;

                *startSolid = false;
                *endSolid = false;

                // Extrude triangle along inverted normal (50 units)
                Vec3f extrude[3];
                extrude[0].x = v0.x - normal.x * 50.0f;
                extrude[0].y = v0.y - normal.y * 50.0f;
                extrude[0].z = v0.z - normal.z * 50.0f;
                extrude[1].x = v1.x - normal.x * 50.0f;
                extrude[1].y = v1.y - normal.y * 50.0f;
                extrude[1].z = v1.z - normal.z * 50.0f;
                extrude[2].x = v2.x - normal.x * 50.0f;
                extrude[2].y = v2.y - normal.y * 50.0f;
                extrude[2].z = v2.z - normal.z * 50.0f;

                Vec3f tri[3] = { v0, v1, v2 };

                // Check if start is inside the extruded prism
                if (point_in_prism(start, tri, extrude)) {
                    *startSolid = true;
                }

                // Check if end is inside the extruded prism
                if (point_in_prism(end, tri, extrude)) {
                    *endSolid = true;
                }
            }
        }
    }

    // --- Dyna polys ---
    for (int bgId = 0; bgId < BG_ACTOR_MAX; ++bgId) {
        if (!(colCtx->dyna.bgActorFlags[bgId] & 1)) {
            continue;
        }
        if (bgId == ignoreBgId) {
            continue;
        }
        CollisionPoly* dynaPolyList = colCtx->dyna.polyList;
        Vec3s* dynaVtxList = colCtx->dyna.vtxList;
        BgActor* bgActor = &colCtx->dyna.bgActors[bgId];

        // Only check polys belonging to this actor
        s32 polyStart = bgActor->dynaLookup.polyStartIndex;
        s32 polyEnd = (bgId + 1 < BG_ACTOR_MAX) ? colCtx->dyna.bgActors[bgId + 1].dynaLookup.polyStartIndex
                                                : colCtx->dyna.polyNodes.count;

        for (s32 i = polyStart; i < polyEnd; i++) {
            // Get triangle vertices
            Vec3s v0_s, v1_s, v2_s;
            v0_s.x = dynaVtxList[COLPOLY_VTX_INDEX(dynaPolyList[i].flags_vIA)].x;
            v0_s.y = dynaVtxList[COLPOLY_VTX_INDEX(dynaPolyList[i].flags_vIA)].y;
            v0_s.z = dynaVtxList[COLPOLY_VTX_INDEX(dynaPolyList[i].flags_vIA)].z;
            v1_s.x = dynaVtxList[COLPOLY_VTX_INDEX(dynaPolyList[i].flags_vIB)].x;
            v1_s.y = dynaVtxList[COLPOLY_VTX_INDEX(dynaPolyList[i].flags_vIB)].y;
            v1_s.z = dynaVtxList[COLPOLY_VTX_INDEX(dynaPolyList[i].flags_vIB)].z;
            v2_s.x = dynaVtxList[dynaPolyList[i].vIC].x;
            v2_s.y = dynaVtxList[dynaPolyList[i].vIC].y;
            v2_s.z = dynaVtxList[dynaPolyList[i].vIC].z;

            Vec3f v0, v1, v2;
            Math_Vec3s_ToVec3f(&v0, &v0_s);
            Math_Vec3s_ToVec3f(&v1, &v1_s);
            Math_Vec3s_ToVec3f(&v2, &v2_s);

            // Compute triangle normal
            Vec3f normal = { COLPOLY_GET_NORMAL(dynaPolyList[i].normal.x), COLPOLY_GET_NORMAL(dynaPolyList[i].normal.y), COLPOLY_GET_NORMAL(dynaPolyList[i].normal.z) };

            f32 d = -(normal.x * v0.x + normal.y * v0.y + normal.z * v0.z);

            Vec3f dir = { end->x - start->x, end->y - start->y, end->z - start->z };
            f32 denom = normal.x * dir.x + normal.y * dir.y + normal.z * dir.z;
            if (fabsf(denom) < 1e-6f) {
                continue;
            }
            f32 t = -(normal.x * start->x + normal.y * start->y + normal.z * start->z + d) / denom;
            if (t < 0.0f || t > 1.0f) {
                continue;
            }
            Vec3f intersect = { start->x + t * dir.x, start->y + t * dir.y, start->z + t * dir.z };

            Vec3f v0v1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
            Vec3f v0v2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
            Vec3f v0p = { intersect.x - v0.x, intersect.y - v0.y, intersect.z - v0.z };

            f32 d00 = v0v1.x * v0v1.x + v0v1.y * v0v1.y + v0v1.z * v0v1.z;
            f32 d01 = v0v1.x * v0v2.x + v0v1.y * v0v2.y + v0v1.z * v0v2.z;
            f32 d11 = v0v2.x * v0v2.x + v0v2.y * v0v2.y + v0v2.z * v0v2.z;
            f32 d20 = v0p.x * v0v1.x + v0p.y * v0v1.y + v0p.z * v0v1.z;
            f32 d21 = v0p.x * v0v2.x + v0p.y * v0v2.y + v0p.z * v0v2.z;

            f32 denom2 = d00 * d11 - d01 * d01;
            if (fabsf(denom2) < 1e-6f) {
                continue;
            }
            f32 v = (d11 * d20 - d01 * d21) / denom2;
            f32 w = (d00 * d21 - d01 * d20) / denom2;
            f32 u = 1.0f - v - w;

            if (u >= 0.0f && v >= 0.0f && w >= 0.0f) {
                f32 distSq = (intersect.x - start->x) * (intersect.x - start->x) +
                             (intersect.y - start->y) * (intersect.y - start->y) +
                             (intersect.z - start->z) * (intersect.z - start->z);
                if (distSq < closestDistSq) {
                    closestDistSq = distSq;
                    *outPos = intersect;
                    *outPoly = &dynaPolyList[i];
                    *outBgId = bgId;
                    hit = true;

                    *startSolid = false;
                    *endSolid = false;

                    // Extrude triangle along inverted normal (50 units)
                    Vec3f extrude[3];
                    extrude[0].x = v0.x - normal.x * 50.0f;
                    extrude[0].y = v0.y - normal.y * 50.0f;
                    extrude[0].z = v0.z - normal.z * 50.0f;
                    extrude[1].x = v1.x - normal.x * 50.0f;
                    extrude[1].y = v1.y - normal.y * 50.0f;
                    extrude[1].z = v1.z - normal.z * 50.0f;
                    extrude[2].x = v2.x - normal.x * 50.0f;
                    extrude[2].y = v2.y - normal.y * 50.0f;
                    extrude[2].z = v2.z - normal.z * 50.0f;

                    Vec3f tri[3] = { v0, v1, v2 };

                    // Check if start is inside the extruded prism
                    if (point_in_prism(start, tri, extrude)) {
                        *startSolid = true;
                    }

                    // Check if end is inside the extruded prism
                    if (point_in_prism(end, tri, extrude)) {
                        *endSolid = true;
                    }
                }
            }
        }
    }

    return hit;
}