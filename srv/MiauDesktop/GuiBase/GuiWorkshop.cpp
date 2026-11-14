// GuiWorkshop.cpp
// workshop 实验组件的实现（声明见 GuiBase.h）：心形点赞、新拟态按钮、
// 3D 倾斜卡片、卡片堆叠轮播。四个组件统一用 InvisibleButton 承担命中、
// 用 ImDrawList 自绘外观，跨帧动画状态放进 ImGuiStorage。
// 绘制路径零堆分配：心形/多边形顶点走栈上固定数组，文本按指针区间绘制。
// 移植自 EUI-NEO components/workshop/heart_switch.h / neumorphic_button.h /
//           tilt_card.h / card_slider.h

#include "GUIBase.h"

#include <cfloat>

namespace GuiBase
{
    namespace
    {
        // =========================================================================
        // 私有辅助（平滑 / 心形轮廓 / 内阴影）
        // =========================================================================

        // 与帧率无关的指数平滑（dt 上限避免掉帧时跳变）
        float SmoothTowards(float current, float target, float speed)
        {
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
            const float amount = 1.0f - std::exp(-speed * dt);
            return current + (target - current) * amount;
        }

        // 拆出 "##" 之前的显示文本（"##" 之后仅作实例 ID），零分配
        void SplitVisibleLabel(const char* label, const char** outBegin, int* outLength)
        {
            const char* begin = (label != nullptr) ? label : "";
            int length = 0;
            while (begin[length] != '\0' && !(begin[length] == '#' && begin[length + 1] == '#'))
            {
                ++length;
            }
            *outBegin = begin;
            *outLength = length;
        }

        // 按像素字号居中绘制单行文本
        void DrawCenteredText(ImDrawList* drawList, ImFont* font, float fontSize, const char* text,
                              ImVec2 min, ImVec2 max, ImU32 color)
        {
            if (drawList == nullptr || font == nullptr || text == nullptr || text[0] == '\0')
            {
                return;
            }
            const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            drawList->AddText(font, fontSize,
                              ImVec2(min.x + (max.x - min.x - textSize.x) * 0.5f,
                                     min.y + (max.y - min.y - textSize.y) * 0.5f),
                              color, text);
        }

        // =========================================================================
        // 心形轮廓（heart_switch.h:196-198 的 FontAwesome heart 实心路径，24×24 设计空间）
        // 顶部凹口 (12, 5.217)、双侧穹顶 (17.5/6.5, 1.917)、双肩 (24/0, 8.967)、
        // 底部尖端 (12, 21.847)；用 6 段三次贝塞尔逼近，首点为顶部凹口。
        // =========================================================================
        constexpr int kHeartSegments = 6;
        constexpr int kHeartStepsPerSegment = 5;
        constexpr int kHeartMaxPoints = kHeartSegments * kHeartStepsPerSegment + 2;
        constexpr float kHeartCenterX = 12.0f;
        constexpr float kHeartCenterY = 11.882f; // (1.917 + 21.847) / 2
        constexpr float kHeartNotchY = 5.217f;   // 顶部凹口

        // 每行 = { c1x, c1y, c2x, c2y, 终点 x, 终点 y }
        constexpr float kHeartBezier[kHeartSegments][6] = {
            {13.800f, 2.600f, 15.800f, 1.917f, 17.500f, 1.917f},    // 凹口 → 右穹顶
            {19.200f, 1.917f, 24.000f, 5.200f, 24.000f, 8.967f},    // 右穹顶 → 右肩
            {24.000f, 13.514f, 19.214f, 18.480f, 12.000f, 21.847f}, // 右肩 → 底部尖端
            {4.786f, 18.480f, 0.000f, 13.514f, 0.000f, 8.967f},     // 底部尖端 → 左肩
            {0.000f, 5.200f, 4.800f, 1.917f, 6.500f, 1.917f},       // 左肩 → 左穹顶
            {8.200f, 1.917f, 10.200f, 2.600f, 12.000f, 5.217f}      // 左穹顶 → 凹口
        };

        // 三次贝塞尔求值
        ImVec2 CubicBezierPoint(const ImVec2& p0, const ImVec2& c1, const ImVec2& c2,
                                const ImVec2& p1, float t)
        {
            const float u = 1.0f - t;
            const float w0 = u * u * u;
            const float w1 = 3.0f * u * u * t;
            const float w2 = 3.0f * u * t * t;
            const float w3 = t * t * t;
            return ImVec2(p0.x * w0 + c1.x * w1 + c2.x * w2 + p1.x * w3,
                          p0.y * w0 + c1.y * w1 + c2.y * w2 + p1.y * w3);
        }

