#pragma once
#include "Anim.h"

// ============================================================
// 设置界面按钮系统
// 合并自原 AnimButtonBase.h / MyButton.hpp / PanelButton.hpp / ModuleButton.hpp
// ============================================================

// 按钮文字描述
struct ButtonText { const char* text; ImFont* font; };

// 按钮状态枚举
enum ButtonState { Normal, Selected, Hovered, Active };

// 文字动画目标值（用于插值）
struct TextAnimTarget
{
    float  fontSize;
    ImVec2 center;
    ImVec4 color;
    ImVec2 GetTextSize(const ButtonText& text) const
    {
        IM_ASSERT(text.font != nullptr);
        return text.font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.text);
    }
    ImVec2 CalculatePos(const ButtonText& text) const
    {
        ImVec2 textSize = GetTextSize(text);
        return ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
    }
    void CalculateCenter(const ImVec2& pos, const ButtonText& text)
    {
        ImVec2 textSize = GetTextSize(text);
        center = ImVec2(pos.x + textSize.x * 0.5f, pos.y + textSize.y * 0.5f);
    }
};

// 按钮动画目标值（用于插值）
struct ButtonAnimTarget
{
    ImVec2 size;
    ImVec2 center;
    ImVec4 color;
    ImVec4 borderColor;
    ImVec2 CalculatePos() const { return ImVec2(center.x - size.x / 2, center.y - size.y / 2); }
    void CalculateCenter(const ImVec2& pos) { center = ImVec2(pos.x + size.x / 2, pos.y + size.y / 2); }
};

// 动画参数 — 现代 UI 风格：幅度小、速度快
constexpr ImVec2 panelButtonSize   = ImVec2(150.0f, 50.0f);
constexpr float buttonSizeOffset   = 2.0f;   // 缩放幅度（原 5.0f → 2.0f）
constexpr float buttonHeightOffset = 1.0f;   // 垂直偏移幅度（原 3.0f → 1.0f）

// ============================================================
// ButtonBase：动画按钮基类
// ============================================================
class ButtonBase : public Anim
{
public:
    virtual bool Draw(ImDrawFlags flags = ImDrawFlags_RoundCornersAll) = 0;
    bool IsRightClicked() const { return rightClicked; }
protected:
    virtual void SetStateData() = 0;
    ImVec2 screenPos;
    ImVec2 lastScreenPos;
    float fontSize;
    float lastFontSize;
    ButtonState m_state = Normal;
    ButtonState lastState = Normal;
    bool initialized = false;
    bool rightClicked = false;
    bool skipAnim = false;
    float animSpeed = 30.0f;   // 动画速度（原 25.0f → 30.0f，更快）
    float m_padding = 8.0f;

    // 绘制不可见按钮（用于捕获鼠标事件）
    bool DrawInvisibleButton(const ButtonAnimTarget& current) const
    {
        ImVec2 buttonPos = current.CalculatePos();
        ImGui::PushID(this);
        bool pressed = ImGui::InvisibleButton("##btn", current.size, ImGuiButtonFlags_PressedOnClickRelease);
        ImGui::PopID();
        return pressed;
    }

