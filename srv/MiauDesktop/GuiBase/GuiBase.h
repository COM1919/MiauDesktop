#pragma once

// GUIBase.h
// 本项目自定义 GUI 控件库的中心头文件
// 所有自定义控件（主题 / 动画 / Card / 图表 / 基础控件 / 组件）的声明集中于此，
// 具体实现按分类拆分到 BaseUI/ 下的不同 .cpp 文件：
//   GUIBase.cpp       基础控件 + 纹理系统
//   GuiTheme.cpp      主题令牌系统
//   GuiAnimation.cpp  动画系统
//   GuiCard.cpp       Card 渲染
//   GuiCharts.cpp     图表组件
//   GuiControls.cpp   通用控件
// 其他文件只需 #include "BaseUI/GUIBase.h" 即可使用全部自定义控件。

#include "../Framework.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

// Windows.h 定义了 small 宏（展开为 char），与下方 GuiSpacingTokens 的
// small 字段名冲突，导致 "float 后面的 char 非法" 编译错误。取消该宏定义。
#ifdef small
#undef small
#endif

namespace RenderData
{
    inline ID3D11Device* g_pd3dDevice = nullptr;
    inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    inline IDXGISwapChain* g_pSwapChain = nullptr;
    inline bool g_SwapChainOccluded = false;
    inline UINT g_ResizeWidth = 0;
    inline UINT g_ResizeHeight = 0;
    inline ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

    // DirectComposition 资源（用于 per-pixel 半透明窗口）
    inline IDCompositionDevice* g_pDCompDevice = nullptr;
    inline IDCompositionTarget* g_pDCompTarget = nullptr;
    inline IDCompositionVisual* g_pDCompVisual = nullptr;
}

namespace GuiBase
{
    // =========================================================================
    // 主题令牌系统（GuiTheme.cpp）
    // =========================================================================

    // 颜色结构体：归一化 RGBA 浮点表示
    struct GuiColor
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        GuiColor() = default;

        GuiColor(float r_, float g_, float b_, float a_ = 1.0f)
            : r(r_), g(g_), b(b_), a(a_)
        {
        }

        // 转换为 ImGui 渲染所需的 ImU32 格式
        ImU32 ToImU32() const
        {
            return IM_COL32(
                (int)(std::clamp(r, 0.0f, 1.0f) * 255.0f),
                (int)(std::clamp(g, 0.0f, 1.0f) * 255.0f),
                (int)(std::clamp(b, 0.0f, 1.0f) * 255.0f),
                (int)(std::clamp(a, 0.0f, 1.0f) * 255.0f));
        }

