#pragma once
#include "SettingsButton.h"
#include "../Component.h"

// ComponentCard：组件卡片
// 布局：[图标+名称+描述] .... [Toggle开关] [设置齿轮]
// - 点击卡片内容区域 → 进入组件设置
// - Toggle → 控制组件显隐
// - 齿轮按钮 → 进入组件设置
// - 卡片背景有 hover 动画，无点击缩放
class ComponentCard
{
public:
    ComponentCard(Component* comp)
    {
        m_comp = comp;
        // 设置按钮（齿轮图标），缩小为 36x36
        m_settingButton = new IconButton(ICON_FA_GEAR, ImVec2(36.0f, 36.0f));
        // 计算卡片内容区域宽度（为 Toggle 和设置按钮留出空间）
        // 布局：[CardButton] gap [Toggle] gap [SettingsBtn] rightMargin
        m_toggleWidth = 42.0f;
        m_settingsBtnSize = 36.0f;
        m_innerGap = 12.0f;
        m_rightMargin = 15.0f;
        m_cardButtonWidth = moduleCardWidth - m_toggleWidth - m_settingsBtnSize - m_innerGap * 2 - m_rightMargin;
        m_cardButton = new CardButton(
            comp->GetIcon(),
            comp->GetName().c_str(),
            comp->GetDescription(),
            ImVec2(m_cardButtonWidth, cardButtonSize.y));
    }

    ~ComponentCard()
    {
        delete m_cardButton;
        delete m_settingButton;
    }

    bool Draw()
    {
        bool joinSetting = false;
        ImVec2 cursorPos = ImGui::GetCursorPos();
        m_startPos = cursorPos;

        // 卡片背景（hover 动画）
        ImVec2 screenPos = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 cardMin = screenPos;
        ImVec2 cardMax = ImVec2(screenPos.x + moduleCardWidth, screenPos.y + cardButtonSize.y);

        // 检测整张卡片是否被 hover（排除 Toggle 和设置按钮区域）
        bool cardHovered = ImGui::IsMouseHoveringRect(cardMin, cardMax, false) && ImGui::IsWindowHovered();

        // hover 动画插值
        float dt = ImGui::GetIO().DeltaTime;
        Anim::SmoothLerp(m_hoverAnim, cardHovered ? 1.0f : 0.0f, 15.0f, dt);

        // 卡片背景色：深色主题（rgb ≤ 40，带冷蓝调不发灰），hover 轻微提亮 + 半透明透出渐变
        ImVec4 bgNormal = ImVec4(0.133f, 0.137f, 0.157f, 0.85f);   // (34,35,40)
        ImVec4 bgHovered = ImVec4(0.149f, 0.153f, 0.157f, 0.92f);  // (38,39,40)
        ImVec4 bgColor(
            ImLerp(bgNormal.x, bgHovered.x, m_hoverAnim),
            ImLerp(bgNormal.y, bgHovered.y, m_hoverAnim),
            ImLerp(bgNormal.z, bgHovered.z, m_hoverAnim),
            1.0f);

        // 绘制卡片背景（8.0f 圆角）
        draw->AddRectFilled(cardMin, cardMax, ImGui::ColorConvertFloat4ToU32(bgColor), 8.0f);

        // 卡片内容（图标+名称+描述，点击进入设置）
        // 不再调用 SetSelected，CardButton 背景透明
        if (m_cardButton->Draw())
            joinSetting = true;
        if (m_cardButton->IsRightClicked())
            joinSetting = true;

        // Toggle 开关（卡片内，设置按钮左边，垂直居中）
        float toggleX = cursorPos.x + m_cardButtonWidth + m_innerGap;
        float toggleY = cursorPos.y + (cardButtonSize.y - 20.0f) * 0.5f;  // 20.0f ≈ Toggle 高度
        ImGui::SetCursorPos(ImVec2(toggleX, toggleY));
        bool visible = m_comp->IsVisible();
        ImGui::PushID(m_comp);  // 确保 Toggle ID 唯一
        if (ImGui::Toggle("##toggle", &visible, ImGuiToggleFlags_Animated, 0.2f))
            m_comp->SetVisible(visible);
        ImGui::PopID();

        // 设置按钮（齿轮，最右侧，垂直居中）
        float gearX = cursorPos.x + m_cardButtonWidth + m_innerGap + m_toggleWidth + m_innerGap;
        float gearY = cursorPos.y + (cardButtonSize.y - m_settingsBtnSize) * 0.5f;
        ImGui::SetCursorPos(ImVec2(gearX, gearY));
        if (m_settingButton->Draw())
            joinSetting = true;

        // 移动光标到下一行
        ImGui::SetCursorPos(ImVec2(m_startPos.x, m_startPos.y + cardButtonSize.y + 12.0f));
        return joinSetting;
    }

    Component* GetComponent() const
    {
        return m_comp;
    }

private:
    ImVec2 m_startPos = ImVec2(0.0f, 0.0f);
    float m_cardButtonWidth = 0.0f;
    float m_hoverAnim = 0.0f;       // hover 动画插值（0=正常, 1=hover）
    float m_toggleWidth = 42.0f;
    float m_settingsBtnSize = 36.0f;
    float m_innerGap = 12.0f;
    float m_rightMargin = 15.0f;
    Component* m_comp;
    CardButton* m_cardButton;
    IconButton* m_settingButton;
};