    // 绘制文字标签（带阴影）
    static void DrawLabel(const TextAnimTarget& current, const ButtonText& labelText)
    {
        if (labelText.text == nullptr) return;
        ImVec2 pos = current.CalculatePos(labelText);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(current.color);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        float alpha = ImGui::ColorConvertU32ToFloat4(col).w * 0.5f;
        ImU32 shadowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, alpha));
        draw->AddText(labelText.font, current.fontSize, ImVec2(pos.x + 1.0f, pos.y + 1.0f), shadowCol, labelText.text);
        draw->AddText(labelText.font, current.fontSize, pos, col, labelText.text);
    }

    // 绘制背景
    static void DrawBackground(const ButtonAnimTarget& current, const ImDrawFlags& flags = ImDrawFlags_RoundCornersAll, float rounding = 8.0f)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = current.CalculatePos();
        draw->AddRectFilled(pos, ImVec2(pos.x + current.size.x, pos.y + current.size.y),
            ImGui::ColorConvertFloat4ToU32(current.color), rounding, flags);
    }

    // 绘制边框
    static void DrawBorder(const ButtonAnimTarget& current, const ImDrawFlags& flags = ImDrawFlags_RoundCornersAll, float borderSize = 1.0f, float rounding = 8.0f)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = current.CalculatePos();
        draw->AddRect(pos, ImVec2(pos.x + current.size.x, pos.y + current.size.y),
            ImGui::ColorConvertFloat4ToU32(current.borderColor), rounding, flags, borderSize);
    }

    // 设置下一行光标位置
    void SetNextCursorScreenPos() const
    {
        ImVec2 nextPos = screenPos;
        nextPos.y = screenPos.y + m_normalSize.y + m_padding;
        ImGui::SetCursorScreenPos(nextPos);
    }

    // 位置变化检测
    static bool IsPositionChanged(const ImVec2& pos, const ImVec2& lastPos) { return pos.x != lastPos.x || pos.y != lastPos.y; }
    static bool IsFontSizeChanged(const float& fs, const float& lastFs) { return lastFs != fs; }
    static ImVec2 CalPositionChangedValue(const ImVec2& pos, const ImVec2& lastPos) { return ImVec2(pos.x - lastPos.x, pos.y - lastPos.y); }
    static void ApplyButtonPositionChange(ButtonAnimTarget& button, const ImVec2& posChange) { button.center.x += posChange.x; button.center.y += posChange.y; }
    static void ApplyTextPositionChange(TextAnimTarget& text, const ImVec2& posChange) { text.center.x += posChange.x; text.center.y += posChange.y; }
    virtual void ApplyCenterPositionChange() = 0;

    // 插值函数
    static void LerpButton(ButtonAnimTarget& cur, const ButtonAnimTarget& target, float damping, float dt)
    {
        SmoothLerp(cur.size, target.size, damping, dt);
        SmoothLerp(cur.center, target.center, damping, dt);
        SmoothLerp(cur.color, target.color, damping, dt);
        SmoothLerp(cur.borderColor, target.borderColor, damping, dt);
    }
    static void LerpText(TextAnimTarget& cur, const TextAnimTarget& target, float damping, float dt)
    {
        SmoothLerp(cur.fontSize, target.fontSize, damping, dt);
        SmoothLerp(cur.center, target.center, damping, dt);
        SmoothLerp(cur.color, target.color, damping, dt);
    }
    static bool IsEqual(const ButtonAnimTarget& a, const ButtonAnimTarget& b)
    {
        return AlmostEqual(a.size, b.size) && AlmostEqual(a.center, b.center)
            && AlmostEqual(a.color, b.color) && AlmostEqual(a.borderColor, b.borderColor);
    }
    static bool IsEqual(const TextAnimTarget& a, const TextAnimTarget& b)
    {
        return AlmostEqual(a.fontSize, b.fontSize) && AlmostEqual(a.center, b.center) && AlmostEqual(a.color, b.color);
    }

    ImVec2 m_normalSize = panelButtonSize;  // 记录 normal 状态的尺寸，供 SetNextCursorScreenPos 使用
};

// ============================================================
// IconButton：简单图标/文字按钮（原 MyButton）
// 用于设置界面的关闭按钮、设置齿轮按钮等
// ============================================================
struct IconButtonStateData { ButtonAnimTarget button; TextAnimTarget label; };

class IconButton : public ButtonBase
{
public:
    IconButton(const char* labelText, const ImVec2& size = panelButtonSize, float padding = 20.0f)
    {
        this->labelText.text = labelText;
        m_padding = padding;
        m_normalSize = size;
        m_target = &m_normal;
    }

    bool Draw(ImDrawFlags flags = ImDrawFlags_RoundCornersAll) override
    {
        labelText.font = ImGui::GetFont();
        if (!initialized)
        {
            screenPos = ImGui::GetCursorScreenPos();
            lastScreenPos = screenPos;
            fontSize = ImGui::GetFontSize();
            lastFontSize = fontSize;
            SetStateData();
            m_current = m_normal;
            initialized = true;
        }
        screenPos = ImGui::GetCursorScreenPos();
        if (IsPositionChanged(screenPos, lastScreenPos)) { SetStateData(); ApplyCenterPositionChange(); lastScreenPos = screenPos; }
        fontSize = ImGui::GetFontSize();
        if (IsFontSizeChanged(fontSize, lastFontSize)) { SetStateData(); skipAnim = false; lastFontSize = fontSize; }

        bool pressed = DrawInvisibleButton(m_current.button);
        rightClicked = ImGui::IsItemClicked(1);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        UpdateState(hovered, active);
        if (lastState != m_state) { skipAnim = false; lastState = m_state; }
        if (!IsAnimating()) skipAnim = true;
        if (!skipAnim) { LerpAll(m_current, *m_target, animSpeed, ImGui::GetIO().DeltaTime); }

        DrawBackground(m_current.button, flags);
        DrawBorder(m_current.button, flags);
        DrawLabel(m_current.label, labelText);
        SetNextCursorScreenPos();
        return pressed;
    }

