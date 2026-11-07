// GuiInput.cpp
// 输入与选择控件的实现（声明见 BaseUI/GUIBase.h）
// 移植自 EUI-NEO components/input.h / dropdown.h / colorpicker.h /
//           datepicker.h / timepicker.h
// 采用 ImGui 立即模式：ImDrawList 自绘视觉（GetWindowDrawList / GetCursorScreenPos）
// 搭配 ImGui 原生交互（InvisibleButton / BeginPopup 等），配色遵循 EUI 暗色主题

#include "GUIBase.h"

#include <cstdio>

namespace GuiBase
{
    namespace
    {
        // 判断是否为闰年
        bool IsLeapYear(int year)
        {
            return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        }

        // 获取某年某月的天数（自动处理闰年二月）
        int DaysInMonth(int year, int month)
        {
            static const int daysPerMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
            month = std::clamp(month, 1, 12);
            if (month == 2 && IsLeapYear(year))
            {
                return 29;
            }
            return daysPerMonth[month - 1];
        }

        // 渲染一行"左右箭头 + 居中文本"的步进控件
        // 左箭头点击返回 -1，右箭头点击返回 1，无操作返回 0
        int ArrowStepRow(const char* rowLabel, const char* text, float columnWidth,
                         const GuiThemeTokens& tokens)
        {
            int direction = 0;
            ImGui::PushID(rowLabel);

            const float height = ImGui::GetFrameHeight();
            const float buttonWidth = 28.0f;
            const float textWidth = std::max(1.0f, columnWidth - buttonWidth * 2.0f);
            const ImVec2 origin = ImGui::GetCursorScreenPos();

            // 左箭头按钮
            ImGui::InvisibleButton("##left", ImVec2(buttonWidth, height));
            const bool leftHovered = ImGui::IsItemHovered();
            const bool leftClicked = ImGui::IsItemClicked();

            // 中间文本区域（不响应点击，仅占位）
            ImGui::SetCursorScreenPos(ImVec2(origin.x + buttonWidth, origin.y));
            ImGui::InvisibleButton("##text", ImVec2(textWidth, height));

            // 右箭头按钮
            ImGui::SetCursorScreenPos(ImVec2(origin.x + columnWidth - buttonWidth, origin.y));
            ImGui::InvisibleButton("##right", ImVec2(buttonWidth, height));
            const bool rightHovered = ImGui::IsItemHovered();
            const bool rightClicked = ImGui::IsItemClicked();

            // 绘制左按钮背景（左侧圆角）
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + buttonWidth, origin.y + height),
                (leftHovered ? tokens.surfaceHover : tokens.surface).ToImU32(),
                tokens.radius.control, ImDrawFlags_RoundCornersLeft);

            // 绘制中间文本区域背景（无圆角）
            draw->AddRectFilled(ImVec2(origin.x + buttonWidth, origin.y),
                ImVec2(origin.x + columnWidth - buttonWidth, origin.y + height),
                tokens.surface.ToImU32(), 0.0f);

            // 绘制右按钮背景（右侧圆角）
            draw->AddRectFilled(ImVec2(origin.x + columnWidth - buttonWidth, origin.y),
                ImVec2(origin.x + columnWidth, origin.y + height),
                (rightHovered ? tokens.surfaceHover : tokens.surface).ToImU32(),
                tokens.radius.control, ImDrawFlags_RoundCornersRight);

            // 绘制整体边框
            draw->AddRect(origin, ImVec2(origin.x + columnWidth, origin.y + height),
                WithAlpha(tokens.border, 0.78f).ToImU32(),
                tokens.radius.control, ImDrawFlags_RoundCornersAll, 1.0f);

            // 绘制居中文本
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            draw->AddText(ImVec2(origin.x + buttonWidth + (textWidth - textSize.x) * 0.5f,
                origin.y + (height - textSize.y) * 0.5f),
                tokens.text.ToImU32(), text);