        // 采样心形轮廓到调用方提供的栈上数组，返回点数（<3 表示失败）
        int BuildHeartOutline(ImVec2* outPoints, int maxPoints, const ImVec2& center, float size)
        {
            if (outPoints == nullptr || maxPoints < kHeartMaxPoints || size <= 0.0f)
            {
                return 0;
            }

            const float unit = size / 24.0f;
            const ImVec2 start(center.x, center.y + (kHeartNotchY - kHeartCenterY) * unit);
            outPoints[0] = start;

            int pointCount = 1;
            ImVec2 from = start;
            for (int segment = 0; segment < kHeartSegments; ++segment)
            {
                const ImVec2 control1(center.x + (kHeartBezier[segment][0] - kHeartCenterX) * unit,
                                      center.y + (kHeartBezier[segment][1] - kHeartCenterY) * unit);
                const ImVec2 control2(center.x + (kHeartBezier[segment][2] - kHeartCenterX) * unit,
                                      center.y + (kHeartBezier[segment][3] - kHeartCenterY) * unit);
                const ImVec2 to(center.x + (kHeartBezier[segment][4] - kHeartCenterX) * unit,
                                center.y + (kHeartBezier[segment][5] - kHeartCenterY) * unit);

                // 最后一段跳过 t=1，避免与首点重合
                const int steps = (segment == kHeartSegments - 1)
                                      ? (kHeartStepsPerSegment - 1)
                                      : kHeartStepsPerSegment;
                for (int step = 1; step <= steps; ++step)
                {
                    const float t = static_cast<float>(step) / static_cast<float>(kHeartStepsPerSegment);
                    outPoints[pointCount] = CubicBezierPoint(from, control1, control2, to, t);
                    ++pointCount;
                }
                from = to;
            }
            return pointCount;
        }

        // 三态心形色（heart_switch.h:25-27 的暗色主题令牌）
        GuiColor HeartBaseColor() { return GuiColor(1.00f, 0.42f, 0.62f, 1.0f); }
        GuiColor HeartHoverColor() { return GuiColor(1.00f, 0.50f, 0.68f, 1.0f); }
        GuiColor HeartPressedColor() { return GuiColor(0.92f, 0.28f, 0.50f, 1.0f); }

        // =========================================================================
        // 内阴影：把粗描边矩形按 offset 平移后由矩形自身裁剪，形成"向内渐隐"的暗边
        // （用于新拟态按钮的按下凹陷：neumorphic_button.h:186-206 的双内阴影）
        // =========================================================================
        void DrawInsetShadow(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding,
                             ImVec2 offset, float blur, GuiColor color, int layers)
        {
            if (drawList == nullptr || color.a <= 0.0f || blur <= 0.0f || layers <= 0)
            {
                return;
            }

            const float step = blur / static_cast<float>(layers);
            drawList->PushClipRect(min, max, true);
            for (int i = 0; i < layers; ++i)
            {
                // 越靠内的层 alpha 越高，形成由边缘向中心的渐隐
                const float t = static_cast<float>(i) / static_cast<float>(layers);
                const float ext = step * static_cast<float>(layers - i);
                const float alpha = color.a * (1.0f - t);
                const ImVec2 layerMin(min.x - ext + offset.x, min.y - ext + offset.y);
                const ImVec2 layerMax(max.x + ext + offset.x, max.y + ext + offset.y);
                drawList->AddRect(layerMin, layerMax, WithAlpha(color, alpha).ToImU32(),
                                  rounding + ext, ImDrawFlags_RoundCornersAll, step * 2.0f);
            }
            drawList->PopClipRect();
        }
    }