    void SetSelected(bool selected) { m_state = selected ? Selected : Normal; m_target = m_state == Selected ? &m_selected : &m_normal; }
    void SetLabelText(const char* text) { labelText.text = text; }
    bool IsSelected() const { return m_state == Selected; }
    void SetSize(const ImVec2 size) { m_normalSize = size; SetStateData(); }

protected:
    ButtonText labelText;
    IconButtonStateData m_normal, m_selected, m_hovered, m_active, m_current;
    IconButtonStateData* m_target;

    void UpdateState(bool hovered, bool active)
    {
        if (m_state == Selected)
        {
            m_state = active ? Active : (hovered ? Selected : Selected);
        }
        else
        {
            m_state = active ? Active : (hovered ? Hovered : Normal);
        }
        switch (m_state)
        {
        case Normal:   m_target = &m_normal;   break;
        case Selected: m_target = &m_selected; break;
        case Hovered:  m_target = &m_hovered;  break;
        case Active:   m_target = &m_active;   break;
        }
    }

    void ApplyCenterPositionChange() override
    {
        ImVec2 value = CalPositionChangedValue(screenPos, lastScreenPos);
        ApplyButtonPositionChange(m_current.button, value);
        ApplyTextPositionChange(m_current.label, value);
    }

    void SetStateData() override
    {
        SetNormalStateData();
        SetSelectedStateData(m_normal);
        SetHoveredStateData(m_normal);
        SetActiveStateData(m_normal);
    }

    void SetNormalStateData()
    {
        m_normal.button.size = m_normalSize;
        m_normal.button.CalculateCenter(screenPos);
        m_normal.button.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Button));
        ImVec4 borderColor = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Border));
        borderColor.w = 0.3f;
        m_normal.button.borderColor = borderColor;
        m_normal.label.fontSize = fontSize;
        m_normal.label.center = ImVec2(m_normal.button.center.x, m_normal.button.center.y);
        m_normal.label.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_TextDisabled));
    }

    void SetSelectedStateData(const IconButtonStateData& normal)
    {
        m_selected.button.size = normal.button.size;
        m_selected.button.center = normal.button.center;  // 居中：center 不变
        m_selected.button.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        m_selected.button.borderColor = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Border));
        m_selected.label.center = normal.button.center;  // 图标居中
        m_selected.label.fontSize = fontSize;
        m_selected.label.color = ImVec4(0.1f, 0.8f, 0.3f, 1.0f);
    }

    void SetHoveredStateData(const IconButtonStateData& normal)
    {
        m_hovered.button.size = ImVec2(normal.button.size.x + buttonSizeOffset, normal.button.size.y + buttonSizeOffset);
        m_hovered.button.center = normal.button.center;  // 修复：center 保持不变，缩放围绕中心
        m_hovered.button.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        ImVec4 borderColor = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Border));
        borderColor.w = 0.5f;
        m_hovered.button.borderColor = borderColor;
        m_hovered.label.center = m_hovered.button.center;  // 图标跟随中心
        m_hovered.label.fontSize = fontSize;
        m_hovered.label.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
    }

    void SetActiveStateData(const IconButtonStateData& normal)
    {
        m_active.button.size = ImVec2(normal.button.size.x - buttonSizeOffset, normal.button.size.y - buttonSizeOffset);
        m_active.button.center = normal.button.center;  // 修复：center 保持不变
        m_active.button.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_ButtonActive));
        m_active.button.borderColor = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Border));
        m_active.label.center = m_active.button.center;
        m_active.label.fontSize = fontSize * 0.9f;
        m_active.label.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
    }

    void LerpAll(IconButtonStateData& cur, const IconButtonStateData& target, float damping, float dt)
    {
        LerpButton(cur.button, target.button, damping, dt);
        LerpText(cur.label, target.label, damping, dt);
    }
    bool IsAnimating() const { return !IsAllEqual(m_current, *m_target); }
    static bool IsAllEqual(const IconButtonStateData& a, const IconButtonStateData& b) { return IsEqual(a.button, b.button) && IsEqual(a.label, b.label); }
};