            // 绘制左右箭头图标
            const float fontSize = ImGui::GetFontSize();
            draw->AddText(ImVec2(origin.x + (buttonWidth - fontSize) * 0.5f,
                origin.y + (height - fontSize) * 0.5f),
                tokens.text.ToImU32(), ICON_FA_CHEVRON_LEFT);
            draw->AddText(ImVec2(origin.x + columnWidth - buttonWidth + (buttonWidth - fontSize) * 0.5f,
                origin.y + (height - fontSize) * 0.5f),
                tokens.text.ToImU32(), ICON_FA_CHEVRON_RIGHT);

            ImGui::PopID();

            if (leftClicked)
            {
                direction = -1;
            }
            else if (rightClicked)
            {
                direction = 1;
            }
            return direction;
        }

        // 渲染一行颜色通道滑块：左标签 + 中间轨道 + 右数值
        // 返回 true 表示该通道的值发生了变化
        bool ChannelSliderRow(const char* rowLabel, const char* channelLabel, float* value,
                              const GuiColor& fillColor, const GuiThemeTokens& tokens)
        {
            bool changed = false;
            ImGui::PushID(rowLabel);

            const float rowHeight = 24.0f;
            const float rowWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            const ImVec2 rowPos = ImGui::GetCursorScreenPos();

            // 整行作为交互命中区域
            ImGui::InvisibleButton("##row", ImVec2(rowWidth, rowHeight));
            const bool rowActive = ImGui::IsItemActive();

            ImDrawList* draw = ImGui::GetWindowDrawList();

            // 通道标签（左对齐）
            const float labelWidth = 14.0f;
            const ImVec2 labelSize = ImGui::CalcTextSize(channelLabel);
            draw->AddText(ImVec2(rowPos.x, rowPos.y + (rowHeight - labelSize.y) * 0.5f),
                tokens.text.ToImU32(), channelLabel);

            // 数值文本（右对齐，显示为 0-255 整数）
            char valueText[16];
            snprintf(valueText, sizeof(valueText), "%d",
                (int)std::lround(std::clamp(*value, 0.0f, 1.0f) * 255.0f));
            const ImVec2 valueSize = ImGui::CalcTextSize(valueText);
            const float valueX = rowPos.x + rowWidth - valueSize.x;
            draw->AddText(ImVec2(valueX, rowPos.y + (rowHeight - valueSize.y) * 0.5f),
                WithAlpha(tokens.text, 0.62f).ToImU32(), valueText);

            // 轨道：位于标签与数值之间
            const float trackGap = 8.0f;
            const float trackX = rowPos.x + labelWidth + trackGap;
            const float trackWidth = std::max(1.0f, valueX - trackGap - trackX);
            const float trackHeight = 6.0f;
            const float trackY = rowPos.y + (rowHeight - trackHeight) * 0.5f;

            // 轨道背景
            draw->AddRectFilled(ImVec2(trackX, trackY), ImVec2(trackX + trackWidth, trackY + trackHeight),
                MixColor(tokens.surfaceHover, tokens.surfaceActive, 0.24f).ToImU32(),
                trackHeight * 0.5f);

            // 填充部分（从轨道起点到当前值位置）
            const float clampedValue = std::clamp(*value, 0.0f, 1.0f);
            const float fillWidth = clampedValue * trackWidth;
            if (fillWidth > 0.0f)
            {
                draw->AddRectFilled(ImVec2(trackX, trackY), ImVec2(trackX + fillWidth, trackY + trackHeight),
                    fillColor.ToImU32(), trackHeight * 0.5f);
            }

            // 圆形滑块按钮（位于填充末端，限制在轨道范围内）
            const float knobSize = 14.0f;
            const float knobCenterX = std::clamp(trackX + fillWidth, trackX, trackX + trackWidth);
            const float knobCenterY = trackY + trackHeight * 0.5f;
            draw->AddCircleFilled(ImVec2(knobCenterX, knobCenterY), knobSize * 0.5f,
                tokens.text.ToImU32(), 24);

            // 交互：按住整行时根据鼠标水平位置更新值
            if (rowActive)
            {
                const float nextValue = std::clamp(
                    (ImGui::GetIO().MousePos.x - trackX) / trackWidth, 0.0f, 1.0f);
                if (std::fabs(nextValue - clampedValue) > 0.0001f)
                {
                    *value = nextValue;
                    changed = true;
                }
            }

            ImGui::PopID();
            return changed;
        }
    }

    // 文本输入框：EUI 表面色帧背景 + 白色文字，复用 ImGui::InputText
    // 由于 InputText 需要 char 缓冲区，这里通过 std::string 的临时缓冲区拷贝
    bool GuiInputText(const char* label, std::string* value, const ImVec2& size)
    {
        GuiThemeTokens tokens = Theme::Dark();
        if (value == nullptr)
        {
            return false;
        }

        ImGui::PushID(label);

        // 指定宽度时通过 SetNextItemWidth 生效
        if (size.x > 0.0f)
        {
            ImGui::SetNextItemWidth(size.x);
        }

        // 指定高度时按字体尺寸推算纵向内边距
        float paddingY = -1.0f;
        if (size.y > 0.0f)
        {
            const float fontSize = ImGui::GetFontSize();
            paddingY = std::max(2.0f, (size.y - fontSize) * 0.5f);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens.radius.control);
        if (paddingY > 0.0f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                ImVec2(ImGui::GetStyle().FramePadding.x, paddingY));
        }

        // EUI 输入框配色：表面色背景三态 + 白色文字 + 半透明边框
        ImGui::PushStyleColor(ImGuiCol_FrameBg, tokens.surface.ToImVec4());
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, tokens.surfaceHover.ToImVec4());
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, tokens.surfaceActive.ToImVec4());
        ImGui::PushStyleColor(ImGuiCol_Text, tokens.text.ToImVec4());
        ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(tokens.border, 0.78f).ToImVec4());

        // 拷贝到带余量的临时缓冲区，编辑结果再写回 std::string
        std::string editable = *value;
        editable.resize(std::max<size_t>(editable.size() + 64, 64));
        const bool changed = ImGui::InputText("##input", editable.data(), editable.size());
        if (changed)
        {
            *value = editable.c_str();
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(paddingY > 0.0f ? 2 : 1);
        ImGui::PopID();
        return changed;
    }

    // 下拉选择：surface 背景显示框 + 右侧箭头图标，点击弹出 EUI 风格列表
    // 选中某项后关闭列表，仅当选中的项发生变化时返回 true
    bool GuiDropdown(const char* label, const char* const* items, int itemCount,
                     int* selectedIndex)
    {
        GuiThemeTokens tokens = Theme::Dark();
        bool changed = false;

        ImGui::PushID(label);

        // 当前选中项的显示文本，无选中时显示占位符
        const bool hasSelection = (selectedIndex != nullptr &&
                                   *selectedIndex >= 0 && *selectedIndex < itemCount);
        const char* currentText = hasSelection ? items[*selectedIndex] : "请选择";

        // 显示框尺寸
        const float fieldWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float fieldHeight = ImGui::GetFrameHeight();
        const ImVec2 fieldPos = ImGui::GetCursorScreenPos();
        const ImVec2 fieldSize(fieldWidth, fieldHeight);

        // 显示框命中区域
        ImGui::InvisibleButton("##field", fieldSize);
        const bool fieldHovered = ImGui::IsItemHovered();
        const bool fieldClicked = ImGui::IsItemClicked();

        // 绘制显示框背景（hover 时切换表面色）
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(fieldPos, fieldPos + fieldSize,
            (fieldHovered ? tokens.surfaceHover : tokens.surface).ToImU32(),
            tokens.radius.control);
        draw->AddRect(fieldPos, fieldPos + fieldSize,
            WithAlpha(tokens.border, 0.78f).ToImU32(),
            tokens.radius.control, ImDrawFlags_RoundCornersAll, 1.0f);

        // 绘制当前选中项文本（左对齐，无选中时用弱化文字色）
        const ImVec2 textSize = ImGui::CalcTextSize(currentText);
        draw->AddText(ImVec2(fieldPos.x + 12.0f, fieldPos.y + (fieldHeight - textSize.y) * 0.5f),
            (hasSelection ? tokens.text : WithAlpha(tokens.text, 0.45f)).ToImU32(), currentText);

        // 右侧箭头图标：展开时向上，收起时向下
        const bool popupOpen = ImGui::IsPopupOpen("##dropdown_popup");
        const char* chevron = popupOpen ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN;
        const float fontSize = ImGui::GetFontSize();
        draw->AddText(ImVec2(fieldPos.x + fieldWidth - 26.0f,
            fieldPos.y + (fieldHeight - fontSize) * 0.5f),
            tokens.primary.ToImU32(), chevron);

        // 点击显示框打开下拉列表
        if (fieldClicked)
        {
            ImGui::OpenPopup("##dropdown_popup");
        }

        // 下拉列表：EUI popup 风格（深一档的表面色背景 + 半透明边框）
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, tokens.radius.popup);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg,
            MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.12f).ToImU32());
        ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(tokens.border, 0.78f).ToImU32());

        // 设定弹出窗口宽度与显示框一致，高度自动
        ImGui::SetNextWindowSize(ImVec2(fieldWidth, 0.0f));

        if (ImGui::BeginPopup("##dropdown_popup"))
        {
            const float itemHeight = 30.0f;
            const float itemWidth = std::max(1.0f, fieldWidth - 12.0f);

            for (int index = 0; index < itemCount; ++index)
            {
                ImGui::PushID(index);
                const ImVec2 itemPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##item", ImVec2(itemWidth, itemHeight));
                const bool itemHovered = ImGui::IsItemHovered();
                const bool itemClicked = ImGui::IsItemClicked();
                const bool isSelected = (selectedIndex != nullptr && *selectedIndex == index);

                if (itemClicked)
                {
                    // 选中项发生变化时写回并标记返回值
                    if (selectedIndex != nullptr && *selectedIndex != index)
                    {
                        *selectedIndex = index;
                        changed = true;
                    }
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    // 绘制条目背景：选中项使用主色半透明高亮，hover 用表面色
                    ImDrawList* popupDraw = ImGui::GetWindowDrawList();
                    GuiColor itemBg = GuiColor(0.0f, 0.0f, 0.0f, 0.0f);
                    if (itemHovered)
                    {
                        itemBg = tokens.surfaceHover;
                    }
                    else if (isSelected)
                    {
                        itemBg = WithAlpha(tokens.primary, 0.24f);
                    }
                    popupDraw->AddRectFilled(itemPos, ImVec2(itemPos.x + itemWidth, itemPos.y + itemHeight),
                        itemBg.ToImU32(), std::max(tokens.radius.tiny, tokens.radius.control - 2.0f));

                    // 绘制条目文本：选中项用主色，其余用文本色
                    const ImVec2 itemTextSize = ImGui::CalcTextSize(items[index]);
                    popupDraw->AddText(ImVec2(itemPos.x + 10.0f,
                        itemPos.y + (itemHeight - itemTextSize.y) * 0.5f),
                        (isSelected ? tokens.primary : tokens.text).ToImU32(), items[index]);
                }

                ImGui::PopID();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
        ImGui::PopID();
        return changed;
    }

    // 颜色选择器：当前颜色预览色块 + R/G/B/A 四行自绘滑块
    // 拖动任意通道滑块都会即时更新颜色，颜色发生变化时返回 true
    bool GuiColorPicker(const char* label, float color[4])
    {
        GuiThemeTokens tokens = Theme::Dark();
        bool changed = false;

        ImGui::PushID(label);

        const float previewWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float previewHeight = 44.0f;
        const float previewGap = 8.0f;
        const ImVec2 previewPos = ImGui::GetCursorScreenPos();

        // 预览色块：以当前颜色填充圆角矩形，并绘制半透明边框
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const GuiColor current(color[0], color[1], color[2], color[3]);
        draw->AddRectFilled(previewPos, ImVec2(previewPos.x + previewWidth, previewPos.y + previewHeight),
            current.ToImU32(), tokens.radius.small);
        draw->AddRect(previewPos, ImVec2(previewPos.x + previewWidth, previewPos.y + previewHeight),
            WithAlpha(tokens.border, 0.78f).ToImU32(),
            tokens.radius.small, ImDrawFlags_RoundCornersAll, 1.0f);

        // 色块右侧显示 HEX 文本（忽略 alpha）
        char hexText[16];
        snprintf(hexText, sizeof(hexText), "#%02X%02X%02X",
            (int)std::lround(std::clamp(color[0], 0.0f, 1.0f) * 255.0f),
            (int)std::lround(std::clamp(color[1], 0.0f, 1.0f) * 255.0f),
            (int)std::lround(std::clamp(color[2], 0.0f, 1.0f) * 255.0f));
        const ImVec2 hexSize = ImGui::CalcTextSize(hexText);
        draw->AddText(ImVec2(previewPos.x + previewWidth - hexSize.x - 12.0f,
            previewPos.y + (previewHeight - hexSize.y) * 0.5f),
            WithAlpha(GuiColor(1.0f, 1.0f, 1.0f, 1.0f), 0.94f).ToImU32(), hexText);

        // 推进光标到色块下方，再依次排布四行通道滑块
        ImGui::Dummy(ImVec2(previewWidth, previewHeight + previewGap));

        // R/G/B 使用 EUI 三通道主题色，A 使用中性灰
        changed |= ChannelSliderRow("##r", "R", &color[0],
            GuiColor(0.92f, 0.20f, 0.22f), tokens);
        changed |= ChannelSliderRow("##g", "G", &color[1],
            GuiColor(0.15f, 0.74f, 0.40f), tokens);
        changed |= ChannelSliderRow("##b", "B", &color[2],
            GuiColor(0.20f, 0.46f, 0.92f), tokens);
        changed |= ChannelSliderRow("##a", "A", &color[3],
            GuiColor(0.55f, 0.55f, 0.60f), tokens);

        ImGui::PopID();
        return changed;
    }

    // 日期选择器：年、月、日三个相邻小框，各自带左右箭头独立调整
    // 月、日步进自动进位或借位，值发生变化时返回 true
    bool GuiDatePicker(const char* label, int* year, int* month, int* day)
    {
        GuiThemeTokens tokens = Theme::Dark();
        bool changed = false;

        ImGui::PushID(label);

        // 将输入值合法化（年 1900-2200，月 1-12，日按当月天数）
        int currentYear = std::clamp(*year, 1900, 2200);
        int currentMonth = std::clamp(*month, 1, 12);
        int currentDay = std::clamp(*day, 1, DaysInMonth(currentYear, currentMonth));
        if (currentYear != *year || currentMonth != *month || currentDay != *day)
        {
            *year = currentYear;
            *month = currentMonth;
            *day = currentDay;
            changed = true;
        }

        // 三列宽度分配：年列最宽，月、日列等宽
        const ImVec2 rowOrigin = ImGui::GetCursorScreenPos();
        const float rowHeight = ImGui::GetFrameHeight();
        const float availWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float columnGap = 6.0f;
        const float monthWidth = (availWidth - columnGap * 2.0f) * 0.28f;
        const float dayWidth = monthWidth;
        const float yearWidth = availWidth - columnGap * 2.0f - monthWidth - dayWidth;

        // 三段显示文本
        char yearText[16];
        char monthText[16];
        char dayText[16];
        snprintf(yearText, sizeof(yearText), "%04d", currentYear);
        snprintf(monthText, sizeof(monthText), "%02d", currentMonth);
        snprintf(dayText, sizeof(dayText), "%02d", currentDay);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 separatorSize = ImGui::CalcTextSize("-");
        const GuiColor mutedText = WithAlpha(tokens.text, 0.45f);

        // 年列步进：逐年调整并夹取范围
        const int yearStep = ArrowStepRow("##col_year", yearText, yearWidth, tokens);
        if (yearStep != 0)
        {
            currentYear = std::clamp(currentYear + yearStep, 1900, 2200);
            currentDay = std::min(currentDay, DaysInMonth(currentYear, currentMonth));
            *year = currentYear;
            *month = currentMonth;
            *day = currentDay;
            changed = true;
        }

        // 列间分隔符，并移动光标到月列起点
        draw->AddText(ImVec2(rowOrigin.x + yearWidth + columnGap * 0.5f - separatorSize.x * 0.5f,
            rowOrigin.y + (rowHeight - separatorSize.y) * 0.5f),
            mutedText.ToImU32(), "-");
        ImGui::SetCursorScreenPos(ImVec2(rowOrigin.x + yearWidth + columnGap, rowOrigin.y));

        // 月列步进：循环进位，日按新月份天数收敛
        const int monthStep = ArrowStepRow("##col_month", monthText, monthWidth, tokens);
        if (monthStep != 0)
        {
            currentMonth += monthStep;
            if (currentMonth < 1)
            {
                currentMonth = 12;
            }
            else if (currentMonth > 12)
            {
                currentMonth = 1;
            }
            currentDay = std::min(currentDay, DaysInMonth(currentYear, currentMonth));
            *year = currentYear;
            *month = currentMonth;
            *day = currentDay;
            changed = true;
        }

        // 列间分隔符，并移动光标到日列起点
        draw->AddText(ImVec2(rowOrigin.x + yearWidth + columnGap + monthWidth + columnGap * 0.5f - separatorSize.x * 0.5f,
            rowOrigin.y + (rowHeight - separatorSize.y) * 0.5f),
            mutedText.ToImU32(), "-");
        ImGui::SetCursorScreenPos(ImVec2(rowOrigin.x + yearWidth + columnGap + monthWidth + columnGap,
            rowOrigin.y));

        // 日列步进：按当月天数循环
        const int dayStep = ArrowStepRow("##col_day", dayText, dayWidth, tokens);
        if (dayStep != 0)
        {
            currentDay += dayStep;
            const int maxDay = DaysInMonth(currentYear, currentMonth);
            if (currentDay < 1)
            {
                currentDay = maxDay;
            }
            else if (currentDay > maxDay)
            {
                currentDay = 1;
            }
            *year = currentYear;
            *month = currentMonth;
            *day = currentDay;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    // 时间选择器：时、分两段合并为一行显示，左右箭头以分钟为步进调整
    // 跨 60 分钟自动进位或借位到小时，值发生变化时返回 true
    bool GuiTimePicker(const char* label, int* hour, int* minute)
    {
        GuiThemeTokens tokens = Theme::Dark();
        bool changed = false;

        ImGui::PushID(label);

        // 将输入值合法化（时 0-23，分 0-59）
        int currentHour = std::clamp(*hour, 0, 23);
        int currentMinute = std::clamp(*minute, 0, 59);
        if (currentHour != *hour || currentMinute != *minute)
        {
            *hour = currentHour;
            *minute = currentMinute;
            changed = true;
        }

        char timeText[16];
        snprintf(timeText, sizeof(timeText), "%02d:%02d", currentHour, currentMinute);

        const float columnWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const int step = ArrowStepRow("##row", timeText, columnWidth, tokens);
        if (step != 0)
        {
            // 以分钟为步进：超过范围时向小时进位或借位
            currentMinute += step;
            if (currentMinute > 59)
            {
                currentMinute = 0;
                ++currentHour;
                if (currentHour > 23)
                {
                    currentHour = 0;
                }
            }
            else if (currentMinute < 0)
            {
                currentMinute = 59;
                --currentHour;
                if (currentHour < 0)
                {
                    currentHour = 23;
                }
            }
            *hour = currentHour;
            *minute = currentMinute;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }
}
