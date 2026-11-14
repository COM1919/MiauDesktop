// GuiControls.cpp
// 通用控件的实现（声明见 BaseUI/GUIBase.h）
// 移植自 EUI-NEO components/button.h / slider.h / switch.h / checkbox.h /
//           radio.h / segmented.h / stepper.h
// EUI-NEO 为保留模式 DSL，此处按同样的视觉风格移植到 ImGui 立即模式：
// 以 InvisibleButton 承担交互命中，用 ImDrawList 自绘全部外观

#include "GUIBase.h"

#include <map>
#include <string>
#include <cstdio>

namespace GuiBase
{
    namespace
    {
        // 提取 label 中 "##" 之前的显示文本（"##" 之后的内容仅作为实例 ID）
        std::string DisplayLabelText(const char* label)
        {
            std::string text(label ? label : "");
            std::size_t pos = text.find("##");
            if (pos != std::string::npos)
            {
                text = text.substr(0, pos);
            }
            return text;
        }

        // === 设置界面行式控件的 hover 动画缓存 ===
        // key 用控件标识（label 字符串地址）——地址绝对唯一，不会产生 hash 冲突
        std::map<const void*, float> g_rowHoverAnim;

        // 绘制行背景：hover 时颜色平滑过渡（Catppuccin text 淡白高亮，从 6 → 24 alpha）
        void DrawSettingsRowBg(ImDrawList* dl, const ImVec2& rowMin, const ImVec2& rowMax, const void* key)
        {
            float& anim = g_rowHoverAnim[key];
            const bool hovered = ImGui::IsMouseHoveringRect(rowMin, rowMax, false);
            const float dt = ImGui::GetIO().DeltaTime;
            const float t = 1.0f - std::exp(-15.0f * dt);
            anim = ImLerp(anim, hovered ? 1.0f : 0.0f, t);
            const ImU32 col = IM_COL32(198, 208, 245, (int)(6.0f + 18.0f * anim));  // text #c6d0f5
            dl->AddRectFilled(rowMin, rowMax, col, 6.0f);
        }

        // hover 态颜色：向白色混合变亮（移植自 EUI theme::buttonHover）
        GuiColor HoverColor(const GuiThemeTokens& tokens, const GuiColor& base)
        {
            return MixColor(base, GuiColor(1.0f, 1.0f, 1.0f, base.a), tokens.isDark ? 0.16f : 0.10f);
        }

        // active 态颜色：向黑色混合变暗（移植自 EUI theme::buttonPressed）
        GuiColor ActiveColor(const GuiThemeTokens& tokens, const GuiColor& base)
        {
            return MixColor(base, GuiColor(0.0f, 0.0f, 0.0f, base.a), tokens.isDark ? 0.34f : 0.22f);
        }

        // 滑块通用实现：轨道 + 填充 + 圆形把手，供浮点与整数版本复用
        bool SliderFloatImpl(const char* label, float* value, float vMin, float vMax,
                             const char* valueText)
        {
            GuiThemeTokens tokens = Theme::Dark();
            const float width = 220.0f;
            const float height = 24.0f;
            const float gap = tokens.spacing.compact;

            ImGui::PushID(label);

            std::string displayText = DisplayLabelText(label);
            ImVec2 labelSize = displayText.empty() ? ImVec2(0.0f, 0.0f)
                                                   : ImGui::CalcTextSize(displayText.c_str());
            ImVec2 valueSize = ImGui::CalcTextSize(valueText);

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##gui_slider_hit", ImVec2(width, height));
            bool hovered = ImGui::IsItemHovered();
            bool active = ImGui::IsItemActive();

            // 轨道左右边界：label 之后、值文本之前
            float trackLeft = pos.x + labelSize.x + (labelSize.x > 0.0f ? gap : 0.0f);
            float trackRight = pos.x + width - valueSize.x - (valueSize.x > 0.0f ? gap : 0.0f);
            float trackWidth = std::max(40.0f, trackRight - trackLeft);

            // 拖拽：IsItemActive 期间让值跟随鼠标
            bool changed = false;
            if (active)
            {
                float t = (ImGui::GetIO().MousePos.x - trackLeft) / trackWidth;
                t = std::clamp(t, 0.0f, 1.0f);
                float newValue = vMin + t * (vMax - vMin);
                if (std::fabs(newValue - *value) > 1e-6f)
                {
                    *value = newValue;
                    changed = true;
                }
            }

            // 拖拽结束同样视为一次有效交互
            if (ImGui::IsItemDeactivated())
            {
                changed = true;
            }

            // 几何尺寸：细轨道 + 圆形把手
            const float trackH = std::max(tokens.spacing.tiny - tokens.spacing.hairline, height * 0.18f);
            const float trackY = pos.y + (height - trackH) * 0.5f;
            const float knobSize = std::max(tokens.typography.label, height * 0.72f);
            const float knobRadius = knobSize * 0.5f;
            const float range = (vMax > vMin) ? (vMax - vMin) : 1.0f;
            float ratio = std::clamp((*value - vMin) / range, 0.0f, 1.0f);
            float knobCenterX = trackLeft + ratio * trackWidth;
            knobCenterX = std::clamp(knobCenterX, trackLeft + knobRadius, trackLeft + trackWidth - knobRadius);

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 轨道底色（hover 时略微变亮）
            GuiColor trackColor = hovered ? HoverColor(tokens, tokens.surfaceHover)
                                          : tokens.surfaceHover;
            drawList->AddRectFilled(ImVec2(trackLeft, trackY), ImVec2(trackLeft + trackWidth, trackY + trackH),
                                    trackColor.ToImU32(), trackH * 0.5f);

            // 填充段（primary）
            drawList->AddRectFilled(ImVec2(trackLeft, trackY), ImVec2(knobCenterX, trackY + trackH),
                                    tokens.primary.ToImU32(), trackH * 0.5f);

            // 圆形滑块把手（text 色，跟随拖动位置）
            ImVec2 knobCenter(knobCenterX, pos.y + height * 0.5f);
            drawList->AddCircleFilled(knobCenter, knobRadius, tokens.text.ToImU32(), 24);

            // label 文本（垂直居中）
            if (!displayText.empty())
            {
                ImVec2 textPos(pos.x, pos.y + (height - labelSize.y) * 0.5f);
                drawList->AddText(textPos, tokens.text.ToImU32(), displayText.c_str());
            }

            // 值文本（右对齐）
            ImVec2 valuePos(pos.x + width - valueSize.x, pos.y + (height - valueSize.y) * 0.5f);
            drawList->AddText(valuePos, tokens.text.ToImU32(), valueText);

            ImGui::PopID();
            return changed;
        }
    }