// ============================================================
// NavButton：导航按钮（原 PanelButton）
// 用于左栏导航，图标+文字水平排列
// ============================================================
struct NavButtonStateData { ButtonAnimTarget button; TextAnimTarget icon; TextAnimTarget label; };
constexpr float navTextSpacing = 5.0f;
// Win11 风格导航项：紧凑高度、无边框、左对齐图标+文字
constexpr ImVec2 navButtonSize = ImVec2(150.0f, 40.0f);
constexpr float navItemPadding = 8.0f;   // 导航项垂直间距

class NavButton : public ButtonBase
{
public:
    NavButton(const char* iconText, const char* labelText, bool isExit = false, const ImVec2& size = navButtonSize, float padding = navItemPadding)
    {
        this->isExitButton = isExit;
        this->iconText.text = iconText;
        this->labelText.text = labelText;
        m_padding = padding;
        m_normalSize = size;
    }

    bool Draw(ImDrawFlags flags = ImDrawFlags_RoundCornersAll) override
    {
        labelText.font = ImGui::GetFont();
        iconText.font = ImGui::GetFont();
        if (!initialized)
        {
            screenPos = ImGui::GetCursorScreenPos();
            lastScreenPos = screenPos;
            fontSize = ImGui::GetFontSize();
            lastFontSize = fontSize;
            SetStateData();
            m_current = m_normal;
            m_target = &m_normal;
            initialized = true;
        }
        screenPos = ImGui::GetCursorScreenPos();
        if (IsPositionChanged(screenPos, lastScreenPos)) { SetStateData(); ApplyCenterPositionChange(); lastScreenPos = screenPos; }
        fontSize = ImGui::GetFontSize();
        if (IsFontSizeChanged(fontSize, lastFontSize)) { SetStateData(); skipAnim = false; lastFontSize = fontSize; }

        bool pressed = DrawInvisibleButton(m_current.button);
        rightClicked = ImGui::IsItemClicked(1);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        UpdateState(hovered, active);
        if (lastState != m_state) { skipAnim = false; lastState = m_state; }
        if (!IsAnimating()) skipAnim = true;
        if (!skipAnim) { LerpAll(m_current, *m_target, animSpeed, ImGui::GetIO().DeltaTime); }

        DrawBackground(m_current.button, flags);
        DrawBorder(m_current.button, flags);
        // Win11 风格：选中项左侧主题色指示条（Catppuccin blue #8caaee）
        if (m_state == Selected)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 pos = m_current.button.CalculatePos();
            draw->AddRectFilled(
                ImVec2(pos.x, pos.y + 6.0f),
                ImVec2(pos.x + 3.5f, pos.y + m_current.button.size.y - 6.0f),
                IM_COL32(140, 170, 238, 230), 2.0f);
        }
        DrawLabel(m_current.icon, iconText);
        DrawLabel(m_current.label, labelText);
        SetNextCursorScreenPos();
        return pressed;
    }

    void SetSelected(bool selected) { m_state = selected ? Selected : Normal; }
    bool IsSelected() const { return m_state == Selected; }