    // =========================================================================
    // 心形点赞开关
    // 未选中为心形描边，选中为实心填充（填充度用指数平滑过渡）；
    // 按下时整体缩小、hover 时轻微放大，颜色在 base / hover / pressed 间过渡
    // =========================================================================
    bool GuiHeartSwitch(const char* label, bool* value, const ImVec2& size)
    {
        if (value == nullptr || size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::PushID(label != nullptr ? label : "##gui_heart_switch");
        const bool clicked = ImGui::InvisibleButton("##heart_hit", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        // 缩放：按下 0.88，hover 1.06，常态 1.0
        float* scaleAnim = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##scale"), 1.0f);
        *scaleAnim = SmoothTowards(*scaleAnim, active ? 0.88f : (hovered ? 1.06f : 1.0f), 24.0f);

        // 填充度：选中 → 1，未选中 → 0
        float* fillAnim = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##fill"), *value ? 1.0f : 0.0f);
        *fillAnim = SmoothTowards(*fillAnim, *value ? 1.0f : 0.0f, 14.0f);

        GuiColor heartColor = HeartBaseColor();
        if (active)
        {
            heartColor = HeartPressedColor();
        }
        else if (hovered)
        {
            heartColor = HeartHoverColor();
        }

        const ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        const float heartSize = std::min(size.x, size.y) * 0.86f * (*scaleAnim);

        ImVec2 points[kHeartMaxPoints];
        const int pointCount = BuildHeartOutline(points, kHeartMaxPoints, center, heartSize);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (pointCount >= 3 && heartSize > 1.0f)
        {
            // 凹多边形填充（ImGui 内部三角化），比"以凹口为锚点的扇形近似"更贴合轮廓
            if (*fillAnim > 0.01f)
            {
                drawList->AddConcavePolyFilled(points, pointCount,
                                              WithAlpha(heartColor, *fillAnim).ToImU32());
            }

            // 描边：未选中时用 muted 心形色，选中时用亮色描边
            const GuiColor strokeBase = *value ? heartColor : MixColor(tokens.text, heartColor, 0.45f);
            drawList->AddPolyline(points, pointCount, WithAlpha(strokeBase, 0.90f).ToImU32(),
                                  ImDrawFlags_Closed, std::max(1.5f, heartSize * 0.055f));
        }

        ImGui::PopID();

        if (clicked)
        {
            *value = !(*value);
            return true;
        }
        return false;
    }

    // =========================================================================
    // 新拟态按钮
    // 凸起态：亮阴影（左上 blur 10 / 偏移 -5,-5）+ 暗阴影（右下 blur 14 / 偏移 7,7）；
    // 按下：外阴影淡出、转为双内阴影（右下暗 + 左上亮）+ 表面变暗 + pressScale 0.986
    // =========================================================================
    bool GuiNeumorphicButton(const char* label, const ImVec2& size)
    {
        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        const float width = (size.x > 0.0f) ? size.x : 160.0f;
        const float height = (size.y > 0.0f) ? size.y : 44.0f;
        const float radius = tokens.radius.elevated;

        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::PushID(label != nullptr ? label : "##gui_neumorphic");
        const bool clicked = ImGui::InvisibleButton("##hit", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        // 按压进度：按下 1，松开 0（neomorphic_button.h:25 的 pressScale = 0.986）
        float* pressAnim = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##press"), 0.0f);
        *pressAnim = SmoothTowards(*pressAnim, active ? 1.0f : 0.0f, 26.0f);
        const float scale = 1.0f - 0.014f * (*pressAnim);

        // 以中心缩放后的按钮矩形
        const ImVec2 center(pos.x + width * 0.5f, pos.y + height * 0.5f);
        const ImVec2 buttonMin(center.x - width * 0.5f * scale, center.y - height * 0.5f * scale);
        const ImVec2 buttonMax(center.x + width * 0.5f * scale, center.y + height * 0.5f * scale);
        const ImVec2 buttonSize(buttonMax.x - buttonMin.x, buttonMax.y - buttonMin.y);

        // 主题相关的表层色与阴影色（neumorphic_button.h:31-44）
        const GuiColor surfaceBase = tokens.isDark
                                         ? MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.14f)
                                         : tokens.surface;
        const GuiColor darkShadow = tokens.isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.38f)
                                                  : GuiColor(0.28f, 0.32f, 0.40f, 0.20f);
        const GuiColor lightShadow = tokens.isDark ? GuiColor(1.0f, 1.0f, 1.0f, 0.18f)
                                                   : GuiColor(1.0f, 1.0f, 1.0f, 0.90f);
        const GuiColor innerDark = tokens.isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.28f)
                                                 : GuiColor(0.28f, 0.32f, 0.40f, 0.26f);
        const GuiColor innerLight = tokens.isDark ? GuiColor(1.0f, 1.0f, 1.0f, 0.13f)
                                                  : GuiColor(1.0f, 1.0f, 1.0f, 0.85f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // === 凸起外阴影（按下时淡出）===
        const float outerAlpha = 1.0f - std::clamp(*pressAnim * 2.0f, 0.0f, 1.0f);
        if (outerAlpha > 0.01f)
        {
            GuiShadowStyle lightShadowStyle;
            lightShadowStyle.blur = 10.0f;
            lightShadowStyle.offsetX = -5.0f;
            lightShadowStyle.offsetY = -5.0f;
            lightShadowStyle.color = WithAlpha(lightShadow, lightShadow.a * outerAlpha);
            DrawGuiSoftShadow(drawList, buttonMin, buttonSize, lightShadowStyle, radius);

            GuiShadowStyle darkShadowStyle;
            darkShadowStyle.blur = 14.0f;
            darkShadowStyle.offsetX = 7.0f;
            darkShadowStyle.offsetY = 7.0f;
            darkShadowStyle.color = WithAlpha(darkShadow, darkShadow.a * outerAlpha);
            DrawGuiSoftShadow(drawList, buttonMin, buttonSize, darkShadowStyle, radius);
        }

        // === 表面三态 ===
        GuiColor surface = surfaceBase;
        if (active)
        {
            surface = MixColor(surfaceBase, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.08f);
        }
        else if (hovered)
        {
            surface = MixColor(surfaceBase, GuiColor(1.0f, 1.0f, 1.0f, 1.0f), 0.035f);
        }
        drawList->AddRectFilled(buttonMin, buttonMax, surface.ToImU32(), radius);
        drawList->AddRect(buttonMin, buttonMax, WithAlpha(surface, 0.90f).ToImU32(),
                          radius, ImDrawFlags_RoundCornersAll, 1.0f);

        // === 按下内阴影 ===
        if (*pressAnim > 0.01f)
        {
            DrawInsetShadow(drawList, buttonMin, buttonMax, radius, ImVec2(4.0f, 4.0f), 10.0f,
                            WithAlpha(innerDark, innerDark.a * *pressAnim), 6);
            DrawInsetShadow(drawList, buttonMin, buttonMax, radius, ImVec2(-4.0f, -4.0f), 10.0f,
                            WithAlpha(innerLight, innerLight.a * *pressAnim), 6);
        }

        // === 文字：按下时变淡 ===
        const char* textBegin = nullptr;
        int textLength = 0;
        SplitVisibleLabel(label, &textBegin, &textLength);
        if (textLength > 0)
        {
            const float fontSize = tokens.typography.option;
            const float textAlpha = (tokens.isDark ? 0.94f : 1.0f) * (1.0f - 0.28f * (*pressAnim));
            ImFont* font = ImGui::GetFont();
            if (font != nullptr)
            {
                const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f,
                                                            textBegin, textBegin + textLength);
                drawList->AddText(font, fontSize,
                                  ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                                  WithAlpha(tokens.text, textAlpha).ToImU32(),
                                  textBegin, textBegin + textLength);
            }
        }

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 3D 倾斜卡片
    // 鼠标位置驱动倾斜（tilt_card.h:192 maxTilt = 0.22rad、:113 perspective = 640、
    // :115 hoverScale = 1.012）：四角按绕 Y/X 轴旋转 + 透视投影得到 2D 位置，
    // 主体用上下两段四边形做 surfaceTop → surface 渐变，高光跟随鼠标移动
    // =========================================================================
    bool GuiTiltCard(const char* label, const ImVec2& size, const char* title,
                     const char* subtitle, const std::function<void()>& content)
    {
        if (size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        constexpr float kMaxTilt = 0.22f;   // 最大倾斜角（弧度）
        constexpr float kPerspective = 640.0f;
        constexpr float kHoverScale = 1.012f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
        const ImVec2 center((pos.x + maxPos.x) * 0.5f, (pos.y + maxPos.y) * 0.5f);

        ImGui::PushID(label != nullptr ? label : "##gui_tilt_card");
        const bool clicked = ImGui::InvisibleButton("##hit", size);
        const bool hovered = ImGui::IsItemHovered();

        // 鼠标相对位置（-1..1）与倾斜平滑
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float normX = std::clamp((mouse.x - center.x) / (size.x * 0.5f), -1.0f, 1.0f);
        const float normY = std::clamp((mouse.y - center.y) / (size.y * 0.5f), -1.0f, 1.0f);

        float* tiltX = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##tilt_x"), 0.0f);
        float* tiltY = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##tilt_y"), 0.0f);
        float* zoom = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##zoom"), 1.0f);

        *tiltX = SmoothTowards(*tiltX, hovered ? normX : 0.0f, 10.0f);
        *tiltY = SmoothTowards(*tiltY, hovered ? normY : 0.0f, 10.0f);
        *zoom = SmoothTowards(*zoom, hovered ? kHoverScale : 1.0f, 12.0f);

        const float angleX = (*tiltY) * kMaxTilt; // 绕 X 轴（上下倾斜）
        const float angleY = (*tiltX) * kMaxTilt; // 绕 Y 轴（左右倾斜）
        const float sinX = std::sin(angleX);
        const float sinY = std::sin(angleY);
        const float halfWidth = size.x * 0.5f * (*zoom);
        const float halfHeight = size.y * 0.5f * (*zoom);

        // 四角透视投影：z 越大（越靠近观察者）投影越大
        auto project = [&](float signX, float signY)
        {
            const float localX = signX * halfWidth;
            const float localY = signY * halfHeight;
            const float depth = localX * sinY - localY * sinX;
            const float factor = kPerspective / std::max(1.0f, kPerspective - depth);
            return ImVec2(center.x + localX * factor, center.y + localY * factor);
        };

        const ImVec2 cornerTL = project(-1.0f, -1.0f);
        const ImVec2 cornerTR = project(1.0f, -1.0f);
        const ImVec2 cornerBR = project(1.0f, 1.0f);
        const ImVec2 cornerBL = project(-1.0f, 1.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 背景色（tilt_card.h:24-43 的暗色方案）
        const GuiColor surfaceBase = tokens.isDark
                                         ? MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.10f)
                                         : MixColor(tokens.surface, tokens.surfaceHover, 0.12f);
        const GuiColor surfaceTopBase = tokens.isDark
                                            ? MixColor(tokens.surfaceHover, GuiColor(1.0f, 1.0f, 1.0f, 1.0f), 0.04f)
                                            : MixColor(tokens.surfaceHover, GuiColor(1.0f, 1.0f, 1.0f, 1.0f), 0.35f);

        // 上下两段四边形模拟纵向渐变
        const ImVec2 midL((cornerTL.x + cornerBL.x) * 0.5f, (cornerTL.y + cornerBL.y) * 0.5f);
        const ImVec2 midR((cornerTR.x + cornerBR.x) * 0.5f, (cornerTR.y + cornerBR.y) * 0.5f);

        // 阴影：卡片下方一层柔和黑
        const ImVec2 shadowMin(std::min(std::min(cornerTL.x, cornerTR.x), std::min(cornerBL.x, cornerBR.x)),
                               std::min(std::min(cornerTL.y, cornerTR.y), std::min(cornerBL.y, cornerBR.y)));
        const ImVec2 shadowMax(std::max(std::max(cornerTL.x, cornerTR.x), std::max(cornerBL.x, cornerBR.x)),
                               std::max(std::max(cornerTL.y, cornerTR.y), std::max(cornerBL.y, cornerBR.y)));
        GuiShadowStyle shadowStyle;
        shadowStyle.blur = 22.0f;
        shadowStyle.offsetX = 0.0f;
        shadowStyle.offsetY = 8.0f;
        shadowStyle.color = tokens.isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.34f)
                                          : GuiColor(0.28f, 0.32f, 0.40f, 0.20f);
        DrawGuiSoftShadow(drawList, shadowMin, ImVec2(shadowMax.x - shadowMin.x, shadowMax.y - shadowMin.y),
                          shadowStyle, tokens.radius.feature);

        drawList->AddQuadFilled(cornerTL, cornerTR, midR, midL, surfaceTopBase.ToImU32());
        drawList->AddQuadFilled(midL, midR, cornerBR, cornerBL, surfaceBase.ToImU32());

        // 高光：跟随鼠标的半透明白圆，裁剪到卡片包围盒
        drawList->PushClipRect(shadowMin, shadowMax, true);
        const ImVec2 highlight(normX * size.x * 0.30f + center.x, normY * size.y * 0.30f + center.y);
        drawList->AddCircleFilled(highlight, std::min(size.x, size.y) * 0.85f,
                                  IM_COL32(255, 255, 255, (int)(10.0f + 10.0f * (hovered ? 1.0f : 0.0f))), 32);
        drawList->PopClipRect();

        // 边框
        const ImVec2 outline[4] = { cornerTL, cornerTR, cornerBR, cornerBL };
        drawList->AddPolyline(outline, 4,
                              WithAlpha(tokens.isDark ? tokens.border : tokens.border, 0.72f).ToImU32(),
                              ImDrawFlags_Closed, 1.0f);

        // 标题 / 副标题（居中，不做透视变形以保证可读性）
        ImFont* font = ImGui::GetFont();
        if (font != nullptr)
        {
            const float titleSize = tokens.typography.cardTitle;
            const float subtitleSize = tokens.typography.label;
            const float blockHeight = (subtitle != nullptr && subtitle[0] != '\0')
                                          ? titleSize + tokens.typography.lineGap + subtitleSize
                                          : titleSize;
            float textY = center.y - blockHeight * 0.5f;

            if (title != nullptr && title[0] != '\0')
            {
                const ImVec2 titleExtent = font->CalcTextSizeA(titleSize, size.x - 24.0f, size.x - 24.0f, title);
                drawList->AddText(font, titleSize,
                                  ImVec2(center.x - titleExtent.x * 0.5f, textY),
                                  WithAlpha(tokens.text, tokens.isDark ? 0.96f : 0.94f).ToImU32(), title);
                textY += titleSize + tokens.typography.lineGap;
            }
            if (subtitle != nullptr && subtitle[0] != '\0')
            {
                const ImVec2 subtitleExtent = font->CalcTextSizeA(subtitleSize, size.x - 24.0f,
                                                                  size.x - 24.0f, subtitle);
                drawList->AddText(font, subtitleSize,
                                  ImVec2(center.x - subtitleExtent.x * 0.5f, textY),
                                  WithAlpha(tokens.text, tokens.isDark ? 0.68f : 0.62f).ToImU32(), subtitle);
            }
        }

        // 内容回调：在卡片中心区绘制（绘制后恢复光标，避免影响外部布局）
        if (content)
        {
            const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
            ImGui::PushClipRect(shadowMin, shadowMax, true);
            ImGui::SetCursorScreenPos(ImVec2(center.x - size.x * 0.5f + 16.0f,
                                             center.y + size.y * 0.5f - 48.0f));
            content();
            ImGui::PopClipRect();
            ImGui::SetCursorScreenPos(savedCursor);
        }

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 卡片堆叠轮播
    // 基准尺寸 250×400（card_slider.h:212-213），位距 = 基准宽 × 1.10；
    // 选中卡片居中放大，左右相邻卡片位移并缩小叠加，只绘制 ±2 张；
    // 底部半透明渐变条 + 标题/副标题，左右圆形箭头 + 底部指示点
    // =========================================================================
    bool GuiCardSlider(const char* label, const Texture2D* textures, const char* const* titles,
                       const char* const* subtitles, int count, int* selectedIndex,
                       const ImVec2& size)
    {
        if (count <= 0 || selectedIndex == nullptr || size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
        const float centerX = pos.x + size.x * 0.5f;
        const float centerY = pos.y + size.y * 0.5f - 10.0f;

        // 缩放系数：屏幕越大卡片越大，最多 1.0（基准 750×500）
        const float scale = std::min(std::min(size.x / 750.0f, (size.y - 56.0f) / 500.0f), 1.0f);
        const float baseWidth = 250.0f * std::max(0.4f, scale);
        const float baseHeight = 400.0f * std::max(0.4f, scale);
        const float slotOffset = baseWidth * 1.10f;

        ImGui::PushID(label != nullptr ? label : "##gui_card_slider");
        const ImGuiID sliderId = ImGui::GetID("##gui_card_slider_anim");

        int current = std::clamp(*selectedIndex, 0, count - 1);
        bool changed = false;

        // 位移动画：沿最短方向平滑靠近当前项（支持首尾环绕，不会整圈回绕）
        float* animSlot = ImGui::GetStateStorage()->GetFloatRef(sliderId ^ 0x51ED2707u,
                                                               static_cast<float>(current));
        {
            float delta = static_cast<float>(current) - *animSlot;
            if (count > 1)
            {
                // 把距离归一化到 (-count/2, count/2]，首尾相邻时走最短路径
                const float cycle = static_cast<float>(count);
                delta = std::fmod(delta, cycle);
                if (delta > cycle * 0.5f)
                {
                    delta -= cycle;
                }
                else if (delta < -cycle * 0.5f)
                {
                    delta += cycle;
                }
            }
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
            *animSlot += delta * (1.0f - std::exp(-12.0f * dt));
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 连续视觉中心：animSlot 可为小数/越界，绘制时按 count 取模映射回真实索引
        const int visualCenter = static_cast<int>(std::lround(*animSlot));
        const float fractionalSlot = *animSlot - static_cast<float>(visualCenter);

        // 从最远层向中心绘制，保证中间卡片在最上层；小 count 环绕时用去重表避免重复绘制
        int drawnIndices[5];
        int drawnCount = 0;
        for (int depth = 2; depth >= 0; --depth)
        {
            for (int sign = -1; sign <= 1; sign += 2)
            {
                if (depth == 0 && sign > 0)
                {
                    continue; // 0 层只画一次（sign = -1 那一轮）
                }

                const int slotIndex = sign * depth;
                const int index = ((visualCenter + slotIndex) % count + count) % count;
                const float slot = static_cast<float>(slotIndex) + fractionalSlot;

                // 环绕后可能与已绘制卡片重复（count 小于 5 时），跳过
                bool duplicated = false;
                for (int drawn = 0; drawn < drawnCount; ++drawn)
                {
                    if (drawnIndices[drawn] == index)
                    {
                        duplicated = true;
                        break;
                    }
                }
                if (duplicated || std::fabs(slot) > 2.5f)
                {
                    continue;
                }
                if (drawnCount < 5)
                {
                    drawnIndices[drawnCount++] = index;
                }

                // 距离越远越小：中心 1.0，相邻 0.82
                const float distance = std::fabs(slot);
                const float cardScale = (distance < 0.5f) ? 1.0f : 0.82f;
                const float cardWidth = baseWidth * cardScale;
                const float cardHeight = baseHeight * cardScale;
                const float cardCenterX = centerX + slot * slotOffset;
                const ImVec2 cardMin(cardCenterX - cardWidth * 0.5f, centerY - cardHeight * 0.5f);
                const ImVec2 cardMax(cardMin.x + cardWidth, cardMin.y + cardHeight);
                if (cardMax.x < pos.x || cardMin.x > maxPos.x)
                {
                    continue;
                }

                // 阴影：只给近处卡片绘制
                if (distance <= 1.5f)
                {
                    GuiShadowStyle shadowStyle;
                    shadowStyle.blur = 20.0f;
                    shadowStyle.offsetX = 0.0f;
                    shadowStyle.offsetY = 10.0f;
                    shadowStyle.color = GuiColor(0.0f, 0.0f, 0.0f, 0.30f);
                    DrawGuiSoftShadow(drawList, cardMin, ImVec2(cardWidth, cardHeight),
                                      shadowStyle, tokens.radius.card);
                }

                // 卡片主体：有纹理则绘制圆角图片，否则用表面色占位
                const float cardRounding = tokens.radius.card;
                const Texture2D texture = (textures != nullptr) ? textures[index] : nullptr;
                if (texture != nullptr)
                {
                    drawList->AddImageRounded((ImTextureID)texture, cardMin, cardMax,
                                              ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                              IM_COL32_WHITE, cardRounding);
                }
                else
                {
                    drawList->AddRectFilled(cardMin, cardMax, tokens.surfaceHover.ToImU32(), cardRounding);
                }

                // 底部半透明 overlay：保证文字可读
                const float overlayHeight = cardHeight * 0.42f;
                const ImVec2 overlayMin(cardMin.x, cardMax.y - overlayHeight);
                drawList->AddRectFilledMultiColor(overlayMin, cardMax,
                                                  IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                                  IM_COL32(0, 0, 0, 190), IM_COL32(0, 0, 0, 190));

                drawList->AddRect(cardMin, cardMax, WithAlpha(tokens.border, 0.42f).ToImU32(),
                                  cardRounding, ImDrawFlags_RoundCornersAll, 1.0f);

                // 标题 / 副标题（居中于卡片底部）
                ImFont* font = ImGui::GetFont();
                if (font != nullptr)
                {
                    const char* titleText = (titles != nullptr) ? titles[index] : nullptr;
                    const char* subtitleText = (subtitles != nullptr) ? subtitles[index] : nullptr;
                    float textBottom = cardMax.y - 10.0f;

                    if (subtitleText != nullptr && subtitleText[0] != '\0')
                    {
                        const float subtitleSize = tokens.typography.label;
                        const ImVec2 extent = font->CalcTextSizeA(subtitleSize, cardWidth - 16.0f,
                                                                  cardWidth - 16.0f, subtitleText);
                        drawList->AddText(font, subtitleSize,
                                          ImVec2(cardMin.x + (cardWidth - extent.x) * 0.5f,
                                                 textBottom - extent.y),
                                          IM_COL32(255, 255, 255, 204), subtitleText);
                        textBottom -= extent.y + 2.0f;
                    }
                    if (titleText != nullptr && titleText[0] != '\0')
                    {
                        const float titleSize = tokens.typography.subtitle;
                        const ImVec2 extent = font->CalcTextSizeA(titleSize, cardWidth - 16.0f,
                                                                  cardWidth - 16.0f, titleText);
                        drawList->AddText(font, titleSize,
                                          ImVec2(cardMin.x + (cardWidth - extent.x) * 0.5f,
                                                 textBottom - extent.y),
                                          IM_COL32(255, 255, 255, 250), titleText);
                    }
                }
            }
        }

        // === 左右箭头 ===
        const float buttonSize = std::clamp(size.y * 0.16f, 28.0f, 44.0f);
        const float buttonMargin = tokens.spacing.compact;
        const float halfButton = buttonSize * 0.5f;
        const ImVec2 leftCenter(pos.x + buttonMargin + halfButton, centerY);
        const ImVec2 rightCenter(maxPos.x - buttonMargin - halfButton, centerY);

        auto drawArrow = [&](int buttonIndex, const ImVec2& buttonCenter, const char* icon, bool enabled) -> bool
        {
            bool arrowClicked = false;
            bool arrowHovered = false;
            bool arrowActive = false;
            if (enabled)
            {
                ImGui::PushID(buttonIndex);
                ImGui::SetCursorScreenPos(ImVec2(buttonCenter.x - halfButton, buttonCenter.y - halfButton));
                arrowClicked = ImGui::InvisibleButton("##arrow", ImVec2(buttonSize, buttonSize));
                arrowHovered = ImGui::IsItemHovered();
                arrowActive = ImGui::IsItemActive();
                ImGui::PopID();
            }

            const GuiColor background = enabled
                                            ? (arrowActive ? tokens.surfaceActive
                                                           : (arrowHovered ? tokens.surfaceHover : tokens.surface))
                                            : WithAlpha(tokens.surface, 0.55f);
            drawList->AddCircleFilled(buttonCenter, halfButton, WithAlpha(background, 0.92f).ToImU32(), 24);
            drawList->AddCircle(buttonCenter, halfButton, WithAlpha(tokens.border, 0.62f).ToImU32(), 24, 1.0f);

            ImFont* font = ImGui::GetFont();
            if (font != nullptr)
            {
                const float iconSize = tokens.typography.option;
                const ImVec2 iconExtent = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
                drawList->AddText(font, iconSize,
                                  ImVec2(buttonCenter.x - iconExtent.x * 0.5f,
                                         buttonCenter.y - iconExtent.y * 0.5f),
                                  enabled ? tokens.text.ToImU32() : WithAlpha(tokens.text, 0.30f).ToImU32(),
                                  icon);
            }
            return arrowClicked;
        };

        // 箭头始终可用（count > 1 时首尾环绕）
        const bool wrapEnabled = (count > 1);
        if (drawArrow(0, leftCenter, ICON_FA_CHEVRON_LEFT, wrapEnabled))
        {
            current = (current - 1 + count) % count;
            changed = true;
        }
        if (drawArrow(1, rightCenter, ICON_FA_CHEVRON_RIGHT, wrapEnabled))
        {
            current = (current + 1) % count;
            changed = true;
        }
        if (changed)
        {
            *selectedIndex = current;
        }

        // === 底部指示点：选中项为 primary 胶囊 ===
        const float dotSpacing = 14.0f;
        const float dotsWidth = static_cast<float>(count - 1) * dotSpacing;
        if (dotsWidth + 24.0f <= size.x)
        {
            const float dotY = maxPos.y - 12.0f;
            float dotX = centerX - dotsWidth * 0.5f;
            for (int index = 0; index < count; ++index)
            {
                if (index == current)
                {
                    drawList->AddRectFilled(ImVec2(dotX - 9.0f, dotY - 3.0f), ImVec2(dotX + 9.0f, dotY + 3.0f),
                                            tokens.primary.ToImU32(), 3.0f);
                }
                else
                {
                    drawList->AddCircleFilled(ImVec2(dotX, dotY), 3.0f,
                                              WithAlpha(tokens.text, 0.32f).ToImU32(), 12);
                }
                dotX += dotSpacing;
            }
        }

        // 推进光标到控件底部
        ImGui::SetCursorScreenPos(ImVec2(pos.x, maxPos.y));
        ImGui::PopID();
        return changed;
    }
}
