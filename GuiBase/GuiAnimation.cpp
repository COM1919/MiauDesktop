// GuiAnimation.cpp
// 动画系统的实现（声明见 BaseUI/GUIBase.h，模板部分在头文件中）
// 移植自 EUI-NEO core/animation.h，适配 ImGui 类型（ImVec2 / ImVec4）

#include "GUIBase.h"

namespace GuiBase
{
    // 应用缓动曲线（t 已 clamp 到 [0,1]）
    float ApplyEase(GuiEase ease, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (ease)
        {
        case GuiEase::Linear:
            return t;
        case GuiEase::InQuad:
            return t * t;
        case GuiEase::OutQuad:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case GuiEase::InOutQuad:
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
        case GuiEase::OutCubic:
            return 1.0f - std::pow(1.0f - t, 3.0f);
        case GuiEase::InOutCubic:
            return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
        case GuiEase::OutExpo:
            return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -14.0f * t);
        case GuiEase::OutBack:
        {
            // c1 = 1.70158, c3 = c1 + 1 = 2.70158
            constexpr float c1 = 1.70158f;
            constexpr float c3 = 2.70158f;
            return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
        }
        }
        return t;
    }

    // 全局速度缩放（clamp 到 [0.05, 8.0]）
    float& GlobalTransitionDurationScaleStorage()
    {
        static float value = 1.0f;
        return value;
    }

    void SetGlobalTransitionDurationScale(float value)
    {
        GlobalTransitionDurationScaleStorage() = std::clamp(value, 0.05f, 8.0f);
    }

    float GetGlobalTransitionDurationScale()
    {
        return GlobalTransitionDurationScaleStorage();
    }

    GuiTransition ScaledTransition(GuiTransition transition)
    {
        const float scale = GetGlobalTransitionDurationScale();
        transition.durationSeconds *= scale;
        transition.delaySeconds *= scale;
        return transition;
    }

    // ---------- float 插值与比较 ----------
    float LerpValue(float from, float to, float amount)
    {
        return from + (to - from) * amount;
    }

    bool CloseEnough(float left, float right, float epsilon)
    {
        return std::fabs(left - right) <= epsilon;
    }

    // ---------- ImVec2 插值与比较 ----------
    ImVec2 LerpValue(const ImVec2& from, const ImVec2& to, float amount)
    {
        return ImVec2(LerpValue(from.x, to.x, amount), LerpValue(from.y, to.y, amount));
    }

    bool CloseEnough(const ImVec2& left, const ImVec2& right, float epsilon)
    {
        return CloseEnough(left.x, right.x, epsilon) && CloseEnough(left.y, right.y, epsilon);
    }

    // ---------- ImVec4 插值与比较（颜色插值，各通道独立） ----------
    ImVec4 LerpValue(const ImVec4& from, const ImVec4& to, float amount)
    {
        return ImVec4(
            LerpValue(from.x, to.x, amount),
            LerpValue(from.y, to.y, amount),
            LerpValue(from.z, to.z, amount),
            LerpValue(from.w, to.w, amount));
    }

    bool CloseEnough(const ImVec4& left, const ImVec4& right, float epsilon)
    {
        return CloseEnough(left.x, right.x, epsilon)
            && CloseEnough(left.y, right.y, epsilon)
            && CloseEnough(left.z, right.z, epsilon)
            && CloseEnough(left.w, right.w, epsilon);
    }

    // 平滑目标专用比较（float 版本 epsilon = 0.012）
    bool CloseEnoughSmoothTarget(float left, float right)
    {
        return CloseEnough(left, right, 0.012f);
    }

    // =========================================================================
    // GuiAnimatedProgress 的全局动画设置 Provider（函数指针定义）
    // 由应用层（AppSettings）在启动时调用 SetAnimationSettingsProvider 注册，
    // 使 GuiBase 基础库不反向依赖 AppSettings（保持依赖方向：应用层 → 基础库）
    // =========================================================================
    GuiAnimatedProgress::EnabledFn GuiAnimatedProgress::s_enabledFn = nullptr;
    GuiAnimatedProgress::SpeedFn   GuiAnimatedProgress::s_speedFn = nullptr;
    GuiAnimatedProgress::LerpFn    GuiAnimatedProgress::s_lerpFn = nullptr;
    GuiAnimatedProgress::EaseFn    GuiAnimatedProgress::s_easeFn = nullptr;

    void GuiAnimatedProgress::SetAnimationSettingsProvider(EnabledFn enabledFn, SpeedFn speedFn, LerpFn lerpFn, EaseFn easeFn)
    {
        s_enabledFn = enabledFn;
        s_speedFn = speedFn;
        s_lerpFn = lerpFn;
        s_easeFn = easeFn;
    }
}