private:
    bool isExitButton = false;
    ButtonText iconText;
    float iconFontSize = 24.0f;
    ButtonText labelText;
    NavButtonStateData m_normal, m_selected, m_hovered, m_active, m_current;
    NavButtonStateData* m_target;

    void UpdateState(bool hovered, bool active)
    {
        if (m_state == Selected)
            m_state = active ? Active : (hovered ? Selected : Selected);
        else
            m_state = active ? Active : (hovered ? Hovered : Normal);
        switch (m_state)
        {
        case Normal:   m_target = &m_normal;   break;
        case Selected: m_target = &m_selected; break;
        case Hovered:  m_target = &m_hovered;  break;
        case Active:   m_target = &m_active;   break;
        }
    }

    void ApplyCenterPositionChange() override
    {
        ImVec2 value = CalPositionChangedValue(screenPos, lastScreenPos);
        ApplyButtonPositionChange(m_current.button, value);
        ApplyTextPositionChange(m_current.label, value);
        ApplyTextPositionChange(m_current.icon, value);
    }

    void SetStateData() override
    {
        SetNormalStateData();
        SetSelectedStateData(m_normal);
        SetHoveredStateData(m_normal);
        SetActiveStateData(m_normal);
    }

    void SetNormalStateData()
    {
        m_normal.button.size = m_normalSize;
        m_normal.button.CalculateCenter(screenPos);
        m_normal.button.color = ImVec4(0, 0, 0, 0);            // 透明背景（Win11 列表项无底色无边框）
        m_normal.button.borderColor = ImVec4(0, 0, 0, 0);
        m_normal.icon.fontSize = iconFontSize;
        m_normal.label.fontSize = fontSize;
        // Catppuccin subtext0 #a5adce（闲置态文字）
        ImVec4 idleColor = ImVec4(0.647f, 0.678f, 0.808f, 1.0f);
        m_normal.icon.color = idleColor;
        m_normal.label.color = idleColor;
        ComputeTextLayoutLeft(m_normal.icon, m_normal.label, m_normal.button, navTextSpacing);
    }

    void SetSelectedStateData(const NavButtonStateData& normal)
    {
        m_selected.button.size = normal.button.size;
        m_selected.button.center = normal.button.center;  // 居中
        // Catppuccin blue #8caaee 淡背景（选中高亮）+ 无边框
        m_selected.button.color = ImVec4(0.549f, 0.667f, 0.933f, 0.22f);
        m_selected.button.borderColor = ImVec4(0, 0, 0, 0);
        m_selected.icon.fontSize = iconFontSize;
        m_selected.icon.color = ImVec4(0.549f, 0.667f, 0.933f, 1.0f);   // blue
        m_selected.label.fontSize = fontSize;
        m_selected.label.color = isExitButton ? ImVec4(0.906f, 0.510f, 0.518f, 1.0f) : ImVec4(0.549f, 0.667f, 0.933f, 1.0f);  // red / blue
        ComputeTextLayoutLeft(m_selected.icon, m_selected.label, m_selected.button, navTextSpacing);
    }

    void SetHoveredStateData(const NavButtonStateData& normal)
    {
        m_hovered.button.size = normal.button.size;            // 无缩放动画
        m_hovered.button.center = normal.button.center;
        // Catppuccin text 淡白背景（hover）
        m_hovered.button.color = ImVec4(0.776f, 0.816f, 0.961f, 0.08f);
        m_hovered.button.borderColor = ImVec4(0, 0, 0, 0);
        m_hovered.icon.fontSize = iconFontSize;
        m_hovered.icon.color = ImVec4(0.776f, 0.816f, 0.961f, 1.0f);   // text #c6d0f5
        m_hovered.label.fontSize = fontSize;
        m_hovered.label.color = m_hovered.icon.color;
        ComputeTextLayoutLeft(m_hovered.icon, m_hovered.label, m_hovered.button, navTextSpacing);
    }

    void SetActiveStateData(const NavButtonStateData& normal)
    {
        // 按下：选中色略深，无缩放
        m_active = m_selected;
        m_active.button.color = ImVec4(0.549f, 0.667f, 0.933f, 0.32f);   // blue 加深
        ComputeTextLayoutLeft(m_active.icon, m_active.label, m_active.button, navTextSpacing);
    }

    // 左对齐布局（Win11 NavigationView）：图标在左侧内边距处，文字紧随其后
    void ComputeTextLayoutLeft(TextAnimTarget& icon, TextAnimTarget& label, const ButtonAnimTarget& button, float spacing) const
    {
        float icon_w = iconText.font->CalcTextSizeA(icon.fontSize, FLT_MAX, 0, iconText.text).x;
        float label_w = labelText.font->CalcTextSizeA(label.fontSize, FLT_MAX, 0, labelText.text).x;
        float left = button.center.x - button.size.x * 0.5f + 14.0f;
        icon.center.x = left + icon_w * 0.5f;
        label.center.x = left + icon_w + spacing + label_w * 0.5f;
        icon.center.y = button.center.y;
        label.center.y = button.center.y;
    }

    void LerpAll(NavButtonStateData& cur, const NavButtonStateData& target, float damping, float dt)
    {
        LerpButton(cur.button, target.button, damping, dt);
        LerpText(cur.icon, target.icon, damping, dt);
        LerpText(cur.label, target.label, damping, dt);
    }
    bool IsAnimating() const { return !IsAllEqual(m_current, *m_target); }
    static bool IsAllEqual(const NavButtonStateData& a, const NavButtonStateData& b)
    { return IsEqual(a.button, b.button) && IsEqual(a.icon, b.icon) && IsEqual(a.label, b.label); }
};

