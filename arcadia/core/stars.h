// ============================================================
// core/stars.h — Arcadia 3D 星场系统
// 多层星场 · 星云 · 远景深空 · 渲染
// 提取自 threebody 项目的改进版星场
// ============================================================
#pragma once
#include "math3d.h"
#include "color.h"

// ===================== 星点 =====================
struct Star {
    V3 pos;
    uint16_t col;
    uint8_t sz;  // 1-7 大小等级
};

// ===================== 星场配置 =====================
// 满足 2.5KB 内即可
static const int ARC_STARS_MAIN   = 250;   // 主星场
static const int ARC_STARS_DEEP   = 100;   // 远景深空
static const int ARC_NEB_CLUSTERS = 6;     // 星云团数
static const int ARC_NEB_POINTS   = 30;    // 每团点数

// ===================== 生成主星场 =====================
// 两层: 弥散星 + 星团
static inline void arcInitStars(Star* stars, int n) {
    int idx = 0;

    // 层1: 弥散彩星
    for (int i = 0; i < 120 && idx < n; i++, idx++) {
        float r = 4 + frnd() * 4;
        float th = frnd() * 2 * M_PI;
        float ph = acosf(frnd() * 2 - 1);
        uint16_t col;
        uint8_t sz = 1 + (uint8_t)(frnd() * 5);
        float t = frnd();
        if (t < 0.12f)      col = c565(255, 220, 120);  // 黄
        else if (t < 0.25f) col = c565(255, 160, 80);   // 橙
        else if (t < 0.38f) col = c565(160, 200, 255);  // 蓝白
        else if (t < 0.48f) col = c565(255, 140, 140);  // 红
        else if (t < 0.62f) col = c565(200, 230, 255);  // 冷白
        else                col = c565(255, 255, 250);   // 纯白
        stars[idx] = {{r*sinf(ph)*cosf(th), r*cosf(ph), r*sinf(ph)*sinf(th)}, col, sz};
    }

    // 层2: 星团
    for (int cl = 0; cl < 8 && idx < n; cl++) {
        float cx = (frnd()-0.5f)*5, cy = (frnd()-0.5f)*5, cz = (frnd()-0.5f)*5;
        float len = sqrtf(cx*cx+cy*cy+cz*cz);
        if (len < 0.1f) len = 0.1f;
        float tr = 2 + frnd() * 4;
        cx = cx/len*tr; cy = cy/len*tr; cz = cz/len*tr;
        int ns = 8 + (int)(frnd() * 14);
        uint16_t ccol;
        float ct = frnd();
        if (ct < 0.34f)      ccol = c565(200, 230, 255);
        else if (ct < 0.67f) ccol = c565(255, 240, 210);
        else                 ccol = c565(220, 200, 255);
        for (int s = 0; s < ns && idx < n; s++, idx++) {
            float sp = 0.15f + frnd() * 0.6f;
            stars[idx] = {{cx+(frnd()-0.5f)*sp, cy+(frnd()-0.5f)*sp, cz+(frnd()-0.5f)*sp},
                          ccol, (uint8_t)(2+frnd()*5)};
        }
    }

    // 填充剩余
    while (idx < n) {
        float r = 2 + frnd() * 5;
        float th = frnd() * 2 * M_PI;
        float ph = acosf(frnd() * 2 - 1);
        stars[idx] = {{r*sinf(ph)*cosf(th), r*cosf(ph), r*sinf(ph)*sinf(th)},
                       c565(160, 190, 230), (uint8_t)(1+frnd()*3)};
        idx++;
    }
}

// ===================== 生成远景深空星 =====================
// z=20-50, 暗淡, 单像素
static inline void arcInitDeepStars(Star* stars, int n) {
    for (int i = 0; i < n; i++) {
        float r = 20 + frnd() * 30;
        float th = frnd() * 2 * M_PI;
        float ph = acosf(frnd() * 2 - 1);
        uint16_t col;
        float t = frnd();
        if (t < 0.15f)      col = c565(60,  70,  120);  // 深蓝
        else if (t < 0.30f) col = c565(80,  50,  100);  // 紫
        else if (t < 0.45f) col = c565(40,  60,  80);   // 冷灰
        else if (t < 0.60f) col = c565(90,  90,  100);  // 灰白
        else if (t < 0.75f) col = c565(120, 100, 80);   // 暖灰
        else                col = c565(50,  50,  60);    // 暗灰
        stars[i] = {{r*sinf(ph)*cosf(th), r*cosf(ph), r*sinf(ph)*sinf(th)}, col, 1};
    }
}

// ===================== 生成星云 =====================
// 返回星云数组, 每个星云是一组点
// nebulae[n][p]: n=团索引, p=点索引
template<int N_CLUSTERS, int N_POINTS>
struct NebulaField {
    float x[N_CLUSTERS][N_POINTS];
    float y[N_CLUSTERS][N_POINTS];
    float z[N_CLUSTERS][N_POINTS];
    uint16_t col[N_CLUSTERS][N_POINTS];