        // 转换为 ImVec4 格式（用于 TextColored / PushStyleColor 等）
        ImVec4 ToImVec4() const
        {
            return ImVec4(
                std::clamp(r, 0.0f, 1.0f),
                std::clamp(g, 0.0f, 1.0f),
                std::clamp(b, 0.0f, 1.0f),
                std::clamp(a, 0.0f, 1.0f));
        }
    };

    // 颜色线性混合：result = base + (overlay - base) * amount
    inline GuiColor MixColor(const GuiColor& base, const GuiColor& overlay, float amount)
    {
        float t = std::clamp(amount, 0.0f, 1.0f);
        return GuiColor(
            base.r + (overlay.r - base.r) * t,
            base.g + (overlay.g - base.g) * t,
            base.b + (overlay.b - base.b) * t,
            base.a + (overlay.a - base.a) * t);
    }

    // 修改颜色的 alpha 通道（替换而非叠加）
    inline GuiColor WithAlpha(GuiColor color, float alpha)
    {
        color.a = std::clamp(alpha, 0.0f, 1.0f);
        return color;
    }

    // 字号令牌：从微小标注到英雄标题的完整梯度
    struct GuiTypographyTokens
    {
        float micro = 11.0f;
        float caption = 12.0f;
        float hint = 13.0f;
        float label = 14.0f;
        float option = 15.0f;
        float body = 16.0f;
        float input = 17.0f;
        float control = 18.0f;
        float cardTitle = 19.0f;
        float subtitle = 20.0f;
        float title = 22.0f;
        float heading = 24.0f;
        float headline = 27.0f;
        float displayCompact = 30.0f;
        float display = 31.0f;
        float hero = 42.0f;

        // 行高附加量：行高 = 字号 + lineGap
        float lineGapTight = 3.0f;
        float lineGap = 4.0f;
        float lineGapRelaxed = 5.0f;
    };

    // 间距令牌：从发丝级到覆盖层的完整梯度
    struct GuiSpacingTokens
    {
        float hairline = 1.0f;
        float micro = 2.0f;
        float tiny = 4.0f;
        float small = 6.0f;
        float compact = 8.0f;
        float control = 10.0f;
        float content = 12.0f;
        float section = 16.0f;
        float large = 20.0f;
        float panel = 24.0f;
        float header = 30.0f;
        float page = 40.0f;
        float overlay = 48.0f;
    };

    // 圆角令牌：full 表示完全圆形（胶囊 / 圆形元素使用）
    struct GuiRadiusTokens
    {
        float micro = 2.0f;
        float tiny = 4.0f;
        float small = 6.0f;
        float control = 8.0f;
        float tooltip = 9.0f;
        float popup = 10.0f;
        float card = 12.0f;
        float elevated = 14.0f;
        float overlay = 16.0f;
        float section = 18.0f;
        float feature = 22.0f;
        float full = 999.0f;
    };

    // 控件尺寸令牌：与 EUI-NEO ControlSizeTokens 对齐
    struct GuiControlSizeTokens
    {
        float progress = 14.0f;
        float indicator = 22.0f;
        float switchHeight = 24.0f;
        float compact = 28.0f;
        float menuItem = 34.0f;
        float field = 35.0f;
        float segmented = 36.0f;
        float input = 40.0f;
        float control = 44.0f;
        float large = 44.0f;
        float switchWidth = 46.0f;
        float navigation = 50.0f;
        float scrollbar = 8.0f;
    };

    // 阴影分级：从轻量按钮到辉光的五级阴影体系
    enum class GuiShadowLevel
    {
        Button,
        Chart,
        Panel,
        Dialog,
        Glow
    };

    // 阴影样式：模糊半径、偏移量与颜色
    struct GuiShadowStyle
    {
        float blur = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        GuiColor color;
    };

    // 根据阴影分级与主题模式获取阴影样式（offsetX 始终为 0）
    GuiShadowStyle GetShadowStyle(GuiShadowLevel level, bool isDark);

    // 主题令牌聚合：颜色 + 子令牌（排版 / 间距 / 圆角）
    struct GuiThemeTokens
    {
        GuiColor background;
        GuiColor primary;
        GuiColor surface;
        GuiColor surfaceHover;
        GuiColor surfaceActive;
        GuiColor text;
        GuiColor border;
        bool isDark = false;
        GuiTypographyTokens typography;
        GuiSpacingTokens spacing;
        GuiRadiusTokens radius;
        GuiControlSizeTokens control;
    };

    // 主题工厂：暗色 / 亮色配色（主色 #3871E0）
    namespace Theme
    {
        GuiThemeTokens Dark();
        GuiThemeTokens Light();
    }

    // =========================================================================
    // 动画系统（GuiAnimation.cpp，模板部分在本头文件中定义）
    // =========================================================================

    // 缓动曲线枚举
    enum class GuiEase
    {
        Linear,
        InQuad,
        OutQuad,
        InOutQuad,
        OutCubic,
        InOutCubic,
        OutExpo,
        OutBack
    };

    // 应用缓动曲线（t 已 clamp 到 [0,1]）
    float ApplyEase(GuiEase ease, float t);

    // 动画属性位掩码
    enum class GuiAnimProperty : uint32_t
    {
        None = 0,
        Frame = 1u << 0,
        Color = 1u << 1,
        TextColor = 1u << 2,
        Opacity = 1u << 3,
        Radius = 1u << 4,
        Border = 1u << 5,
        Shadow = 1u << 6,
        Blur = 1u << 7,
        Transform = 1u << 8,
        All = 0xFFFFFFFFu
    };

    inline GuiAnimProperty operator|(GuiAnimProperty left, GuiAnimProperty right)
    {
        return static_cast<GuiAnimProperty>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
    }

    inline GuiAnimProperty operator&(GuiAnimProperty left, GuiAnimProperty right)
    {
        return static_cast<GuiAnimProperty>(static_cast<uint32_t>(left) & static_cast<uint32_t>(right));
    }

    inline GuiAnimProperty& operator|=(GuiAnimProperty& left, GuiAnimProperty right)
    {
        left = left | right;
        return left;
    }

    // 判断掩码中是否包含指定属性
    inline bool HasAnimProperty(GuiAnimProperty mask, GuiAnimProperty property)
    {
        return (static_cast<uint32_t>(mask & property) != 0u);
    }

    // 过渡配置
    struct GuiTransition
    {
        bool enabled = false;
        float durationSeconds = 0.18f;
        float delaySeconds = 0.0f;
        GuiEase ease = GuiEase::OutCubic;
        GuiAnimProperty properties = GuiAnimProperty::All;

        // 返回默认过渡（不启用动画）
        static GuiTransition None()
        {
            return {};
        }

        // 返回启用的过渡
        static GuiTransition Make(float duration, GuiEase easing = GuiEase::OutCubic)
        {
            GuiTransition transition;
            transition.enabled = true;
            transition.durationSeconds = std::max(0.0f, duration);
            transition.ease = easing;
            return transition;
        }
    };

    // 全局过渡时长缩放（clamp 到 [0.05, 8.0]）
    float& GlobalTransitionDurationScaleStorage();
    void SetGlobalTransitionDurationScale(float value);
    float GetGlobalTransitionDurationScale();

    // 按全局缩放调整过渡的 duration 与 delay
    GuiTransition ScaledTransition(GuiTransition transition);

    // 插值与近似比较（重载在 GuiAnimation.cpp 中实现）
    float LerpValue(float from, float to, float amount);
    bool CloseEnough(float left, float right, float epsilon = 0.001f);
    ImVec2 LerpValue(const ImVec2& from, const ImVec2& to, float amount);
    bool CloseEnough(const ImVec2& left, const ImVec2& right, float epsilon = 0.001f);
    ImVec4 LerpValue(const ImVec4& from, const ImVec4& to, float amount);
    bool CloseEnough(const ImVec4& left, const ImVec4& right, float epsilon = 0.001f);

    // 平滑目标专用比较（float 版本 epsilon = 0.012）
    bool CloseEnoughSmoothTarget(float left, float right);

    // 平滑目标专用比较（非 float 类型委托给 CloseEnough）
    template <typename T>
    inline bool CloseEnoughSmoothTarget(const T& left, const T& right)
    {
        return CloseEnough(left, right);
    }

    // 基于 Transition 的属性动画
    template <typename T>
    class GuiAnimatedValue
    {
    public:
        // 设置目标值。返回 true 表示产生了状态变化。
        bool SetTarget(const T& target, const GuiTransition& transition, bool animateProperty = true)
        {
            // 首次初始化：直接吸附到目标
            if (!m_initialized)
            {
                m_initialized = true;
                m_start = target;
                m_current = target;
                m_target = target;
                m_active = false;
                return true;
            }

            // 目标与当前目标足够接近：无需变化
            if (CloseEnough(m_target, target))
            {
                return false;
            }

            m_target = target;

            // 过渡未启用 / 不动画该属性 / 时长非正：立即吸附
            if (!transition.enabled || !animateProperty || transition.durationSeconds <= 0.0f)
            {
                m_start = target;
                m_current = target;
                m_active = false;
                m_elapsedSeconds = 0.0f;
                return true;
            }

            // 启动动画
            m_start = m_current;
            m_spec = ScaledTransition(transition);
            m_elapsedSeconds = 0.0f;
            m_active = true;
            return true;
        }

        // 推进动画。返回 true 表示当前值发生了变化。
        bool Tick(float deltaSeconds)
        {
            if (!m_active)
            {
                return false;
            }

            const T previous = m_current;
            m_elapsedSeconds += std::max(0.0f, deltaSeconds);

            const float localTime = m_elapsedSeconds - m_spec.delaySeconds;
            if (localTime <= 0.0f)
            {
                return false;
            }

            const float duration = std::max(0.0001f, m_spec.durationSeconds);
            const float amount = ApplyEase(m_spec.ease, localTime / duration);
            m_current = LerpValue(m_start, m_target, amount);

            // 到达终点
            if (localTime >= duration || CloseEnough(m_current, m_target))
            {
                m_current = m_target;
                m_active = false;
            }

            return !CloseEnough(previous, m_current);
        }

        const T& Value() const
        {
            return m_current;
        }

        bool IsActive() const
        {
            return m_active;
        }

    private:
        bool m_initialized = false;
        bool m_active = false;
        float m_elapsedSeconds = 0.0f;
        GuiTransition m_spec;
        T m_start{};
        T m_current{};
        T m_target{};
    };

    // 基于指数平滑的跟随动画
    template <typename T>
    class GuiSmoothedValue
    {
    public:
        // 以指数平滑跟随目标。返回 true 表示当前值发生了变化。
        bool Update(const T& target, float speed, float deltaSeconds)
        {
            // 首次初始化：直接吸附到目标
            if (!m_initialized)
            {
                m_initialized = true;
                m_current = target;
                return true;
            }

            const T previous = m_current;

            // 已足够接近：吸附并返回比较结果
            if (CloseEnoughSmoothTarget(m_current, target))
            {
                m_current = target;
                return !CloseEnough(previous, m_current);
            }

            // 无时间或无速度：直接吸附
            if (deltaSeconds <= 0.0f || speed <= 0.0f)
            {
                m_current = target;
            }
            else
            {
                // 指数平滑：amount = 1 - exp(-min(speed, 32) * dt)
                const float amount = 1.0f - std::exp(-std::min(speed, 32.0f) * deltaSeconds);
                m_current = LerpValue(m_current, target, amount);
            }

            // 平滑后足够接近：吸附
            if (CloseEnoughSmoothTarget(m_current, target))
            {
                m_current = target;
            }

            return !CloseEnough(previous, m_current);
        }

        const T& Value() const
        {
            return m_current;
        }

        // 当前是否仍在向目标移动（未到达）
        bool IsMovingTo(const T& target) const
        {
            return !CloseEnoughSmoothTarget(m_current, target);
        }

    private:
        bool m_initialized = false;
        T m_current{};
    };

    // =========================================================================
    // 动画进度值（全局动画设置驱动，组件零配置）
    // 组件只需持有本类型一个成员，每帧调用 Advance(target) 得到动画进度：
    //   - 全局"启用动画"关闭 → 直接返回目标值（无动画）
    //   - 全局"优先公式" = Lerp → 指数平滑跟随
    //   - 否则 → GuiEase 缓动曲线（时长 = 0.6s / 全局速度）
    // 新组件扩展时无需关心动画实现细节，也无需写任何重复代码
    // =========================================================================
    class GuiAnimatedProgress
    {
    public:
        // 推进动画并返回当前动画值（每帧调用一次；target 为期望目标值）
        float Advance(float target)
        {
            if (!IsEnabled())
                return target;

            const float speed = GetSpeed();
            if (IsLerp())
            {
                m_smooth.Update(target, 12.0f * speed, ImGui::GetIO().DeltaTime);
                return m_smooth.Value();
            }

            GuiTransition tr = GuiTransition::Make(0.6f / std::max(0.1f, speed), GetEase());
            m_ease.SetTarget(target, tr);
            m_ease.Tick(ImGui::GetIO().DeltaTime);
            return m_ease.Value();
        }

        // 直接复位到指定值（不播放动画）
        // Update(speed=0, dt=0) 会让 GuiSmoothedValue 直接吸附到目标；GuiAnimatedValue 用 None 过渡立即吸附
        void Reset(float value)
        {
            m_smooth.Update(value, 0.0f, 0.0f);
            m_ease.SetTarget(value, GuiTransition::None());
            m_ease.Tick(1.0f);
        }

        // 注册全局动画设置的读取回调（应用层启动时调用一次）
        // 回调均来自 AppSettings（IsAnimationEnabled / GetAnimationSpeed / IsLerpPreferred / GetPreferredEase）
        using EnabledFn = bool(*)();
        using SpeedFn = float(*)();
        using LerpFn = bool(*)();
        using EaseFn = GuiEase(*)();
        static void SetAnimationSettingsProvider(EnabledFn enabledFn, SpeedFn speedFn, LerpFn lerpFn, EaseFn easeFn);

    private:
        // === 全局动画设置读取（来自 AppSettings）===
        // 通过函数指针间接调用，避免 GuiBase（基础库）反向依赖 AppSettings（应用层）
        static EnabledFn s_enabledFn;
        static SpeedFn s_speedFn;
        static LerpFn s_lerpFn;
        static EaseFn s_easeFn;

        static bool IsEnabled() { return s_enabledFn ? s_enabledFn() : true; }
        static float GetSpeed() { return s_speedFn ? s_speedFn() : 1.0f; }
        static bool IsLerp() { return s_lerpFn ? s_lerpFn() : false; }
        static GuiEase GetEase() { return s_easeFn ? s_easeFn() : GuiEase::OutCubic; }

        GuiSmoothedValue<float> m_smooth;
        GuiAnimatedValue<float> m_ease;
    };

    // =========================================================================
    // Card 渲染（GuiCard.cpp）
    // =========================================================================

    // Card 样式：圆角 / 内边距 / 背景色 / 边框 / 阴影 / 透明度
    struct GuiCardStyle
    {
        float radius = 18.0f;
        float padding = 20.0f;
        GuiColor backgroundColor;
        GuiColor borderColor;
        float borderWidth = 1.0f;
        float borderAlpha = 0.72f;
        GuiShadowStyle shadow;
        float opacity = 1.0f;

        GuiCardStyle();
        GuiCardStyle(const GuiThemeTokens& tokens);
    };

    // 绘制高斯软阴影：通过多层半透明圆角矩形叠加模拟高斯模糊
    void DrawGuiSoftShadow(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                           const GuiShadowStyle& shadow, float rounding);

    // 绘制 Card 背景：阴影 → 背景圆角矩形 → 边框描边（z-order 从底到顶）
    void DrawGuiCardBackground(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                               const GuiCardStyle& style);

    // 绘制渐变矩形：水平 / 垂直线性渐变（AddRectFilledMultiColor 不支持圆角）
    void DrawGuiGradientRect(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                             GuiColor colorStart, GuiColor colorEnd,
                             bool horizontal, float rounding);

    // 开始 Card 内容区：设置光标到内容区左上角，并裁剪到内容区内
    // 配合 EndGuiCardContent() 使用，在两者之间放置 ImGui 控件
    void BeginGuiCardContent(ImVec2 pos, ImVec2 size, float padding);

    // 结束 Card 内容区：恢复裁剪状态
    void EndGuiCardContent();

    // =========================================================================
    // 图表组件（GuiCharts.cpp）
    // =========================================================================

    // 图表统一样式：背景 / 文字 / 网格 / 圆角 / 阴影 / 调色板 / 字号
    struct GuiChartStyle
    {
        GuiColor backgroundColor;
        GuiColor textColor;
        GuiColor gridColor;
        float radius = 18.0f;
        GuiShadowStyle shadow;
        GuiColor palette[4];
        float titleFontSize = 22.0f;
        float labelFontSize = 14.0f;

        GuiChartStyle();
        GuiChartStyle(const GuiThemeTokens& tokens);
    };

    // 由图表样式构造 Card 样式：复用背景色 / 圆角 / 阴影
    GuiCardStyle ChartStyleToCardStyle(const GuiChartStyle& style);

    // 绘制柱状图：背景 + 标题 + 网格线 + 圆角柱子 + 标签
    void DrawGuiBarChart(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                         const float* values, int count,
                         const char* const* labels, const char* title,
                         const GuiChartStyle& style);

    // 折线线型
    enum class GuiLineStyle
    {
        Linear,
        Curve,
        Step
    };

    // 绘制折线图：背景 + 标题 + 网格线 + 折线 + 数据点
    void DrawGuiLineChart(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                          const float* values, int count,
                          const char* title,
                          const GuiChartStyle& style, GuiLineStyle lineStyle);

    // 绘制饼图：背景 + 扇形 + 外侧标签
    void DrawGuiPieChart(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                         const float* values, int count,
                         const char* const* labels,
                         const GuiChartStyle& style);

    // 绘制胶囊形进度条：track 底色 + fill 前景色
    void DrawGuiProgress(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                         float value, GuiColor fillColor, GuiColor trackColor);

    // =========================================================================
    // 基础控件 + 纹理系统（GUIBase.cpp）
    // =========================================================================

    // 标题文本
    void TitleText(const char* label);

    // 发光描边
    void DrawGlowBorder(ImVec2 min, ImVec2 max, ImU32 color, float thickness = 3.0f);

    // 弧形进度条（带动画插值）
    void ProgressBarArc(const char* label, float progress, ImVec2 size,
                        ImVec4 Color, ImVec4 bgColor, float thickness = 5.0f);

    // 圆形按钮（圆角大按钮）
    bool CircularButton(const char* label, ImVec2 size);

    // 纹理加载与缓存
    Texture2D LoadTexture(unsigned char* data, int& image_width, int& image_height,
                          int* w, int* h, const char* id);
    Texture2D LoadTextureFromFile(const std::string& filename, bool CacheDisabled = false,
                                  int* w = NULL, int* h = NULL);
    Texture2D LoadTextureFromMemory(const unsigned char* buffer, int len, const char* id,
                                    bool CacheDisabled = false, int* w = NULL, int* h = NULL);
    void ClearAllTextureCache();
    void UnloadTexture(const std::string& id);

    // =========================================================================
    // 通用控件（GuiControls.cpp）
    // 移植自 EUI-NEO components/button.h / slider.h / switch.h / checkbox.h /
    //           radio.h / segmented.h / stepper.h
    // =========================================================================

    // 按钮：EUI 三态色 + 阴影
    bool GuiButton(const char* label, const ImVec2& size = ImVec2(0, 0));

    // 胶囊按钮（EUI pill 风格，full 圆角）
    bool GuiPillButton(const char* label, const ImVec2& size = ImVec2(0, 0));

    // 滑块（浮点）
    bool GuiSliderFloat(const char* label, float* value, float vMin, float vMax,
                        const char* format = "%.2f");

    // 滑块（整数）
    bool GuiSliderInt(const char* label, int* value, int vMin, int vMax,
                      const char* format = "%d");

    // 开关
    bool GuiSwitch(const char* label, bool* value);

    // 复选框
    bool GuiCheckbox(const char* label, bool* value);

    // 单选
    bool GuiRadio(const char* label, bool* value);

    // 分段控件（返回 true 表示选中项变化）
    bool GuiSegmented(const char* label, const char* const* items, int itemCount,
                      int* selectedIndex);

    // 步进器（加减按钮 + 数值显示）
    bool GuiStepper(const char* label, int* value, int step = 1,
                    int minValue = 0, int maxValue = 100);

    // =========================================================================
    // 设置界面"行式"样式（GuiControls.cpp）
    // 设置窗口与组件 SettingsRender 共用：一行 = 左侧文字 + 右侧控件
    //（Toggle / 滑块 / 下拉），行背景 hover 平滑过渡，左右内边距 = style.padding × 2
    // =========================================================================

    // 设置行：Toggle 开关
    bool SettingsToggleRow(const char* label, bool* value);
    // 设置行：滑块
    bool SettingsSliderRow(const char* label, float* value, float vMin, float vMax,
                           const char* format = "%.2f");
    // 设置行：下拉选择
    bool SettingsDropdownRow(const char* label, const char* const* items, int itemCount,
                             int* selectedIndex);

    // =========================================================================
    // 导航与容器（GuiNavigation.cpp）
    // 移植自 EUI-NEO components/tabs.h / carousel.h / datatable.h
    // =========================================================================

    // 标签页（返回 true 表示选中页变化）
    bool GuiTabs(const char* label, const char* const* tabs, int tabCount,
                 int* selectedIndex);

    // 轮播（左右箭头切换，返回 true 表示选中项变化）
    bool GuiCarousel(const char* label, const char* const* items, int itemCount,
                     int* selectedIndex, float width, float height);

    // 数据表格（EUI datatable 风格：表头 + 行 + 交替行背景）
    void GuiDatatable(const char* label,
                      const char* const* headers, int headerCount,
                      const std::vector<std::vector<std::string>>& rows);

    // =========================================================================
    // 输入与选择（GuiInput.cpp）
    // 移植自 EUI-NEO components/input.h / dropdown.h / colorpicker.h /
    //           datepicker.h / timepicker.h
    // =========================================================================

    // 文本输入框
    bool GuiInputText(const char* label, std::string* value,
                      const ImVec2& size = ImVec2(0, 0));

    // 下拉选择（返回 true 表示选中项变化）
    bool GuiDropdown(const char* label, const char* const* items, int itemCount,
                     int* selectedIndex);

    // 颜色选择器（RGBA 浮点数组，返回 true 表示颜色变化）
    bool GuiColorPicker(const char* label, float color[4]);

    // 日期选择器（返回 true 表示日期变化）
    bool GuiDatePicker(const char* label, int* year, int* month, int* day);

    // 时间选择器（返回 true 表示时间变化）
    bool GuiTimePicker(const char* label, int* hour, int* minute);

    // =========================================================================
    // 浮层与反馈（GuiOverlay.cpp）
    // 移植自 EUI-NEO components/tooltip.h / dialog.h / contextmenu.h / toast.h
    // =========================================================================

    // 悬浮提示（在当前鼠标位置附近显示）
    void GuiTooltip(const char* text, const ImVec2& position = ImVec2(-1, -1));

    // 模态对话框（返回 true 表示被关闭）
    bool GuiDialog(const char* title, bool* open, const char* content);

    // 右键菜单（返回 true 表示选中某项）
    bool GuiContextMenu(const char* label, const char* const* items, int itemCount,
                        int* selectedIndex);

    // 轻提示（短暂显示后自动消失）
    void GuiToast(const char* text, float duration = 2.0f);

    // =========================================================================
    // 滚动与虚拟化（GuiScroll.cpp）
    // 移植自 EUI-NEO components/scroll.h / scrollview.h / virtuallist.h /
    //           virtualmasonry.h
    // =========================================================================

    // 滚动条样式：轨道 / 滑块三态色 / 圆角 / 粗细
    struct GuiScrollStyle
    {
        GuiColor track;
        GuiColor thumb;
        GuiColor thumbHover;
        GuiColor thumbPressed;
        float radius = 999.0f;
        float thickness = 8.0f;

        GuiScrollStyle();
        GuiScrollStyle(const GuiThemeTokens& tokens);
    };

    // 独立滚动条：offset 为滚动偏移（自动 clamp 到 [0, contentSize - viewportSize]）
    // size 为滚动条自身占位尺寸（宽/高其一为 0 时取 style.thickness）
    // 返回 true 表示 offset 本帧发生了变化
    bool GuiScrollBar(const char* label, float* offset, float viewportSize, float contentSize,
                      const ImVec2& size, bool horizontal = false,
                      const GuiScrollStyle& style = GuiScrollStyle());

    // 滚动视图：Begin 内可放任意 ImGui 控件，End 时按内容高度绘制滚动条并 clamp offset
    // offset 为调用方持有的滚动偏移；返回 false 表示内容不可见，此时仍需调用 End
    bool BeginGuiScrollView(const char* label, const ImVec2& size, float* offset,
                            const GuiScrollStyle& style = GuiScrollStyle(),
                            ImGuiWindowFlags flags = 0);
    void EndGuiScrollView();

    // 虚拟列表可见区间：first/last 为闭区间行索引，min/max 为视口屏幕矩形
    struct GuiVirtualRange
    {
        int first = 0;
        int last = -1;
        float offset = 0.0f;
        ImVec2 min;
        ImVec2 max;
    };

    // 固定行高虚拟列表：提交滚动/滚轮交互并返回可见区间（last < first 表示无可见行）
    GuiVirtualRange GuiVirtualList(const char* label, const ImVec2& size, int itemCount,
                                   float rowHeight, float* offset, float overscanRows = 2.0f,
                                   const GuiScrollStyle& style = GuiScrollStyle());

    // 虚拟列表行：绘制行背景 + 命中检测，返回是否被点击
    bool GuiVirtualListRow(const char* label, const GuiVirtualRange& range, int index,
                           float rowHeight, bool selected = false);

    // 瀑布流可见区间
    struct GuiMasonryRange
    {
        int first = 0;
        int last = -1;
        float offset = 0.0f;
        ImVec2 min;
        ImVec2 max;
    };

    // 多列虚拟瀑布流：heightOf(index, columnWidth) 返回该 cell 的高度
    // 列高前缀在 itemCount / columns / gap 变化时才重算，其余帧 O(可见项)
    GuiMasonryRange GuiVirtualMasonry(const char* label, const ImVec2& size, int itemCount,
                                      int columns, float gap, float* offset,
                                      const std::function<float(int, float)>& heightOf,
                                      float overscanRows = 1.0f,
                                      const GuiScrollStyle& style = GuiScrollStyle());

    // 瀑布流 cell：矩形取自上一次 GuiVirtualMasonry 的布局缓存，返回是否被点击
    bool GuiVirtualMasonryCell(const char* label, int index, bool selected = false);

    // =========================================================================
    // 容器与布局（GuiContainer.cpp）
    // 移植自 EUI-NEO components/navbar.h / sidebar.h / panel.h / layout_debug.h
    // =========================================================================

    // 导航项：图标 + 文字 + 值
    struct GuiNavbarItem
    {
        const char* label = nullptr;
        const char* icon = nullptr;
        int value = 0;
    };

    // 导航栏配置
    struct GuiNavbarOptions
    {
        float width = 0.0f;   // <=0 时取可用宽度
        float height = 64.0f;
        bool compact = false; // 紧凑模式：只显示图标
        const char* brandTitle = nullptr;
        const char* brandIcon = nullptr;
        const char* subtitle = nullptr;
        const char* footerText = nullptr;
        const char* footerIcon = nullptr;
        bool* footerClicked = nullptr; // 输出：footer 按钮本帧是否被点击
        bool dark = true;              // 取暗色 / 亮色主题令牌
    };

    // 顶部导航栏：返回 true 表示选中项变化（*selectedIndex 同步更新）
    bool GuiNavbar(const char* label, const GuiNavbarItem* items, int itemCount,
                   int* selectedIndex, const GuiNavbarOptions& options = GuiNavbarOptions());

    // 右侧抽屉配置
    struct GuiSidebarOptions
    {
        float width = 0.0f;  // <=0 时取可用宽度
        float height = 0.0f; // <=0 时取可用高度
        float drawerWidth = 320.0f;
        const char* title = nullptr;
        const char* eyebrow = nullptr;
        float duration = 0.24f; // 滑入/滑出时长（秒）
        bool dark = true;
    };

    // 右侧抽屉：content 为抽屉内部内容回调，返回 true 表示 open 被本帧改变
    bool GuiSidebar(const char* label, bool* open, const GuiSidebarOptions& options = GuiSidebarOptions(),
                    const std::function<void()>& content = nullptr);

    // 面板容器：绘制 Card 背景并把光标/裁剪推进到内容区（与 EndGuiPanel 配对）
    bool BeginGuiPanel(const char* label, const ImVec2& size,
                       const GuiCardStyle& style = GuiCardStyle());
    void EndGuiPanel();

    // 布局调试样式：外框 / 内边距 / 内容区 / 标签
    struct GuiLayoutDebugStyle
    {
        GuiColor frame;
        GuiColor padding;
        GuiColor content;
        GuiColor label;
        float stroke = 1.0f;
        float labelFontSize = 12.0f;
        float labelLineHeight = 16.0f;
        float labelX = 8.0f;
        float labelY = 6.0f;
        float labelHeight = 18.0f;

        GuiLayoutDebugStyle();
        GuiLayoutDebugStyle(const GuiThemeTokens& tokens);
    };

    // 布局调试覆盖层：外框 + inset 内边距 + 内容区描边 + 尺寸标签
    void GuiLayoutDebugOverlay(const char* label, const ImVec2& size, float inset,
                               const char* text = nullptr,
                               const GuiLayoutDebugStyle& style = GuiLayoutDebugStyle());

    // =========================================================================
    // 基础元素（GuiElement.cpp）
    // 移植自 EUI-NEO components/text.h / image.h / mousearea.h
    // =========================================================================

    // 水平对齐
    enum class GuiTextAlign
    {
        Left,
        Center,
        Right
    };

    // 文本绘制：fontSize<=0 用 ImGui 当前字号；maxWidth>0 时按像素换行
    // color.a<=0 时取主题 text 色；返回实际占用尺寸，光标推进到其下方
    ImVec2 GuiText(const char* label, const char* text, float fontSize = 0.0f,
                   GuiColor color = GuiColor(), GuiTextAlign align = GuiTextAlign::Left,
                   float maxWidth = 0.0f, float lineHeight = 0.0f);

    // 文本测量（不绘制）：maxWidth>0 时返回换行后的实际宽高
    ImVec2 GuiTextMeasure(const char* text, float fontSize = 0.0f,
                          float maxWidth = 0.0f, float lineHeight = 0.0f);

    // 图片：纹理 + UV + tint + 透明度 + 圆角裁剪（texture 为空时不绘制）
    void GuiImage(const char* label, Texture2D texture, const ImVec2& size,
                  GuiColor tint = GuiColor(), float rounding = 0.0f, float opacity = 1.0f,
                  const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f));

    // 鼠标区域配置
    struct GuiMouseAreaOptions
    {
        bool disabled = false;
        float rounding = 0.0f;
        GuiColor color; // alpha<=0 时不绘制背景
        ImGuiMouseCursor cursor = ImGuiMouseCursor_Arrow;
        const char* tooltip = nullptr;
    };

    // 鼠标区域原语：hover 时设置鼠标指针，可选背景与 tooltip，返回是否被点击
    bool GuiMouseArea(const char* label, const ImVec2& size,
                      const GuiMouseAreaOptions& options = GuiMouseAreaOptions(),
                      const std::function<void()>& onClick = nullptr);

    // =========================================================================
    // workshop 实验组件（GuiWorkshop.cpp）
    // 移植自 EUI-NEO components/workshop/heart_switch.h / neumorphic_button.h /
    //           tilt_card.h / card_slider.h
    // =========================================================================

    // 心形点赞开关：点击切换，按下缩放回弹 + 颜色过渡，返回 true 表示状态变化
    bool GuiHeartSwitch(const char* label, bool* value, const ImVec2& size = ImVec2(36.0f, 36.0f));

    // 新拟态按钮：亮/暗双阴影凸起，按下转为双内阴影并缩放
    bool GuiNeumorphicButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));

    // 3D 倾斜卡片：鼠标在卡片上移动时按透视偏移四角并叠加高光
    bool GuiTiltCard(const char* label, const ImVec2& size, const char* title,
                     const char* subtitle = nullptr,
                     const std::function<void()>& content = nullptr);

    // 卡片轮播：textures 可为 nullptr 项（无图时用 surface 底色），返回选中项是否变化
    bool GuiCardSlider(const char* label, const Texture2D* textures, const char* const* titles,
                       const char* const* subtitles, int count, int* selectedIndex,
                       const ImVec2& size);
}

// =============================================================================
// D3D11 + DirectComposition 初始化（GUIBase.cpp）
// =============================================================================

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
bool InitDirectComposition(IDXGIDevice* dxgiDevice, HWND hWnd);