// ============================================================
// CardButton：组件卡片按钮（原 ModuleButton）
// 用于主面板组件列表，图标+名称+描述
// ============================================================
struct CardButtonStateData { ButtonAnimTarget button; TextAnimTarget icon; TextAnimTarget label; TextAnimTarget description; };
constexpr ImVec2 cardButtonSize = ImVec2(548.0f, 60.0f);

class CardButton : public ButtonBase
{
public:
    CardButton(const char* iconText, const char* labelText, const char* descriptionText, const ImVec2& size = cardButtonSize, float padding = 20.0f)
    {
        this->iconText.text = iconText;
        this->labelText.text = labelText;
        this->descriptionText.text = descriptionText;
        m_padding = padding;
        m_normalSize = size;
    }

    bool Draw(ImDrawFlags flags = ImDrawFlags_RoundCornersAll) override
    {
        labelText.font = ImGui::GetFont();
        descriptionText.font = labelText.font;
        iconText.font = ImGui::GetFont();
        if (!initialized)
        {
            screenPos = ImGui::GetCursorScreenPos();
            lastScreenPos = screenPos;
            fontSize = ImGui::GetFontSize();
            lastFontSize = fontSize;
            SetStateData();
            m_current = m_normal;
            m_target = &m_normal;
            initialized = true;
        }
        screenPos = ImGui::GetCursorScreenPos();
        if (IsPositionChanged(screenPos, lastScreenPos)) { SetStateData(); ApplyCenterPositionChange(); lastScreenPos = screenPos; }
        fontSize = ImGui::GetFontSize();
        if (IsFontSizeChanged(fontSize, lastFontSize)) { SetStateData(); skipAnim = false; lastFontSize = fontSize; }

        bool pressed = DrawInvisibleButton(m_current.button);
        rightClicked = ImGui::IsItemClicked(1);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        UpdateState(hovered, active);
        if (lastState != m_state) { skipAnim = false; lastState = m_state; }
        if (!IsAnimating()) skipAnim = true;
        if (!skipAnim) { LerpAll(m_current, *m_target, animSpeed, ImGui::GetIO().DeltaTime); }

        DrawBackground(m_current.button, flags);
        DrawBorder(m_current.button, flags, 1.0f);
        DrawLabel(m_current.icon, iconText);
        DrawLabel(m_current.label, labelText);
        DrawLabel(m_current.description, descriptionText);
        SetNextCursorScreenPos();
        return pressed;
    }

    void SetSelected(bool selected) { (void)selected; /* 不再使用边框颜色表示开关状态，由 Toggle 控制 */ }
    bool IsSelected() const { return m_state == Selected; }

private:
    ButtonText iconText;
    float iconFontSize = 24.0f;
    ButtonText labelText;
    ButtonText descriptionText;
    CardButtonStateData m_normal, m_selected, m_hovered, m_active, m_current;
    CardButtonStateData* m_target;

    void UpdateState(bool hovered, bool active)
    {
        if (m_state == Selected)
            m_state = active ? Active : (hovered ? Hovered : Selected);
        else
            m_state = active ? Active : (hovered ? Hovered : Normal);
        switch (m_state)
        {
        case Normal:   m_target = &m_normal;   break;
        case Selected: m_target = &m_selected; break;
        case Hovered:  m_target = &m_hovered;  break;
        case Active:   m_target = &m_active;   break;
        }
    }

    void ApplyCenterPositionChange() override
    {
        ImVec2 value = CalPositionChangedValue(screenPos, lastScreenPos);
        ApplyButtonPositionChange(m_current.button, value);
        ApplyTextPositionChange(m_current.label, value);
        ApplyTextPositionChange(m_current.description, value);
        ApplyTextPositionChange(m_current.icon, value);
    }

