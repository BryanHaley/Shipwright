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

/* ---------- Position: GoldSrc -> OoT ---------- */
vec3_t pos_goldsrc_to_oot(vec3_t pg){
    vec3_t po;
    po.x = -pg.y;  // left -> -right
    po.y =  pg.z;  // up   -> up
    po.z =  pg.x;  // fwd  -> fwd
    return po;
}

/* ---------- GoldSrc AngleVectors (degrees in, vectors out) ----------
   GoldSrc convention (Quake heritage):
     yaw   around +Z
     pitch around +X
     roll  around +Y
   Forward/Right/Up are *GoldSrc* basis vectors.
*/
void goldsrc_angle_vectors(float pitch_deg, float yaw_deg, float roll_deg,
                                         vec3_t* fwd, vec3_t* right, vec3_t* up)
{
    float sp = sinf(rad(pitch_deg)), cp = cosf(rad(pitch_deg));
    float sy = sinf(rad(yaw_deg)),   cy = cosf(rad(yaw_deg));
    float sr = sinf(rad(roll_deg)),  cr = cosf(rad(roll_deg));

    // Forward
    fwd->x = cp * cy;
    fwd->y = cp * sy;
    fwd->z = -sp;

    // Right (matches HL/Quake handedness)
    right->x = -sr * sp * cy + cr * -sy;
    right->y = -sr * sp * sy + cr *  cy;
    right->z = -sr * cp;

    // Up
    up->x =  cr * sp * cy + sr * -sy;
    up->y =  cr * sp * sy + sr *  cy;
    up->z =  cr * cp;
}

/* ---------- Basis swap: GoldSrc -> OoT for any vector ---------- */
vec3_t v_goldsrc_to_oot(vec3_t vg){
    vec3_t vo;
    vo.x = -vg.y;
    vo.y =  vg.z;
    vo.z =  vg.x;
    return vo;
}

/* ---------- Build OoT rotation matrix from transformed basis ----------
   In OoT (right-handed, Y-up) we use columns = [Right, Up, Forward]
*/
mat3 oot_matrix_from_basis(vec3_t right, vec3_t up, vec3_t fwd){
    mat3 R;
    R.m[0][0] = right.x; R.m[1][0] = right.y; R.m[2][0] = right.z;
    R.m[0][1] =   up.x;  R.m[1][1] =   up.y;  R.m[2][1] =   up.z;
    R.m[0][2] =  fwd.x;  R.m[1][2] =  fwd.y;  R.m[2][2] =  fwd.z;
    return R;
}

/* ---------- Extract Y(aw)-X(itch)-Z(roll) from RH matrix ----------
   R = R_y(yaw) * R_x(pitch) * R_z(roll)
   Handles gimbal (|cos(pitch)| near 0) gracefully.
*/
void oot_euler_yxz_from_matrix(mat3 R, float* yaw_deg, float* pitch_deg, float* roll_deg){
    float sy = -R.m[2][1];                       // -Rzy
    float cy = sqrtf(fmaxf(0.0f, 1.0f - sy*sy));
    float yaw, pitch, roll;

    if (cy > 1e-6f){
        pitch = asinf(sy);                       // pitch = asin(-Rzy) w/ sign as defined above
        yaw   = atan2f(R.m[0][1], R.m[1][1]);    // atan2(Rxy, Ryy)
        roll  = atan2f(R.m[2][0], R.m[2][2]);    // atan2(Rzx, Rzz)
    }else{
        // Gimbal close: choose roll = 0, fold into yaw
        pitch = asinf(sy);
        yaw   = atan2f(-R.m[1][0], R.m[0][0]);
        roll  = 0.0f;
    }

    *yaw_deg   = deg(yaw);
    *pitch_deg = deg(pitch);
    *roll_deg  = deg(roll);
}

/* ---------- Full pose conversion ----------
   Input: GoldSrc pos (meters/units), angles in degrees: pitchX, yawZ, rollY
   Output: OoT pos (units), angles in degrees (yawY, pitchX, rollZ)
*/
void goldsrc_pose_to_oot(vec3_t pos_g,
                                       float pitch_g_deg, float yaw_g_deg, float roll_g_deg,
                                       vec3_t* pos_o, float* yaw_o_deg, float* pitch_o_deg, float* roll_o_deg)
{
    // Position
    *pos_o = pos_goldsrc_to_oot(pos_g);

    // Orientation:  GoldSrc -> basis vectors -> swap -> OoT matrix -> YXZ angles
    vec3_t fwd_g, right_g, up_g;
    goldsrc_angle_vectors(pitch_g_deg, yaw_g_deg, roll_g_deg, &fwd_g, &right_g, &up_g);

    vec3_t fwd_o   = v_goldsrc_to_oot(fwd_g);
    vec3_t right_o = v_goldsrc_to_oot(right_g);
    vec3_t up_o    = v_goldsrc_to_oot(up_g);

    // (Optional) Orthonormalize minor drift
    // Simple Gram-Schmidt
    float fn = 1.0f / fmaxf(1e-6f, sqrtf(fwd_o.x*fwd_o.x + fwd_o.y*fwd_o.y + fwd_o.z*fwd_o.z));
    fwd_o.x *= fn; fwd_o.y *= fn; fwd_o.z *= fn;
    // right' = normalize(right - dot(right,fwd)*fwd)
    float dotrf = right_o.x*fwd_o.x + right_o.y*fwd_o.y + right_o.z*fwd_o.z;
    right_o.x -= dotrf * fwd_o.x; right_o.y -= dotrf * fwd_o.y; right_o.z -= dotrf * fwd_o.z;
    float rn = 1.0f / fmaxf(1e-6f, sqrtf(right_o.x*right_o.x + right_o.y*right_o.y + right_o.z*right_o.z));
    right_o.x *= rn; right_o.y *= rn; right_o.z *= rn;
    // up = cross(fwd, right)
    up_o.x = fwd_o.y*right_o.z - fwd_o.z*right_o.y;
    up_o.y = fwd_o.z*right_o.x - fwd_o.x*right_o.z;
    up_o.z = fwd_o.x*right_o.y - fwd_o.y*right_o.x;

    mat3 R = oot_matrix_from_basis(right_o, up_o, fwd_o);
    oot_euler_yxz_from_matrix(R, yaw_o_deg, pitch_o_deg, roll_o_deg);
}

