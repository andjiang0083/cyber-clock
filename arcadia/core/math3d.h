// ============================================================
// core/math3d.h — Arcadia 3D 数学引擎
// 向量 · 矩阵 · 旋转 · 投影
// 提取自 threebody/render_helpers.h
// ============================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

// ===================== 3D 向量 =====================
struct V3 {
    float x, y, z;
};

// ===================== 3×3 矩阵 =====================
using M3 = float[3][3];

// ===================== 旋转矩阵 =====================
static inline void rotX(M3 m, float a) {
    float c = cosf(a), s = sinf(a);
    m[0][0]=1; m[0][1]=0; m[0][2]=0;
    m[1][0]=0; m[1][1]=c; m[1][2]=-s;
    m[2][0]=0; m[2][1]=s; m[2][2]=c;
}

static inline void rotY(M3 m, float a) {
    float c = cosf(a), s = sinf(a);
    m[0][0]=c; m[0][1]=0; m[0][2]=s;
    m[1][0]=0; m[1][1]=1; m[1][2]=0;
    m[2][0]=-s; m[2][1]=0; m[2][2]=c;
}

static inline void rotZ(M3 m, float a) {
    float c = cosf(a), s = sinf(a);
    m[0][0]=c;  m[0][1]=-s; m[0][2]=0;
    m[1][0]=s;  m[1][1]=c;  m[1][2]=0;
    m[2][0]=0;  m[2][1]=0;  m[2][2]=1;
}

// ===================== 矩阵乘法 =====================
static inline void mulM(const M3 A, const M3 B, M3 R) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            R[i][j] = A[i][0]*B[0][j] + A[i][1]*B[1][j] + A[i][2]*B[2][j];
}

// ===================== 矩阵×向量 =====================
static inline V3 mv(const M3 m, V3 v) {
    return {
        m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z,
        m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z,
        m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z
    };
}

// ===================== 矩阵转置 =====================
static inline void transposeM(M3 m) {
    for (int i = 0; i < 3; i++)
        for (int j = i+1; j < 3; j++) {
            float t = m[i][j]; m[i][j] = m[j][i]; m[j][i] = t;
        }
}

// ===================== 向量运算 =====================
static inline V3 v3add(V3 a, V3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline V3 v3sub(V3 a, V3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static inline V3 v3scale(V3 v, float s) { return {v.x*s, v.y*s, v.z*s}; }
static inline float v3dot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float v3len2(V3 v) { return v.x*v.x + v.y*v.y + v.z*v.z; }
static inline float v3len(V3 v) { return sqrtf(v3len2(v)); }
static inline V3 v3norm(V3 v) { float l = v3len(v); return l>0 ? V3{v.x/l, v.y/l, v.z/l} : V3{0,0,0}; }
static inline V3 v3cross(V3 a, V3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

// ===================== 3D 投影 =====================
struct Pr { int x, y; float depth; };

// 投影: 世界坐标 → 屏幕坐标
// cx, cy: 屏幕中心; dist: 相机距离; scale: PROJ_SCALE * zoom
static inline Pr project(const M3 RT, V3 p, float cx, float cy, float dist, float scale) {
    V3 vr = mv(RT, p);
    float zd = vr.z + dist;
    if (zd < 0.3f) return {-32768, 0, 0};
    return {
        (int)(cx + vr.x * scale / zd),
        (int)(cy - vr.y * scale / zd),
        vr.z  // 深度信息
    };
}

// ===================== 构建相机矩阵 =====================
// 组合多个旋转: autoRot * swayY * swayX * roll * pitch
static inline void buildCamMatrix(M3 RT, float pitch, float roll,
                                  float autoAng, float swayX, float swayY) {
    M3 Rx, Ry, Rsx, Rsy, Rauto, Rtmp, Rtmp2;
    rotX(Rx, pitch);
    rotY(Ry, roll);
    rotX(Rsx, swayX);
    rotY(Rsy, swayY);
    rotY(Rauto, autoAng);

    // Rauto * swayY * swayX * roll * pitch
    mulM(Ry, Rx, Rtmp);
    mulM(Rsx, Rtmp, Rtmp2);
    mulM(Rsy, Rtmp2, Rtmp);
    mulM(Rauto, Rtmp, RT);

    // 转置 → 世界→相机空间
    transposeM(RT);
}