    // =========================================================================
    // 按钮（EUI 三态色 + control 圆角）
    // =========================================================================
    bool GuiButton(const char* label, const ImVec2& size)
    {
        GuiThemeTokens tokens = Theme::Dark();
        const float width = size.x > 0.0f ? size.x : 120.0f;
        const float height = size.y > 0.0f ? size.y : 36.0f;
        const float rounding = tokens.radius.control;

        ImGui::PushID(label);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##gui_button", ImVec2(width, height));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        // 三态背景：normal / hover（变亮）/ active（变暗）
        GuiColor bg = tokens.surface;
        if (active)
        {
            bg = ActiveColor(tokens, tokens.surface);
        }
        else if (hovered)
        {
            bg = HoverColor(tokens, tokens.surface);
        }

        ImVec2 min = pos;
        ImVec2 max = ImVec2(pos.x + width, pos.y + height);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, bg.ToImU32(), rounding);
        drawList->AddRect(min, max, WithAlpha(tokens.border, 0.70f).ToImU32(), rounding, 0, 1.0f);

        // 文本居中
        std::string displayText = DisplayLabelText(label);
        ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
        ImVec2 textPos(pos.x + (width - textSize.x) * 0.5f, pos.y + (height - textSize.y) * 0.5f);
        drawList->AddText(textPos, tokens.text.ToImU32(), displayText.c_str());

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 胶囊按钮（EUI pill 风格：圆角 = 高度一半，primary 背景）
    // =========================================================================
    bool GuiPillButton(const char* label, const ImVec2& size)
    {
        GuiThemeTokens tokens = Theme::Dark();
        const float width = size.x > 0.0f ? size.x : 120.0f;
        const float height = size.y > 0.0f ? size.y : 36.0f;
        const float rounding = height * 0.5f;

        ImGui::PushID(label);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##gui_pill_button", ImVec2(width, height));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        // 三态背景：以 primary 为基底，hover 变亮、active 变暗
        GuiColor bg = tokens.primary;
        if (active)
        {
            bg = ActiveColor(tokens, tokens.primary);
        }
        else if (hovered)
        {
            bg = HoverColor(tokens, tokens.primary);
        }

        ImVec2 min = pos;
        ImVec2 max = ImVec2(pos.x + width, pos.y + height);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, bg.ToImU32(), rounding);