    void init() {
        static const uint16_t hues[N_CLUSTERS] = {
            c565(40,  10,  60),   // 紫
            c565(10,  20,  60),   // 蓝
            c565(60,  30,  10),   // 橙
            c565(8,   50,  40),   // 青
            c565(50,  10,  30),   // 玫红
            c565(30,  40,  50),   // 灰蓝
        };
        for (int n = 0; n < N_CLUSTERS; n++) {
            float nx = (frnd()-0.5f) * 10.0f;
            float ny = (frnd()-0.5f) * 8.0f;
            float nz = (frnd()-0.5f) * 10.0f;
            float r  = 0.6f + frnd() * 1.2f;
            for (int p = 0; p < N_POINTS; p++) {
                float spread = r * sqrtf(frnd());
                float th = frnd() * 2 * M_PI;
                float ph = acosf(frnd() * 2 - 1);
                x[n][p] = nx + spread * sinf(ph) * cosf(th);
                y[n][p] = ny + spread * cosf(ph);
                z[n][p] = nz + spread * sinf(ph) * sinf(th);
                float bright = 0.3f + frnd() * 0.5f;
                col[n][p] = blend565(TFT_BLACK, hues[n], bright);
            }
        }
    }
};

// ===================== 渲染星场 =====================
// drawPixel: (int x, int y, uint16_t color) → void
// comX/Y/Z: 质心偏移（相机跟踪目标）
template<typename DrawFn>
static inline void arcDrawStars(const Star* stars, int n, const M3 RT,
                                float cx, float cy, float dist, float scale,
                                float comX, float comY, float comZ,
                                int screenW, int screenH, DrawFn drawPixel) {
    for (int i = 0; i < n; i++) {
        V3 pos = {stars[i].pos.x - comX, stars[i].pos.y - comY, stars[i].pos.z - comZ};
        V3 vr = mv(RT, pos);
        float zd = vr.z + dist;
        if (zd < 0.3f || zd > 12.0f) continue;
        int sx = (int)(cx + vr.x * scale / zd);
        int sy = (int)(cy - vr.y * scale / zd);
        if (sx < -5 || sx >= screenW+5 || sy < -5 || sy >= screenH+5) continue;

        int sz = stars[i].sz;
        if (sz >= 6) {
            drawPixel(sx, sy, c565(255, 255, 255));
            drawPixel(sx+1, sy, stars[i].col);
        } else if (sz >= 3) {
            drawPixel(sx, sy, stars[i].col);
        } else {
            drawPixel(sx, sy, stars[i].col);
        }
    }
}

// ===================== 渲染星云 =====================
template<int NC, int NP, typename DrawFn>
static inline void arcDrawNebula(const NebulaField<NC,NP>& neb, const M3 RT,
                                 float cx, float cy, float dist, float scale,
                                 float comX, float comY, float comZ,
                                 int screenW, int screenH, DrawFn drawPixel) {
    for (int n = 0; n < NC; n++) {
        for (int p = 0; p < NP; p++) {
            V3 v = {neb.x[n][p] - comX, neb.y[n][p] - comY, neb.z[n][p] - comZ};
            V3 vr = mv(RT, v);
            float zd = vr.z + dist;
            if (zd < 0.3f || zd > 18.0f) continue;
            int sx = (int)(cx + vr.x * scale / zd);
            int sy = (int)(cy - vr.y * scale / zd);
            if (sx < 0 || sx >= screenW || sy < 0 || sy >= screenH) continue;
            drawPixel(sx, sy, neb.col[n][p]);
        }
    }
}

// ===================== 渲染远景深空 =====================
// 视差系数 pfac: 只跟随质心的 8% 移动
template<typename DrawFn>
static inline void arcDrawDeepStars(const Star* stars, int n, const M3 RT,
                                    float cx, float cy, float dist, float scale,
                                    float comX, float comY, float comZ,
                                    int screenW, int screenH, DrawFn drawPixel,
                                    float pfac = 0.08f) {
    for (int i = 0; i < n; i++) {
        V3 pos = {stars[i].pos.x - comX * pfac,
                  stars[i].pos.y - comY * pfac,
                  stars[i].pos.z - comZ * pfac};
        V3 vr = mv(RT, pos);
        float zd = vr.z + dist;
        if (zd < 0.3f || zd > 60.0f) continue;
        int sx = (int)(cx + vr.x * scale / zd);
        int sy = (int)(cy - vr.y * scale / zd);
        if (sx < 0 || sx >= screenW || sy < 0 || sy >= screenH) continue;
        drawPixel(sx, sy, stars[i].col);
    }
}