    void SetStateData() override
    {
        SetNormalStateData();
        SetSelectedStateData(m_normal);
        SetHoveredStateData(m_normal);
        SetActiveStateData(m_normal);
    }

    void SetNormalStateData()
    {
        m_normal.button.size = m_normalSize;
        m_normal.button.CalculateCenter(screenPos);
        // 透明背景，由 ComponentCard 统一绘制卡片背景
        m_normal.button.color = ImVec4(0, 0, 0, 0);
        m_normal.button.borderColor = ImVec4(0, 0, 0, 0);
        m_normal.icon.fontSize = iconFontSize;
        m_normal.icon.center = ImVec2(screenPos.x + m_normal.button.size.y * 0.5f, m_normal.button.center.y);
        m_normal.icon.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_TextDisabled));
        m_normal.label.fontSize = fontSize;
        ImVec2 labelPos = ImVec2(screenPos.x + m_normal.button.size.y, screenPos.y + (m_normal.button.size.y - m_normal.label.fontSize) * 0.5f);
        m_normal.label.CalculateCenter(labelPos, labelText);
        m_normal.label.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
        m_normal.description.fontSize = fontSize * 0.8f;
        ImVec2 descPos = ImVec2(screenPos.x + m_normal.button.size.y, screenPos.y + (m_normal.button.size.y - m_normal.description.fontSize) * 0.5f);
        m_normal.description.CalculateCenter(descPos, descriptionText);
        ImVec4 descColor = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_TextDisabled));
        descColor.w = 0.0f;
        m_normal.description.color = descColor;
    }

    void SetSelectedStateData(const CardButtonStateData& normal)
    {
        m_selected = normal;  // 不再区分选中状态
    }

    void SetHoveredStateData(const CardButtonStateData& normal)
    {
        // 保留轻微缩放动画增加美感，背景仍透明（由 ComponentCard 处理）
        m_hovered.button.size = ImVec2(normal.button.size.x + buttonSizeOffset, normal.button.size.y + buttonSizeOffset);
        m_hovered.button.center = normal.button.center;
        m_hovered.button.color = ImVec4(0, 0, 0, 0);
        m_hovered.button.borderColor = ImVec4(0, 0, 0, 0);
        m_hovered.icon.fontSize = iconFontSize;
        m_hovered.icon.center = normal.icon.center;
        m_hovered.icon.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
        m_hovered.label.fontSize = fontSize;
        m_hovered.label.color = normal.label.color;
        m_hovered.label.center.x = normal.label.center.x;
        m_hovered.description.fontSize = fontSize * 0.8f;
        m_hovered.description.color = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_TextDisabled));
        m_hovered.description.center.x = normal.description.center.x;
        ComputeTextLayoutVertical(m_hovered.label, m_hovered.description, m_hovered.button, 0.0f);
    }

    void SetActiveStateData(const CardButtonStateData& normal)
    {
        // 移除点击缩放动画，Active 与 Hovered 一致
        m_active = m_hovered;
    }

    static void ComputeTextLayoutVertical(TextAnimTarget& label, TextAnimTarget& description, const ButtonAnimTarget& button, float spacing)
    {
        float label_h = label.fontSize;
        float desc_h = description.fontSize;
        float total_h = label_h + spacing + desc_h;
        float start_y = button.center.y - total_h * 0.5f;
        label.center.y = start_y + label_h * 0.5f;
        description.center.y = start_y + label_h + spacing + desc_h * 0.5f;
    }

    void LerpAll(CardButtonStateData& cur, const CardButtonStateData& target, float damping, float dt)
    {
        LerpButton(cur.button, target.button, damping, dt);
        LerpText(cur.icon, target.icon, damping, dt);
        LerpText(cur.label, target.label, damping, dt);
        LerpText(cur.description, target.description, damping, dt);
    }
    bool IsAnimating() const { return !IsAllEqual(m_current, *m_target); }
    static bool IsAllEqual(const CardButtonStateData& a, const CardButtonStateData& b)
    { return IsEqual(a.button, b.button) && IsEqual(a.icon, b.icon) && IsEqual(a.label, b.label) && IsEqual(a.description, b.description); }
};