/* ---------- Helpers for OoT s16 angle units ----------
   0..65535 maps to 0..360 degrees (wraps).
*/
int16_t oot_deg_to_s16(float degrees){
    // Normalize to [0,360)
    float d = fmodf(degrees, 360.0f);
    if (d < 0) d += 360.0f;
    uint32_t u = (uint32_t)lrintf(d * (65536.0f / 360.0f)) & 0xFFFFu;
    return (int16_t)u; // OoT stores as s16, wraps naturally
}

/* ---------- Position: OoT -> GoldSrc ---------- */
vec3_t pos_oot_to_goldsrc(vec3_t po){
    vec3_t pg;
    pg.x =  po.z;   // fwd -> fwd
    pg.y = -po.x;   // right -> -left
    pg.z =  po.y;   // up -> up
    return pg;
}

/* ---------- Build OoT orientation matrix (Yaw-Pitch-Roll, YXZ) ---------- */
mat3 oot_matrix_from_euler(float yaw_deg, float pitch_deg, float roll_deg){
    float sy = sinf(rad(yaw_deg)),   cy = cosf(rad(yaw_deg));
    float sx = sinf(rad(pitch_deg)), cx = cosf(rad(pitch_deg));
    float sz = sinf(rad(roll_deg)),  cz = cosf(rad(roll_deg));

    mat3 R;
    // R = Ry * Rx * Rz
    R.m[0][0] = cy*cz + sy*sx*sz; R.m[0][1] = cy*(-sz) + sy*sx*cz; R.m[0][2] = sy*cx;
    R.m[1][0] =     cx*sz;        R.m[1][1] =      cx*cz;          R.m[1][2] =    -sx;
    R.m[2][0] = -sy*cz + cy*sx*sz;R.m[2][1] = sy*sz + cy*sx*cz;    R.m[2][2] = cy*cx;
    return R;
}

/* ---------- Basis swap: OoT -> GoldSrc for any vector ---------- */
vec3_t v_oot_to_goldsrc(vec3_t vo){
    vec3_t vg;
    vg.x =  vo.z;
    vg.y = -vo.x;
    vg.z =  vo.y;
    return vg;
}

/* ---------- Extract GoldSrc angles from basis vectors ----------
   GoldSrc expects (pitchX, yawZ, rollY).
   Forward, Right, Up basis is enough.
*/
void goldsrc_angles_from_basis(vec3_t fwd, vec3_t right, vec3_t up,
                                             float* pitch_deg, float* yaw_deg, float* roll_deg)
{
    // pitch: rotation around X axis
    *pitch_deg = -deg(asinf(fwd.z));  // fwd.z = -sin(pitch) in HL AngleVectors

    // yaw: rotation around Z axis
    *yaw_deg = deg(atan2f(fwd.y, fwd.x));

    // roll: rotation around Y axis
    // In HL, roll shifts right/up around forward; derive from right vector
    *roll_deg = deg(atan2f(-right.z, up.z));
}

/* ---------- Full pose conversion ----------
   Input: OoT pos (x,y,z), angles in degrees (yawY, pitchX, rollZ)
   Output: GoldSrc pos (x,y,z), angles in degrees (pitchX, yawZ, rollY)
*/
void oot_pose_to_goldsrc(vec3_t pos_o,
                                       float yaw_o_deg, float pitch_o_deg, float roll_o_deg,
                                       vec3_t* pos_g, float* pitch_g_deg, float* yaw_g_deg, float* roll_g_deg)
{
    // Position
    *pos_g = pos_oot_to_goldsrc(pos_o);

    // Orientation: OoT Euler -> matrix -> basis swap -> GoldSrc Euler
    mat3 Ro = oot_matrix_from_euler(yaw_o_deg, pitch_o_deg, roll_o_deg);

    // Basis vectors in OoT
    vec3_t right_o = { Ro.m[0][0], Ro.m[1][0], Ro.m[2][0] };
    vec3_t up_o    = { Ro.m[0][1], Ro.m[1][1], Ro.m[2][1] };
    vec3_t fwd_o   = { Ro.m[0][2], Ro.m[1][2], Ro.m[2][2] };

    // Convert basis vectors to GoldSrc
    vec3_t right_g = v_oot_to_goldsrc(right_o);
    vec3_t up_g    = v_oot_to_goldsrc(up_o);
    vec3_t fwd_g   = v_oot_to_goldsrc(fwd_o);

    goldsrc_angles_from_basis(fwd_g, right_g, up_g, pitch_g_deg, yaw_g_deg, roll_g_deg);
}