        // 文本居中（primary 背景上使用浅色文字）
        std::string displayText = DisplayLabelText(label);
        ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
        GuiColor textColor = GuiColor(0.94f, 0.97f, 1.0f, 1.0f);
        ImVec2 textPos(pos.x + (width - textSize.x) * 0.5f, pos.y + (height - textSize.y) * 0.5f);
        drawList->AddText(textPos, textColor.ToImU32(), displayText.c_str());

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 滑块（浮点）
    // =========================================================================
    bool GuiSliderFloat(const char* label, float* value, float vMin, float vMax, const char* format)
    {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), format, *value);
        return SliderFloatImpl(label, value, vMin, vMax, buffer);
    }

    // =========================================================================
    // 滑块（整数）
    // =========================================================================
    bool GuiSliderInt(const char* label, int* value, int vMin, int vMax, const char* format)
    {
        float fValue = static_cast<float>(*value);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), format, *value);

        // 先按浮点路径执行拖拽交互
        GuiThemeTokens tokens = Theme::Dark();
        const float width = 220.0f;
        const float height = 24.0f;
        const float gap = tokens.spacing.compact;

        ImGui::PushID(label);

        std::string displayText = DisplayLabelText(label);
        ImVec2 labelSize = displayText.empty() ? ImVec2(0.0f, 0.0f)
                                               : ImGui::CalcTextSize(displayText.c_str());
        ImVec2 valueSize = ImGui::CalcTextSize(buffer);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##gui_slider_hit", ImVec2(width, height));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        float trackLeft = pos.x + labelSize.x + (labelSize.x > 0.0f ? gap : 0.0f);
        float trackRight = pos.x + width - valueSize.x - (valueSize.x > 0.0f ? gap : 0.0f);
        float trackWidth = std::max(40.0f, trackRight - trackLeft);

        bool changed = false;
        if (active)
        {
            float t = (ImGui::GetIO().MousePos.x - trackLeft) / trackWidth;
            t = std::clamp(t, 0.0f, 1.0f);
            fValue = static_cast<float>(vMin) + t * static_cast<float>(vMax - vMin);
            int newValue = static_cast<int>(std::lround(static_cast<double>(fValue)));
            if (newValue != *value)
            {
                *value = newValue;
                changed = true;
            }
        }

        if (ImGui::IsItemDeactivated())
        {
            changed = true;
        }

        // 绘制轨道 / 填充 / 把手（与浮点版本一致的视觉）
        const float trackH = std::max(tokens.spacing.tiny - tokens.spacing.hairline, height * 0.18f);
        const float trackY = pos.y + (height - trackH) * 0.5f;
        const float knobSize = std::max(tokens.typography.label, height * 0.72f);
        const float knobRadius = knobSize * 0.5f;
        const float range = (vMax > vMin) ? static_cast<float>(vMax - vMin) : 1.0f;
        float ratio = std::clamp((static_cast<float>(*value) - static_cast<float>(vMin)) / range, 0.0f, 1.0f);
        float knobCenterX = trackLeft + ratio * trackWidth;
        knobCenterX = std::clamp(knobCenterX, trackLeft + knobRadius, trackLeft + trackWidth - knobRadius);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        GuiColor trackColor = hovered ? HoverColor(tokens, tokens.surfaceHover)
                                      : tokens.surfaceHover;
        drawList->AddRectFilled(ImVec2(trackLeft, trackY), ImVec2(trackLeft + trackWidth, trackY + trackH),
                                trackColor.ToImU32(), trackH * 0.5f);
        drawList->AddRectFilled(ImVec2(trackLeft, trackY), ImVec2(knobCenterX, trackY + trackH),
                                tokens.primary.ToImU32(), trackH * 0.5f);

        ImVec2 knobCenter(knobCenterX, pos.y + height * 0.5f);
        drawList->AddCircleFilled(knobCenter, knobRadius, tokens.text.ToImU32(), 24);

        if (!displayText.empty())
        {
            ImVec2 textPos(pos.x, pos.y + (height - labelSize.y) * 0.5f);
            drawList->AddText(textPos, tokens.text.ToImU32(), displayText.c_str());
        }

        ImVec2 valuePos(pos.x + width - valueSize.x, pos.y + (height - valueSize.y) * 0.5f);
        drawList->AddText(valuePos, tokens.text.ToImU32(), buffer);

        ImGui::PopID();
        return changed;
    }

    // =========================================================================
    // 开关（胶囊轨道 + 圆形滑块，带滑动动画）
    // =========================================================================
    bool GuiSwitch(const char* label, bool* value)
    {
        GuiThemeTokens tokens = Theme::Dark();
        const float trackWidth = 44.0f;
        const float trackHeight = 24.0f;
        const float height = 24.0f;
        const float gap = tokens.spacing.compact;

        ImGui::PushID(label);

        // 动画状态：以 ImGuiID 区分不同开关实例
        static std::map<ImGuiID, float> animMap;
        ImGuiID animId = ImGui::GetID("##gui_switch_anim");
        float& anim = animMap[animId];
        float target = *value ? 1.0f : 0.0f;
        const float speed = 1.0f - std::exp(-12.0f * ImGui::GetIO().DeltaTime);
        anim = ImLerp(anim, target, speed);
        if (std::fabs(anim - target) < 0.001f)
        {
            anim = target;
        }

        std::string displayText = DisplayLabelText(label);
        ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
        float totalWidth = trackWidth + gap + textSize.x;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##gui_switch", ImVec2(totalWidth, height));
        if (clicked)
        {
            *value = !*value;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 轨道：关闭态为 surfaceHover 与 surfaceActive 的混合，开启态为 primary，随动画过渡
        GuiColor offColor = MixColor(tokens.surfaceHover, tokens.surfaceActive, 0.30f);
        GuiColor trackColor = MixColor(offColor, tokens.primary, anim);
        ImVec2 trackPos(pos.x, pos.y + (height - trackHeight) * 0.5f);
        drawList->AddRectFilled(trackPos, ImVec2(trackPos.x + trackWidth, trackPos.y + trackHeight),
                                trackColor.ToImU32(), trackHeight * 0.5f);

        // 滑块：接近白色的表面色，位置随动画滑动
        const float margin = std::max(tokens.spacing.micro, trackHeight * 0.125f);
        const float knobSize = std::max(0.0f, trackHeight - margin * 2.0f);
        const float knobTravel = std::max(0.0f, trackWidth - margin * 2.0f - knobSize);
        float knobX = margin + knobTravel * anim;
        GuiColor knobColor = MixColor(tokens.surface, GuiColor(1.0f, 1.0f, 1.0f, 1.0f), 0.75f);
        ImVec2 knobCenter(trackPos.x + knobX + knobSize * 0.5f, trackPos.y + margin + knobSize * 0.5f);
        drawList->AddCircleFilled(knobCenter, knobSize * 0.5f, knobColor.ToImU32(), 24);

        // label 文本
        if (!displayText.empty())
        {
            ImVec2 textPos(pos.x + trackWidth + gap, pos.y + (height - textSize.y) * 0.5f);
            drawList->AddText(textPos, tokens.text.ToImU32(), displayText.c_str());
        }

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 复选框（方形圆角框 + 对勾）
    // =========================================================================
    bool GuiCheckbox(const char* label, bool* value)
    {
        GuiThemeTokens tokens = Theme::Dark();
        const float boxSize = 18.0f;
        const float height = 26.0f;
        const float gap = tokens.spacing.control;
        const float radius = tokens.radius.small;

        ImGui::PushID(label);

        std::string displayText = DisplayLabelText(label);
        ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
        float totalWidth = boxSize + gap + textSize.x;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##gui_checkbox", ImVec2(totalWidth, height));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        if (clicked)
        {
            *value = !*value;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 boxPos(pos.x, pos.y + (height - boxSize) * 0.5f);
        ImVec2 boxMax(boxPos.x + boxSize, boxPos.y + boxSize);

        // 框体三态：选中为 primary，未选中为 surface，hover / active 变亮或变暗
        GuiColor boxColor = *value ? tokens.primary : tokens.surface;
        if (active)
        {
            boxColor = ActiveColor(tokens, boxColor);
        }
        else if (hovered)
        {
            boxColor = HoverColor(tokens, boxColor);
        }
        drawList->AddRectFilled(boxPos, boxMax, boxColor.ToImU32(), radius);
        drawList->AddRect(boxPos, boxMax, WithAlpha(tokens.border, 0.70f).ToImU32(), radius, 0, 1.0f);

        // 对勾（选中时绘制）：两条折线，几何移植自 EUI checkbox
        if (*value)
        {
            const float thickness = std::max(2.0f, boxSize * 0.12f);
            const float angleCos = 0.7109f;
            const float angleSin = 0.7033f;
            const float shortLen = boxSize * 0.28f;
            const float longLen = boxSize * 0.46f;
            ImVec2 markStart(boxPos.x + boxSize * 0.26f, boxPos.y + boxSize * 0.55f);
            ImVec2 markCorner(markStart.x + shortLen * angleCos, markStart.y + shortLen * angleSin);
            ImVec2 markEnd(markCorner.x + longLen * angleCos, markCorner.y - longLen * angleSin);
            ImU32 markColor = GuiColor(1.0f, 1.0f, 1.0f, 1.0f).ToImU32();
            drawList->AddLine(markStart, markCorner, markColor, thickness);
            drawList->AddLine(markCorner, markEnd, markColor, thickness);
        }

        // label 文本
        if (!displayText.empty())
        {
            ImVec2 textPos(pos.x + boxSize + gap, pos.y + (height - textSize.y) * 0.5f);
            drawList->AddText(textPos, tokens.text.ToImU32(), displayText.c_str());
        }

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 单选（圆形外圈 + 选中实心圆点）
    // =========================================================================
    bool GuiRadio(const char* label, bool* value)
    {
        GuiThemeTokens tokens = Theme::Dark();
        const float outerSize = 18.0f;
        const float height = 26.0f;
        const float gap = tokens.spacing.control;
        const float innerSize = outerSize * 0.48f;

        ImGui::PushID(label);

        std::string displayText = DisplayLabelText(label);
        ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
        float totalWidth = outerSize + gap + textSize.x;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##gui_radio", ImVec2(totalWidth, height));
        bool hovered = ImGui::IsItemHovered();
        if (clicked)
        {
            *value = true;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 center(pos.x + outerSize * 0.5f, pos.y + height * 0.5f);
        const float outerRadius = outerSize * 0.5f;

        // 外圈：选中时淡 primary 填充 + primary 描边，未选中时 surface 填充 + border 描边
        GuiColor outerFill = *value ? WithAlpha(tokens.primary, 0.18f) : tokens.surface;
        if (hovered && !*value)
        {
            outerFill = tokens.surfaceHover;
        }
        GuiColor outerBorder = *value ? tokens.primary : tokens.border;
        drawList->AddCircleFilled(center, outerRadius, outerFill.ToImU32(), 24);
        drawList->AddCircle(center, outerRadius, outerBorder.ToImU32(), 24, 1.5f);

        // 内圆点（选中时绘制）
        if (*value)
        {
            drawList->AddCircleFilled(center, innerSize * 0.5f, tokens.primary.ToImU32(), 24);
        }

        // label 文本
        if (!displayText.empty())
        {
            ImVec2 textPos(pos.x + outerSize + gap, pos.y + (height - textSize.y) * 0.5f);
            drawList->AddText(textPos, tokens.text.ToImU32(), displayText.c_str());
        }

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 分段控件（整条背景 + 选中段高亮，选中项变化时返回 true）
    // =========================================================================
    bool GuiSegmented(const char* label, const char* const* items, int itemCount, int* selectedIndex)
    {
        if (items == nullptr || itemCount <= 0)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark();
        const float height = 36.0f;
        const float radius = tokens.radius.control + tokens.spacing.hairline;
        const float innerInset = std::max(0.0f, tokens.spacing.tiny - tokens.spacing.hairline);

        // 宽度按最大项文本自适应，保证各段等宽
        float maxTextWidth = 0.0f;
        for (int i = 0; i < itemCount; ++i)
        {
            if (items[i] != nullptr)
            {
                maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(items[i]).x);
            }
        }
        float width = std::max(220.0f, static_cast<float>(itemCount) * (maxTextWidth + 32.0f));
        float segmentWidth = width / static_cast<float>(itemCount);

        int clampedIndex = std::clamp(*selectedIndex, 0, itemCount - 1);

        ImGui::PushID(label);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton("##gui_segmented", ImVec2(width, height));
        bool hovered = ImGui::IsItemHovered();

        // 点击时按鼠标位置切换选中段
        bool changed = false;
        if (clicked)
        {
            float mouseX = ImGui::GetIO().MousePos.x;
            int index = static_cast<int>((mouseX - pos.x) / segmentWidth);
            index = std::clamp(index, 0, itemCount - 1);
            if (index != clampedIndex)
            {
                clampedIndex = index;
                *selectedIndex = index;
                changed = true;
            }
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = pos;
        ImVec2 max = ImVec2(pos.x + width, pos.y + height);

        // 整条背景 + 边框
        drawList->AddRectFilled(min, max, tokens.surface.ToImU32(), radius);
        drawList->AddRect(min, max, WithAlpha(tokens.border, 0.70f).ToImU32(), radius, 0, 1.0f);

        // hover 的未选中段：surfaceHover 高亮
        if (hovered)
        {
            float mouseX = ImGui::GetIO().MousePos.x;
            int hoverIndex = static_cast<int>((mouseX - pos.x) / segmentWidth);
            hoverIndex = std::clamp(hoverIndex, 0, itemCount - 1);
            if (hoverIndex != clampedIndex)
            {
                ImVec2 segMin(pos.x + hoverIndex * segmentWidth, pos.y + innerInset);
                ImVec2 segMax(segMin.x + segmentWidth, pos.y + height - innerInset);
                drawList->AddRectFilled(segMin, segMax, tokens.surfaceHover.ToImU32(),
                                        std::max(0.0f, radius - tokens.spacing.micro));
            }
        }

        // 选中段指示器（primary）
        ImVec2 indicatorMin(pos.x + clampedIndex * segmentWidth + innerInset, pos.y + innerInset);
        ImVec2 indicatorMax(indicatorMin.x + segmentWidth - innerInset * 2.0f, pos.y + height - innerInset);
        if (indicatorMax.x > indicatorMin.x && indicatorMax.y > indicatorMin.y)
        {
            drawList->AddRectFilled(indicatorMin, indicatorMax, tokens.primary.ToImU32(),
                                    std::max(0.0f, radius - tokens.spacing.micro));
        }

        // 各段文本：选中段使用浅色文字，其余使用主题文字色
        GuiColor selectedText = GuiColor(0.96f, 0.98f, 1.0f, 1.0f);
        for (int i = 0; i < itemCount; ++i)
        {
            if (items[i] == nullptr)
            {
                continue;
            }
            ImVec2 textSize = ImGui::CalcTextSize(items[i]);
            ImVec2 textPos(pos.x + i * segmentWidth + (segmentWidth - textSize.x) * 0.5f,
                           pos.y + (height - textSize.y) * 0.5f);
            GuiColor textColor = (i == clampedIndex) ? selectedText : tokens.text;
            drawList->AddText(textPos, textColor.ToImU32(), items[i]);
        }

        ImGui::PopID();
        return changed;
    }

    // =========================================================================
    // 步进器（左侧减号 + 中间数值 + 右侧加号，越界时夹紧）
    // =========================================================================
    bool GuiStepper(const char* label, int* value, int step, int minValue, int maxValue)
    {
        GuiThemeTokens tokens = Theme::Dark();
        const float width = 140.0f;
        const float height = 36.0f;
        const float buttonWidth = height;
        const float gap = tokens.spacing.compact;
        const float displayWidth = std::max(36.0f, width - buttonWidth * 2.0f - gap * 2.0f);
        const float displayX = buttonWidth + gap;
        const float radius = tokens.radius.card;
        const int stepValue = std::max(1, step);

        bool canDecrease = *value > minValue;
        bool canIncrease = *value < maxValue;

        ImGui::PushID(label);
        ImVec2 pos = ImGui::GetCursorScreenPos();

        // 减号按钮
        bool minusClicked = ImGui::InvisibleButton("##gui_stepper_minus", ImVec2(buttonWidth, height));
        bool minusHovered = ImGui::IsItemHovered();
        bool minusActive = ImGui::IsItemActive();
        ImGui::SameLine(0.0f, gap);

        // 中间数值区域（仅占位推进光标）
        ImGui::InvisibleButton("##gui_stepper_display", ImVec2(displayWidth, height));
        ImGui::SameLine(0.0f, gap);

        // 加号按钮
        bool plusClicked = ImGui::InvisibleButton("##gui_stepper_plus", ImVec2(buttonWidth, height));
        bool plusHovered = ImGui::IsItemHovered();
        bool plusActive = ImGui::IsItemActive();

        // 步进并夹紧到 [minValue, maxValue]
        bool changed = false;
        if (minusClicked && canDecrease)
        {
            *value = std::max(minValue, *value - stepValue);
            changed = true;
        }
        if (plusClicked && canIncrease)
        {
            *value = std::min(maxValue, *value + stepValue);
            changed = true;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 减号按钮背景：可用时三态，越界时禁用色
        GuiColor minusBg = canDecrease ? tokens.surface : MixColor(tokens.surface, tokens.background, 0.34f);
        if (canDecrease && minusActive)
        {
            minusBg = tokens.surfaceActive;
        }
        else if (canDecrease && minusHovered)
        {
            minusBg = tokens.surfaceHover;
        }
        ImVec2 minusMin = pos;
        ImVec2 minusMax(pos.x + buttonWidth, pos.y + height);
        drawList->AddRectFilled(minusMin, minusMax, minusBg.ToImU32(), radius);
        drawList->AddRect(minusMin, minusMax, WithAlpha(tokens.border, 0.82f).ToImU32(), radius, 0, 1.0f);

        // 中间数值背景：surface 向 background 混合，带边框
        ImVec2 displayMin(pos.x + displayX, pos.y);
        ImVec2 displayMax(displayMin.x + displayWidth, pos.y + height);
        GuiColor fieldColor = MixColor(tokens.surface, tokens.background, 0.12f);
        drawList->AddRectFilled(displayMin, displayMax, fieldColor.ToImU32(), radius);
        drawList->AddRect(displayMin, displayMax, WithAlpha(tokens.border, 0.82f).ToImU32(), radius, 0, 1.0f);

        // 加号按钮背景：与减号对称
        GuiColor plusBg = canIncrease ? tokens.surface : MixColor(tokens.surface, tokens.background, 0.34f);
        if (canIncrease && plusActive)
        {
            plusBg = tokens.surfaceActive;
        }
        else if (canIncrease && plusHovered)
        {
            plusBg = tokens.surfaceHover;
        }
        ImVec2 plusMin(pos.x + displayX + displayWidth + gap, pos.y);
        ImVec2 plusMax(plusMin.x + buttonWidth, pos.y + height);
        drawList->AddRectFilled(plusMin, plusMax, plusBg.ToImU32(), radius);
        drawList->AddRect(plusMin, plusMax, WithAlpha(tokens.border, 0.82f).ToImU32(), radius, 0, 1.0f);

        // 减号符号
        GuiColor minusTextColor = canDecrease ? tokens.text : WithAlpha(tokens.text, 0.42f);
        ImVec2 minusTextSize = ImGui::CalcTextSize("-");
        drawList->AddText(ImVec2(pos.x + (buttonWidth - minusTextSize.x) * 0.5f,
                                 pos.y + (height - minusTextSize.y) * 0.5f),
                          minusTextColor.ToImU32(), "-");

        // 中间数值文本（primary 色，居中）
        char valueBuffer[32];
        snprintf(valueBuffer, sizeof(valueBuffer), "%d", *value);
        ImVec2 valueTextSize = ImGui::CalcTextSize(valueBuffer);
        drawList->AddText(ImVec2(displayMin.x + (displayWidth - valueTextSize.x) * 0.5f,
                                 pos.y + (height - valueTextSize.y) * 0.5f),
                          tokens.primary.ToImU32(), valueBuffer);

        // 加号符号
        GuiColor plusTextColor = canIncrease ? tokens.text : WithAlpha(tokens.text, 0.42f);
        ImVec2 plusTextSize = ImGui::CalcTextSize("+");
        drawList->AddText(ImVec2(plusMin.x + (buttonWidth - plusTextSize.x) * 0.5f,
                                 pos.y + (height - plusTextSize.y) * 0.5f),
                          plusTextColor.ToImU32(), "+");

        ImGui::PopID();
        return changed;
    }

    // =========================================================================
    // 设置界面"行式 + 分组"样式
    // 设置窗口与组件 SettingsRender 共用：一行 = 左侧文字 + 右侧控件（Toggle / 滑块 / 下拉），
    // 行背景 hover 平滑过渡；左右内边距 = style.padding × 2（对称），行间距 = ItemSpacing
    // （不再提供分组容器 SettingsGroupBegin/End，行直接铺在当前窗口上）
    // =========================================================================

    // 设置行：左侧文字 + 右侧 Toggle（与组件启用界面一致，行背景 hover 提亮）
    bool SettingsToggleRow(const char* label, bool* value)
    {
        bool changed = false;
        ImGui::PushID(label);

        const float rowHeight = ImGui::GetFrameHeight() + 10.0f;
        // 行内边距：左右 = style.padding × 3（对称，基于内容区宽度，不依赖当前光标 x）
        const float padX = ImGui::GetStyle().WindowPadding.x * 3.0f;
        const float contentW = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        const float rowWidth = contentW - padX * 2.0f;
        // 行起点：内容区左 + padX（强制复位，避免受标题/前序控件推进的光标 x 影响）
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMin().x + padX, ImGui::GetCursorPos().y));
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = ImVec2(rowMin.x + rowWidth, rowMin.y + rowHeight);

        // 行背景：hover 平滑过渡（key = label 地址，唯一）
        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawSettingsRowBg(dl, rowMin, rowMax, label);

        // 左侧文字（垂直居中）
        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + (rowHeight - textSize.y) * 0.5f));
        ImGui::TextUnformatted(label);

        // 右侧 Toggle（垂直居中）
        const float toggleWidth = 42.0f;
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - toggleWidth, rowMin.y + (rowHeight - 20.0f) * 0.5f));
        if (ImGui::Toggle("##toggle", value, ImGuiToggleFlags_Animated, 0.2f))
            changed = true;

        // 下一行：回到行起点（屏幕坐标），行间距 = ItemSpacing
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + rowHeight + ImGui::GetStyle().ItemSpacing.y));
        ImGui::PopID();
        return changed;
    }

    // 设置行：左侧文字 + 右侧滑块
    bool SettingsSliderRow(const char* label, float* value, float vMin, float vMax, const char* format)
    {
        bool changed = false;
        ImGui::PushID(label);

        const float rowHeight = ImGui::GetFrameHeight() + 10.0f;
        // 行内边距：左右 = style.padding × 3（对称，基于内容区宽度，不依赖当前光标 x）
        const float padX = ImGui::GetStyle().WindowPadding.x * 3.0f;
        const float contentW = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        const float rowWidth = contentW - padX * 2.0f;
        // 行起点：内容区左 + padX（强制复位，避免受标题/前序控件推进的光标 x 影响）
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMin().x + padX, ImGui::GetCursorPos().y));
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = ImVec2(rowMin.x + rowWidth, rowMin.y + rowHeight);

        // 行背景：hover 平滑过渡（key = label 地址，唯一）
        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawSettingsRowBg(dl, rowMin, rowMax, label);

        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + (rowHeight - textSize.y) * 0.5f));
        ImGui::TextUnformatted(label);

        // 右侧滑块：占行宽 45%
        const float controlWidth = rowWidth * 0.45f;
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - controlWidth - 6.0f,
                                         rowMin.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f));
        ImGui::PushItemWidth(controlWidth);
        if (ImGui::SliderFloat("##slider", value, vMin, vMax, format))
            changed = true;
        ImGui::PopItemWidth();

        // 下一行：回到行起点（屏幕坐标），行间距 = ItemSpacing
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + rowHeight + ImGui::GetStyle().ItemSpacing.y));
        ImGui::PopID();
        return changed;
    }

    // 设置行：左侧文字 + 右侧下拉选择
    bool SettingsDropdownRow(const char* label, const char* const* items, int itemCount, int* selectedIndex)
    {
        bool changed = false;
        ImGui::PushID(label);

        const float rowHeight = ImGui::GetFrameHeight() + 10.0f;
        // 行内边距：左右 = style.padding × 3（对称，基于内容区宽度，不依赖当前光标 x）
        const float padX = ImGui::GetStyle().WindowPadding.x * 3.0f;
        const float contentW = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
        const float rowWidth = contentW - padX * 2.0f;
        // 行起点：内容区左 + padX（强制复位，避免受标题/前序控件推进的光标 x 影响）
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMin().x + padX, ImGui::GetCursorPos().y));
        ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImVec2 rowMax = ImVec2(rowMin.x + rowWidth, rowMin.y + rowHeight);

        // 行背景：hover 平滑过渡（key = label 地址，唯一）
        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawSettingsRowBg(dl, rowMin, rowMax, label);

        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + (rowHeight - textSize.y) * 0.5f));
        ImGui::TextUnformatted(label);

        const float controlWidth = rowWidth * 0.45f;
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - controlWidth - 6.0f,
                                         rowMin.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f));
        ImGui::PushItemWidth(controlWidth);
        if (ImGui::Combo("##combo", selectedIndex, items, itemCount))
            changed = true;
        ImGui::PopItemWidth();

        // 下一行：回到行起点（屏幕坐标），行间距 = ItemSpacing
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMin.y + rowHeight + ImGui::GetStyle().ItemSpacing.y));
        ImGui::PopID();
        return changed;
    }

    // =========================================================================
    // 按钮变体（EUI button.h:51-52/65/70：前置图标 / 禁用态 / 按压缩放）
    // 图标与文字作为一个整体在按钮内居中；禁用态整体降 alpha 且不响应交互
    // =========================================================================
    bool GuiButtonEx(const char* label, const ImVec2& size, const char* icon,
                     bool disabled, float pressScale)
    {
        GuiThemeTokens tokens = Theme::Dark();
        const float width = size.x > 0.0f ? size.x : 120.0f;
        const float height = size.y > 0.0f ? size.y : 36.0f;
        const float rounding = tokens.radius.control;

        ImGui::PushID(label);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        bool clicked = false;
        bool hovered = false;
        bool active = false;
        if (disabled)
        {
            // 禁用态仍占位，避免后续布局跳动
            ImGui::Dummy(ImVec2(width, height));
        }
        else
        {
            clicked = ImGui::InvisibleButton("##gui_button_ex", ImVec2(width, height));
            hovered = ImGui::IsItemHovered();
            active = ImGui::IsItemActive();
        }

        // 三态背景：normal / hover（变亮）/ active（变暗）
        GuiColor bg = tokens.surface;
        if (active)
        {
            bg = ActiveColor(tokens, tokens.surface);
        }
        else if (hovered)
        {
            bg = HoverColor(tokens, tokens.surface);
        }

        // 按压缩放（button.h:70 pressScale）：绕按钮中心缩放
        const float scale = active ? std::clamp(pressScale, 0.90f, 1.0f) : 1.0f;
        const ImVec2 center(pos.x + width * 0.5f, pos.y + height * 0.5f);
        const ImVec2 halfSize(width * 0.5f * scale, height * 0.5f * scale);
        const ImVec2 min(center.x - halfSize.x, center.y - halfSize.y);
        const ImVec2 max(center.x + halfSize.x, center.y + halfSize.y);

        const float alphaScale = disabled ? 0.45f : 1.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, WithAlpha(bg, bg.a * alphaScale).ToImU32(), rounding);
        drawList->AddRect(min, max, WithAlpha(tokens.border, 0.70f * alphaScale).ToImU32(),
                          rounding, ImDrawFlags_RoundCornersAll, 1.0f);

        // 图标 + 文字作为整体居中
        const std::string displayText = DisplayLabelText(label);
        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        const float iconFontSize = fontSize * 1.05f;
        const bool hasIcon = (icon != nullptr && icon[0] != '\0' && font != nullptr);
        const bool hasText = (!displayText.empty() && font != nullptr);

        const ImVec2 iconExtent = hasIcon ? font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, icon)
                                          : ImVec2(0.0f, 0.0f);
        const ImVec2 textExtent = hasText ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, displayText.c_str())
                                          : ImVec2(0.0f, 0.0f);
        const float gap = (hasIcon && hasText) ? tokens.spacing.small : 0.0f;
        const float contentWidth = iconExtent.x + gap + textExtent.x;

        const ImU32 textColor = WithAlpha(tokens.text, alphaScale).ToImU32();
        float cursorX = center.x - contentWidth * 0.5f;
        if (hasIcon)
        {
            drawList->AddText(font, iconFontSize,
                              ImVec2(cursorX, center.y - iconExtent.y * 0.5f), textColor, icon);
            cursorX += iconExtent.x + gap;
        }
        if (hasText)
        {
            drawList->AddText(font, fontSize,
                              ImVec2(cursorX, center.y - textExtent.y * 0.5f), textColor,
                              displayText.c_str());
        }

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 步进器变体（EUI stepper.h:61-66：进制 / 补零 / 位宽 / 前缀 / 大写）
    // 值按 base 格式化显示，- / + 按钮按 step 增减并 clamp 到 [minValue, maxValue]
    // =========================================================================
    bool GuiStepperEx(const char* label, int* value, const GuiStepperOptions& options)
    {
        if (value == nullptr)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark();
        const float width = 180.0f;
        const float height = 34.0f;
        const float buttonWidth = 32.0f;
        const int base = (options.base == 2 || options.base == 8 || options.base == 16) ? options.base : 10;
        const int minValue = std::min(options.minValue, options.maxValue);
        const int maxValue = std::max(options.minValue, options.maxValue);

        ImGui::PushID(label);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // === 值格式化：进制 → 位宽/位数补零 → 前缀 → 大写 ===
        char valueText[64];
        {
            char digits[40];
            int digitCount = 0;
            const long long signedValue = static_cast<long long>(*value);
            const bool negative = (signedValue < 0);
            unsigned long long magnitude = negative
                                               ? static_cast<unsigned long long>(-signedValue)
                                               : static_cast<unsigned long long>(signedValue);
            do
            {
                const int digit = static_cast<int>(magnitude % static_cast<unsigned long long>(base));
                digits[digitCount++] = static_cast<char>(digit < 10
                                                            ? ('0' + digit)
                                                            : ((options.uppercase ? 'A' : 'a') + digit - 10));
                magnitude /= static_cast<unsigned long long>(base);
            } while (magnitude > 0 && digitCount < 39);

            // 目标位数：digits 优先，其次按 bitWidth 换算（2/8/16 进制）
            int targetDigits = options.digits;
            if (targetDigits <= 0 && options.bitWidth > 0 && (base == 2 || base == 8 || base == 16))
            {
                int bitsPerDigit = 1;
                while ((1 << bitsPerDigit) < base)
                {
                    ++bitsPerDigit;
                }
                targetDigits = (options.bitWidth + bitsPerDigit - 1) / bitsPerDigit;
            }

            int writePos = 0;
            if (options.showBasePrefix)
            {
                if (base == 2)
                {
                    valueText[writePos++] = '0';
                    valueText[writePos++] = 'b';
                }
                else if (base == 8)
                {
                    valueText[writePos++] = '0';
                    valueText[writePos++] = 'o';
                }
                else if (base == 16)
                {
                    valueText[writePos++] = '0';
                    valueText[writePos++] = options.uppercase ? 'X' : 'x';
                }
            }
            if (negative)
            {
                valueText[writePos++] = '-';
            }
            for (int pad = digitCount; pad < targetDigits; ++pad)
            {
                valueText[writePos++] = '0';
            }
            for (int index = digitCount - 1; index >= 0; --index)
            {
                valueText[writePos++] = digits[index];
            }
            valueText[writePos] = '\0';
        }

        // === 交互：左侧减号 / 右侧加号 ===
        ImGui::PushID("##gui_stepper_minus");
        ImGui::SetCursorScreenPos(pos);
        const bool minusClicked = ImGui::InvisibleButton("##minus", ImVec2(buttonWidth, height));
        const bool minusHovered = ImGui::IsItemHovered();
        ImGui::PopID();

        ImGui::PushID("##gui_stepper_plus");
        ImGui::SetCursorScreenPos(ImVec2(pos.x + width - buttonWidth, pos.y));
        const bool plusClicked = ImGui::InvisibleButton("##plus", ImVec2(buttonWidth, height));
        const bool plusHovered = ImGui::IsItemHovered();
        ImGui::PopID();

        // === 外观：底 surface + 边框 + 按钮 hover 高亮 + 居中数值 ===
        const float rounding = tokens.radius.control;
        const ImVec2 maxPos(pos.x + width, pos.y + height);
        drawList->AddRectFilled(pos, maxPos, tokens.surface.ToImU32(), rounding);
        drawList->AddRect(pos, maxPos, WithAlpha(tokens.border, 0.70f).ToImU32(),
                          rounding, ImDrawFlags_RoundCornersAll, 1.0f);

        const ImU32 hoverColor = WithAlpha(tokens.surfaceHover, 0.85f).ToImU32();
        if (minusHovered)
        {
            drawList->AddRectFilled(pos, ImVec2(pos.x + buttonWidth, maxPos.y), hoverColor,
                                    rounding, ImDrawFlags_RoundCornersLeft);
        }
        if (plusHovered)
        {
            drawList->AddRectFilled(ImVec2(maxPos.x - buttonWidth, pos.y), maxPos, hoverColor,
                                    rounding, ImDrawFlags_RoundCornersRight);
        }

        ImFont* font = ImGui::GetFont();
        if (font != nullptr)
        {
            const float iconSize = tokens.typography.body;
            const char* minusIcon = ICON_FA_MINUS;
            const char* plusIcon = ICON_FA_PLUS;
            const ImVec2 minusExtent = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, minusIcon);
            const ImVec2 plusExtent = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, plusIcon);
            drawList->AddText(font, iconSize,
                              ImVec2(pos.x + (buttonWidth - minusExtent.x) * 0.5f,
                                     pos.y + (height - minusExtent.y) * 0.5f),
                              tokens.text.ToImU32(), minusIcon);
            drawList->AddText(font, iconSize,
                              ImVec2(maxPos.x - buttonWidth + (buttonWidth - plusExtent.x) * 0.5f,
                                     pos.y + (height - plusExtent.y) * 0.5f),
                              tokens.text.ToImU32(), plusIcon);

            const float valueFontSize = tokens.typography.option;
            const ImVec2 valueExtent = font->CalcTextSizeA(valueFontSize, FLT_MAX, 0.0f, valueText);
            drawList->AddText(font, valueFontSize,
                              ImVec2(pos.x + (width - valueExtent.x) * 0.5f,
                                     pos.y + (height - valueExtent.y) * 0.5f),
                              tokens.text.ToImU32(), valueText);
        }

        // 推进光标到控件下方
        ImGui::SetCursorScreenPos(ImVec2(pos.x, maxPos.y));
        ImGui::PopID();

        // === 数值更新 ===
        bool changed = false;
        int next = *value;
        if (minusClicked)
        {
            next -= options.step;
        }
        if (plusClicked)
        {
            next += options.step;
        }
        if (next != *value)
        {
            *value = std::clamp(next, minValue, maxValue);
            changed = true;
        }
        return changed;
    }
}
