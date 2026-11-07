#pragma once
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"

// 动画工具类：提供指数衰减插值和浮点比较
// 所有设置界面的按钮动画都基于此类的 SmoothLerp
class Anim
{
public:
    // 指数衰减插值：damping 越大速度越快，dt 为帧间隔
    template<typename T>
    static void SmoothLerp(T& cur, const T& target, float damping, float dt)
    {
        float t = 1.f - std::exp(-damping * dt);
        cur = ImLerp(cur, target, t);
    }

    static bool AlmostEqual(float a, float b, float eps = 0.001f)
    {
        return fabsf(a - b) < eps;
    }
    static bool AlmostEqual(const ImVec2& a, const ImVec2& b, float eps = 0.001f)
    {
        return AlmostEqual(a.x, b.x, eps) && AlmostEqual(a.y, b.y, eps);
    }
    static bool AlmostEqual(const ImVec4& a, const ImVec4& b, float eps = 0.001f)
    {
        return AlmostEqual(a.x, b.x, eps) && AlmostEqual(a.y, b.y, eps)
            && AlmostEqual(a.z, b.z, eps) && AlmostEqual(a.w, b.w, eps);
    }
};
