// GuiNavigation.cpp
// 导航与容器控件的实现（声明见 BaseUI/GUIBase.h）
// 移植自 EUI-NEO components/tabs.h / carousel.h / datatable.h，
// 保留 EUI 暗色主题的视觉层级，改用 ImGui 立即模式 + ImDrawList 自绘

#include "GUIBase.h"

namespace GuiBase
{
    // =========================================================================
    // 标签页
    // 一行标签 + 底部边框线，选中项以 primary 色文字与下划线指示条高亮，
    // 点击标签切换选中项，选中项变化时返回 true
    // =========================================================================
    bool GuiTabs(const char* label, const char* const* tabs, int tabCount, int* selectedIndex)
    {
        if (tabs == nullptr || tabCount <= 0 || selectedIndex == nullptr)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark();
        ImGui::PushID(label);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float tabHeight = 40.0f;
        const float tabWidth = totalWidth / static_cast<float>(tabCount);
        const float textAlpha = 0.66f;
        const float indicatorHeight = 3.0f;
        const float indicatorInset = tabWidth * 0.18f;

        bool changed = false;
        int newIndex = *selectedIndex;
        if (newIndex < 0)
        {
            newIndex = 0;
        }
        if (newIndex >= tabCount)
        {
            newIndex = tabCount - 1;
        }

        for (int index = 0; index < tabCount; ++index)
        {
            const float x0 = cursor.x + static_cast<float>(index) * tabWidth;
            const bool selected = (index == newIndex);

            // 交互命中区域
            ImGui::PushID(index);
            const bool clicked = ImGui::InvisibleButton("##tab", ImVec2(tabWidth, tabHeight));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            if (clicked && index != newIndex)
            {
                newIndex = index;
                changed = true;
            }

            // hover 背景：未选中的标签悬浮时显示浅色表面
            if (hovered && !selected)
            {
                drawList->AddRectFilled(
                    ImVec2(x0, cursor.y),
                    ImVec2(x0 + tabWidth, cursor.y + tabHeight),
                    WithAlpha(tokens.surfaceHover, 0.55f).ToImU32(),
                    tokens.radius.control);
            }

            // 标签文字：选中用 primary 色，未选中用淡化文字色
            const char* text = tabs[index] != nullptr ? tabs[index] : "";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            const ImVec2 textPos(
                x0 + (tabWidth - textSize.x) * 0.5f,
                cursor.y + (tabHeight - textSize.y) * 0.5f);
            const ImU32 textCol = selected
                ? tokens.primary.ToImU32()
                : WithAlpha(tokens.text, textAlpha).ToImU32();
            drawList->AddText(textPos, textCol, text);

            // 选中指示条：底部圆角长条
            if (selected)
            {
                drawList->AddRectFilled(
                    ImVec2(x0 + indicatorInset, cursor.y + tabHeight - indicatorHeight),
                    ImVec2(x0 + tabWidth - indicatorInset, cursor.y + tabHeight),
                    tokens.primary.ToImU32(),
                    indicatorHeight * 0.5f);
            }
        }

        // 底部边框线
        drawList->AddLine(
            ImVec2(cursor.x, cursor.y + tabHeight - 1.0f),
            ImVec2(cursor.x + totalWidth, cursor.y + tabHeight - 1.0f),
            WithAlpha(tokens.border, 0.70f).ToImU32());

        // 推进光标到控件底部
        ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + tabHeight));

        if (changed)
        {
            *selectedIndex = newIndex;
        }
        ImGui::PopID();
        return changed;
    }

    // =========================================================================
    // 轮播
    // 固定尺寸的内容区展示当前项文字，左右各一个圆形箭头按钮，
    // 点击箭头循环切换选中项（越界项不响应），选中项变化时返回 true
    // =========================================================================
    bool GuiCarousel(const char* label, const char* const* items, int itemCount,
                     int* selectedIndex, float width, float height)
    {
        if (items == nullptr || itemCount <= 0 || selectedIndex == nullptr ||
            width <= 0.0f || height <= 0.0f)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark();
        ImGui::PushID(label);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float radius = tokens.radius.feature;
        const float buttonSize = 36.0f;
        const float buttonMargin = 12.0f;

        bool changed = false;
        int newIndex = *selectedIndex;
        if (newIndex < 0)
        {
            newIndex = 0;
        }
        if (newIndex >= itemCount)
        {
            newIndex = itemCount - 1;
        }

        // 内容区背景与边框
        drawList->AddRectFilled(
            cursor,
            ImVec2(cursor.x + width, cursor.y + height),
            tokens.surface.ToImU32(),
            radius);
        drawList->AddRect(
            cursor,
            ImVec2(cursor.x + width, cursor.y + height),
            WithAlpha(tokens.border, 0.62f).ToImU32(),
            radius,
            0,
            1.0f);

        // 当前项文字：水平垂直居中
        const char* text = items[newIndex] != nullptr ? items[newIndex] : "";
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 textPos(
            cursor.x + (width - textSize.x) * 0.5f,
            cursor.y + (height - textSize.y) * 0.5f);
        drawList->AddText(textPos, WithAlpha(tokens.text, 0.96f).ToImU32(), text);

        // 左右箭头按钮：圆形背景 + Font Awesome 图标
        // 越界方向上的箭头置为禁用态（半透明、不响应点击）
        const ImVec2 leftCenter(
            cursor.x + buttonMargin + buttonSize * 0.5f,
            cursor.y + height * 0.5f);
        const ImVec2 rightCenter(
            cursor.x + width - buttonMargin - buttonSize * 0.5f,
            cursor.y + height * 0.5f);

        auto drawArrowButton = [&](int buttonIndex, const ImVec2& center,
                                   const char* icon, bool enabled) -> bool
        {
            const float half = buttonSize * 0.5f;
            bool clicked = false;
            bool hovered = false;
            bool active = false;

            if (enabled)
            {
                ImGui::SetCursorScreenPos(ImVec2(center.x - half, center.y - half));
                ImGui::PushID(buttonIndex);
                clicked = ImGui::InvisibleButton("##arrow", ImVec2(buttonSize, buttonSize));
                hovered = ImGui::IsItemHovered();
                active = ImGui::IsItemActive();
                ImGui::PopID();
            }

            // 圆形背景：启用态按 hover/active 三态切换，禁用态半透明
            const GuiColor bgColor = enabled
                ? (active ? tokens.surfaceActive
                          : (hovered ? tokens.surfaceHover : tokens.surface))
                : WithAlpha(tokens.surface, 0.55f);
            drawList->AddCircleFilled(center, half, bgColor.ToImU32());
            drawList->AddCircle(center, half, WithAlpha(tokens.border, 0.62f).ToImU32(), 0, 1.0f);

            // 箭头图标居中
            const ImVec2 iconSize = ImGui::CalcTextSize(icon);
            const ImVec2 iconPos(
                center.x - iconSize.x * 0.5f,
                center.y - iconSize.y * 0.5f);
            const ImU32 iconCol = enabled
                ? tokens.text.ToImU32()
                : WithAlpha(tokens.text, 0.30f).ToImU32();
            drawList->AddText(iconPos, iconCol, icon);

            return clicked;
        };

        const bool canPrev = (newIndex > 0);
        const bool canNext = (newIndex < itemCount - 1);

        if (drawArrowButton(0, leftCenter, ICON_FA_CHEVRON_LEFT, canPrev))
        {
            newIndex = newIndex - 1;
            changed = true;
        }

        if (drawArrowButton(1, rightCenter, ICON_FA_CHEVRON_RIGHT, canNext))
        {
            newIndex = newIndex + 1;
            changed = true;
        }

        // 推进光标到控件底部
        ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + height));

        if (changed)
        {
            *selectedIndex = newIndex;
        }
        ImGui::PopID();
        return changed;
    }

    // =========================================================================
    // 数据表格
    // 表头行（header 背景）+ 数据行（交替行背景），列宽按表头数均分，
    // 单元格文字垂直居中，表头文字用 primary 色，数据文字用 text 色。
    // 纯绘制函数，底部以 Dummy 占位推进布局
    // =========================================================================
    void GuiDatatable(const char* label,
                      const char* const* headers, int headerCount,
                      const std::vector<std::vector<std::string>>& rows)
    {
        if (headers == nullptr || headerCount <= 0)
        {
            return;
        }

        GuiThemeTokens tokens = Theme::Dark();
        ImGui::PushID(label);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float tableWidth = ImGui::GetContentRegionAvail().x;
        const float headerHeight = 40.0f;
        const float rowHeight = 36.0f;
        const float columnWidth = tableWidth / static_cast<float>(headerCount);
        const float textInset = 16.0f;
        const float radius = tokens.radius.card;

        // EUI 暗色表格配色
        const GuiColor headerColor = MixColor(tokens.surfaceHover, tokens.surface, 0.32f);
        const GuiColor rowColor = tokens.surface;
        const GuiColor rowAltColor = MixColor(tokens.surface, tokens.surfaceHover, 0.30f);
        const GuiColor dividerColor = WithAlpha(tokens.border, 0.42f);
        const GuiColor borderColor = WithAlpha(tokens.border, 0.72f);

        const int rowCount = static_cast<int>(rows.size());
        float y = cursor.y;

        // 表头行
        const ImDrawFlags headerFlags = (rowCount > 0)
            ? (ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight)
            : ImDrawFlags_RoundCornersAll;
        drawList->AddRectFilled(
            ImVec2(cursor.x, y),
            ImVec2(cursor.x + tableWidth, y + headerHeight),
            headerColor.ToImU32(),
            radius,
            headerFlags);
        for (int column = 0; column < headerCount; ++column)
        {
            const char* text = headers[column] != nullptr ? headers[column] : "";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            const ImVec2 textPos(
                cursor.x + static_cast<float>(column) * columnWidth + textInset,
                y + (headerHeight - textSize.y) * 0.5f);
            drawList->AddText(textPos, tokens.primary.ToImU32(), text);
        }
        y += headerHeight;

        // 表头分隔线
        drawList->AddLine(
            ImVec2(cursor.x, y - 1.0f),
            ImVec2(cursor.x + tableWidth, y - 1.0f),
            dividerColor.ToImU32());

        // 数据行
        for (int row = 0; row < rowCount; ++row)
        {
            const bool lastRow = (row == rowCount - 1);
            const bool alternate = (row % 2 != 0);
            const GuiColor cellBg = alternate ? rowAltColor : rowColor;
            const ImDrawFlags rowFlags = lastRow
                ? (ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight)
                : ImDrawFlags_None;

            drawList->AddRectFilled(
                ImVec2(cursor.x, y),
                ImVec2(cursor.x + tableWidth, y + rowHeight),
                cellBg.ToImU32(),
                radius,
                rowFlags);

            for (int column = 0; column < headerCount; ++column)
            {
                std::string cellText;
                if (static_cast<std::size_t>(column) < rows[row].size())
                {
                    cellText = rows[row][column];
                }
                const ImVec2 textSize = ImGui::CalcTextSize(cellText.c_str());
                const ImVec2 textPos(
                    cursor.x + static_cast<float>(column) * columnWidth + textInset,
                    y + (rowHeight - textSize.y) * 0.5f);
                drawList->AddText(textPos, tokens.text.ToImU32(), cellText.c_str());
            }

            // 行间分隔线
            if (!lastRow)
            {
                drawList->AddLine(
                    ImVec2(cursor.x, y + rowHeight - 1.0f),
                    ImVec2(cursor.x + tableWidth, y + rowHeight - 1.0f),
                    dividerColor.ToImU32());
            }
            y += rowHeight;
        }

        // 外框描边
        drawList->AddRect(
            cursor,
            ImVec2(cursor.x + tableWidth, y),
            borderColor.ToImU32(),
            radius,
            0,
            1.0f);

        // 占位推进布局，避免后续控件与表格重叠
        const float totalHeight = headerHeight + static_cast<float>(rowCount) * rowHeight;
        ImGui::Dummy(ImVec2(tableWidth, totalHeight));

        ImGui::PopID();
    }
}
