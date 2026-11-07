#pragma once
#include <vector>
#include "SettingsButton.h"

// ============================================================
// NavPanel：左栏导航面板（原 SelectPanel）
// 包含多个 NavButton，退出按钮固定在底部
// ============================================================
class NavPanel
{
public:
    NavPanel()
    {
        buttons.emplace_back(ICON_FA_LIST,                      "组件列表");
        buttons.emplace_back(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "位置调整");
        buttons.emplace_back(ICON_FA_GEAR,                      "设置");
        exitButton = new NavButton(ICON_FA_RIGHT_FROM_BRACKET,  "退出程序", true);
    }

    ~NavPanel() { delete exitButton; }

    // 绘制导航面板，panelHeight 为左栏总高度，退出按钮固定在底部
    bool Draw(float panelHeight)
    {
        bool clicked = false;

        // 上方导航按钮
        for (int i = 0; i < (int)buttons.size(); i++)
        {
            buttons[i].SetSelected(i == selectedButtonIndex);
            if (buttons[i].Draw())
            {
                selectedButtonIndex = i;
                clicked = true;
            }
        }

        // 退出按钮固定在底部
        ImVec2 currentPos = ImGui::GetCursorScreenPos();
        float exitButtonTotalHeight = navButtonSize.y + 20.0f;  // 按钮高度 + 底部 padding
        float usedHeight = currentPos.y - ImGui::GetWindowPos().y;
        float remainingSpace = panelHeight - usedHeight - exitButtonTotalHeight;
        if (remainingSpace > 0)
            ImGui::SetCursorScreenPos(ImVec2(currentPos.x, currentPos.y + remainingSpace));
        if (exitButton->Draw())
        {
            selectedButtonIndex = (int)buttons.size();  // 退出按钮索引 = 导航按钮数量
            clicked = true;
        }

        return clicked;
    }

    int  GetSelectedIndex() const { return selectedButtonIndex; }
    void SetSelectedIndex(int index) { selectedButtonIndex = index; }

private:
    std::vector<NavButton> buttons;
    NavButton* exitButton = nullptr;
    int selectedButtonIndex = 0;
};

// ============================================================
// FilterBar：搜索过滤（原 CategoryBar，已移除分类按钮，仅保留搜索框）
// ============================================================
class FilterBar
{
public:
    FilterBar() {}

    bool Draw()
    {
        ImVec2 curPos = ImGui::GetCursorPos();
        // 保持搜索框原有尺寸与位置：模拟原 5 个分类按钮占用的宽度，搜索框定位在原按钮结束位置（靠右）
        curPos.x += 5.0f * (buttonSize.x + padding * 0.625f);
        curPos.y += 1.0f;
        ImGui::SetCursorPos(curPos);

        // 搜索框：高度与按钮一致（30px）
        float framePaddingY = (buttonSize.y - ImGui::GetFontSize()) * 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, framePaddingY));

        float windowWidth = ImGui::GetWindowWidth();
        bool changed = filter.Draw("##filter", windowWidth - curPos.x - padding);
        ImGui::PopStyleVar();

        // 搜索框为空且未聚焦时，右侧显示放大镜提示图标
        if (!filter.IsActive() && filter.InputBuf[0] == '\0')
        {
            curPos.y += framePaddingY + 2.0f;
            ImGui::SetCursorPos(ImVec2(windowWidth - padding - 25.0f, curPos.y));
            ImGui::BeginDisabled();
            ImGui::TextUnformatted(ICON_FA_MAGNIFYING_GLASS);
            ImGui::EndDisabled();
        }

        return changed;
    }

    float GetButtonHeight() const { return buttonSize.y; }
    ImGuiTextFilter* GetFilter() { return &filter; }

private:
    const float padding = 20.0f;
    ImVec2 buttonSize = ImVec2(80, 30);
    ImGuiTextFilter filter;
};
