// ============================================================
// core/imu.h — Arcadia IMU 管理器
// 互补滤波 · pitch/roll 输出
// 提取自 threebody 和 relativity 的 IMU 代码
// ============================================================
#pragma once
#include <cstdint>
#include <cmath>

struct ArcIMU {
    float pitch = -0.3f;   // 俯仰 (绕X轴)
    float roll  = 0.0f;    // 横滚 (绕Y轴)
    float yaw   = 0.0f;    // 偏航 (绕Z轴, 需要磁力计支持)
    uint32_t lastTime = 0;
    float tau = 1.0f;      // 互补滤波时间常数
    bool init = false;
    bool hasImu = false;

    // ===================== 初始化 =====================
    void begin() {
        hasImu = M5.Imu.begin();
        lastTime = millis();
        init = true;
    }

    // ===================== 每帧更新 =====================
    void update() {
        if (!init || !hasImu) return;

        M5.Imu.update();
        float ax, ay, az, gx, gy, gz;
        M5.Imu.getAccel(&ax, &ay, &az);
        M5.Imu.getGyro(&gx, &gy, &gz);

        // 陀螺仪积分 (角度增量)
        float gxr = gx * DEG_TO_RAD;
        float gyr = gy * DEG_TO_RAD;
        float gzr = gz * DEG_TO_RAD;

        uint32_t now = millis();
        float dt = (now - lastTime) / 1000.0f;
        if (dt <= 0.0f)   dt = 0.01f;
        if (dt > 0.1f)    dt = 0.01f;  // 限制帧间隔
        lastTime = now;

        pitch += gxr * dt;
        roll  += gyr * dt;
        yaw   += gzr * dt;

        // 加速度计修正 (互补滤波)
        float am = sqrtf(ax*ax + ay*ay + az*az);
        if (am > 0.001f) {
            float ap = atan2f(ay, az);                     // 加速度计估算的 pitch
            float ar = atan2f(-ax, sqrtf(ay*ay + az*az));  // 加速度计估算的 roll

            // 只有当加速度幅值接近 1g 时才做修正 (避免运动干扰)
            if (am > 0.8f && am < 1.2f) {
                float a = dt / (tau + dt);
                if (a > 1.0f) a = 1.0f;

                float dp = ap - pitch;
                while (dp > M_PI)  dp -= 2 * M_PI;
                while (dp < -M_PI) dp += 2 * M_PI;

                float dr = ar - roll;
                while (dr > M_PI)  dr -= 2 * M_PI;
                while (dr < -M_PI) dr += 2 * M_PI;

                pitch += dp * a;
                roll  += dr * a;
            }
        }
    }

    // ===================== 获取姿态 =====================
    void getAttitude(float& outPitch, float& outRoll, float& outYaw) const {
        outPitch = pitch;
        outRoll  = roll;
        outYaw   = yaw;
    }

    void getAttitude(float& outPitch, float& outRoll) const {
        outPitch = pitch;
        outRoll  = roll;
    }

    // ===================== 重置为零 =====================
    void reset() {
        pitch = 0;
        roll  = 0;
        yaw   = 0;
    }

    // ===================== 检测是否可用 =====================
    bool available() const { return init && hasImu; }
};
