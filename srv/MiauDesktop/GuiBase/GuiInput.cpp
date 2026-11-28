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
                tokens.radius.control, 1.0f, ImDrawFlags_RoundCornersAll);

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
        return GuiDropdownEx(label, items, itemCount, selectedIndex, GuiDropdownOptions());
    }

    bool GuiDropdownEx(const char* label, const char* const* items, int itemCount,
                       int* selectedIndex, const GuiDropdownOptions& options)
    {
        GuiThemeTokens tokens = Theme::Dark();
        bool changed = false;

        ImGui::PushID(label);

        const ImGuiID stateId = ImGui::GetID("##dropdown_state");
        ImGuiStorage* storage = ImGui::GetStateStorage();
        int* openState = storage->GetIntRef(stateId ^ 0x51ED2707u, options.open ? 1 : 0);
        const bool controlled = static_cast<bool>(options.onOpenChange);
        const bool open = controlled ? options.open : (*openState != 0);

        const bool hasSelection = (selectedIndex != nullptr &&
                                   *selectedIndex >= 0 && *selectedIndex < itemCount);
        const char* currentText = hasSelection
                                      ? items[*selectedIndex]
                                      : (options.placeholder != nullptr ? options.placeholder : "请选择");

        const float fieldWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float fieldHeight = ImGui::GetFrameHeight();
        const ImVec2 fieldPos = ImGui::GetCursorScreenPos();
        const ImVec2 fieldSize(fieldWidth, fieldHeight);

        ImGui::InvisibleButton("##field", fieldSize);
        const bool fieldHovered = ImGui::IsItemHovered();
        const bool fieldClicked = ImGui::IsItemClicked() && !options.disabled;

        bool desiredOpen = open;
        bool requestOpen = false;
        if (fieldClicked)
        {
            const bool next = !open;
            if (controlled)
            {
                options.onOpenChange(next);
            }
            else
            {
                *openState = next ? 1 : 0;
                desiredOpen = next;
                requestOpen = next;
            }
        }
        // 受控模式：options.open 由 false → true 的边沿触发一次打开
        if (controlled)
        {
            int* lastOpen = storage->GetIntRef(stateId ^ 0x7F4A7C15u, 0);
            if (options.open && *lastOpen == 0)
            {
                requestOpen = true;
            }
            *lastOpen = options.open ? 1 : 0;
        }

        const float alpha = options.disabled ? 0.45f : 1.0f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const GuiColor fieldColor = (fieldHovered && !options.disabled) ? tokens.surfaceHover
                                                                       : tokens.surface;
        draw->AddRectFilled(fieldPos, fieldPos + fieldSize,
                            WithAlpha(fieldColor, alpha).ToImU32(), tokens.radius.control);
        draw->AddRect(fieldPos, fieldPos + fieldSize,
                      WithAlpha(tokens.border, 0.78f * alpha).ToImU32(),
                      tokens.radius.control, 1.0f, ImDrawFlags_RoundCornersAll);

        const ImVec2 textSize = ImGui::CalcTextSize(currentText);
        const GuiColor textColor = hasSelection ? WithAlpha(tokens.text, alpha)
                                                : WithAlpha(tokens.text, 0.45f * alpha);
        draw->AddText(ImVec2(fieldPos.x + 12.0f, fieldPos.y + (fieldHeight - textSize.y) * 0.5f),
                      textColor.ToImU32(), currentText);

        const char* chevron = desiredOpen ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN;
        const float fontSize = ImGui::GetFontSize();
        draw->AddText(ImVec2(fieldPos.x + fieldWidth - 26.0f,
                             fieldPos.y + (fieldHeight - fontSize) * 0.5f),
                      WithAlpha(tokens.primary, alpha).ToImU32(), chevron);

        if (desiredOpen && !options.disabled)
        {
            const float itemHeight = std::max(12.0f, options.itemHeight);
            const float popupPadding = 6.0f;
            const float popupHeight = itemHeight * static_cast<float>(std::max(1, itemCount)) +
                                      popupPadding * 2.0f;
            const ImVec2 popupPos(fieldPos.x,
                                  options.openUp ? (fieldPos.y - popupHeight - 4.0f)
                                                 : (fieldPos.y + fieldHeight + 4.0f));

            if (requestOpen)
            {
                ImGui::OpenPopup("##dropdown_popup");
            }
            ImGui::SetNextWindowPos(popupPos);
            ImGui::SetNextWindowSize(ImVec2(fieldWidth, 0.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(popupPadding, popupPadding));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, tokens.radius.popup);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg,
                                  MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.12f).ToImU32());
            ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(tokens.border, 0.78f).ToImU32());

            if (ImGui::BeginPopup("##dropdown_popup"))
            {
                const float itemWidth = std::max(1.0f, fieldWidth - popupPadding * 2.0f);
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
                        if (selectedIndex != nullptr && *selectedIndex != index)
                        {
                            *selectedIndex = index;
                            changed = true;
                        }
                        if (!controlled)
                        {
                            *openState = 0;
                        }
                        else if (options.onOpenChange)
                        {
                            options.onOpenChange(false);
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        ImDrawList* popupDraw = ImGui::GetWindowDrawList();
                        GuiColor itemBg(0.0f, 0.0f, 0.0f, 0.0f);
                        if (itemHovered)
                        {
                            itemBg = tokens.surfaceHover;
                        }
                        else if (isSelected)
                        {
                            itemBg = WithAlpha(tokens.primary, 0.24f);
                        }
                        popupDraw->AddRectFilled(
                            itemPos, ImVec2(itemPos.x + itemWidth, itemPos.y + itemHeight),
                            itemBg.ToImU32(),
                            std::max(tokens.radius.tiny, tokens.radius.control - 2.0f));

                        const ImVec2 itemTextSize = ImGui::CalcTextSize(items[index]);
                        popupDraw->AddText(
                            ImVec2(itemPos.x + 10.0f, itemPos.y + (itemHeight - itemTextSize.y) * 0.5f),
                            (isSelected ? tokens.primary : tokens.text).ToImU32(), items[index]);
                    }
                    ImGui::PopID();
                }
                ImGui::EndPopup();
            }
            else
            {
                if (!controlled)
                {
                    *openState = 0;
                }
                else if (options.onOpenChange)
                {
                    options.onOpenChange(false);
                }
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }

        ImGui::PopID();
        return changed;
    }


    // 颜色选择器：当前颜色预览色块 + R/G/B/A 四行自绘滑块
    // 拖动任意通道滑块都会即时更新颜色，颜色发生变化时返回 true
        // RGB ↔ HSV 转换（颜色选择器弹层的 SV 方块 / 色相条）
        void RgbToHsv(float r, float g, float b, float& h, float& s, float& v)
        {
            const float maxValue = std::max(r, std::max(g, b));
            const float minValue = std::min(r, std::min(g, b));
            const float delta = maxValue - minValue;
            v = maxValue;
            s = (maxValue > 0.0f) ? (delta / maxValue) : 0.0f;
            if (delta <= 1e-6f)
            {
                h = 0.0f;
                return;
            }
            if (maxValue == r)
            {
                h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
            }
            else if (maxValue == g)
            {
                h = (b - r) / delta + 2.0f;
            }
            else
            {
                h = (r - g) / delta + 4.0f;
            }
            h /= 6.0f;
        }

        void HsvToRgb(float h, float s, float v, float& r, float& g, float& b)
        {
            h = h - std::floor(h);
            const float hue = h * 6.0f;
            const int sector = static_cast<int>(std::floor(hue)) % 6;
            const float f = hue - std::floor(hue);
            const float p = v * (1.0f - s);
            const float q = v * (1.0f - f * s);
            const float t = v * (1.0f - (1.0f - f) * s);
            switch (sector)
            {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
            }
        }

    bool GuiColorPicker(const char* label, float color[4])
    {
        return GuiColorPickerEx(label, color, GuiColorPickerOptions());
    }

    bool GuiColorPickerEx(const char* label, float color[4], const GuiColorPickerOptions& options)
    {
        if (color == nullptr)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark();
        bool changed = false;
        ImGui::PushID(label);

        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float previewHeight = 44.0f;
        const ImVec2 previewPos = ImGui::GetCursorScreenPos();
        const ImVec2 previewSize(width, previewHeight);

        ImGui::InvisibleButton("##preview", previewSize);
        const bool previewClicked = ImGui::IsItemClicked();

        const ImGuiID stateId = ImGui::GetID("##colorpicker_state");
        ImGuiStorage* storage = ImGui::GetStateStorage();
        int* openState = storage->GetIntRef(stateId ^ 0x27D4EB2Fu, options.open ? 1 : 0);
        const bool controlled = static_cast<bool>(options.onOpenChange);
        const bool open = controlled ? options.open : (*openState != 0);
        bool desiredOpen = open;
        bool requestOpen = false;
        if (previewClicked)
        {
            const bool next = !open;
            if (controlled)
            {
                options.onOpenChange(next);
            }
            else
            {
                *openState = next ? 1 : 0;
                desiredOpen = next;
                requestOpen = next;
            }
        }
        if (controlled)
        {
            int* lastOpen = storage->GetIntRef(stateId ^ 0x7F4A7C15u, 0);
            if (options.open && *lastOpen == 0)
            {
                requestOpen = true;
            }
            *lastOpen = options.open ? 1 : 0;
        }

        // 预览色块 + HEX
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const GuiColor current(color[0], color[1], color[2], color[3]);
        draw->AddRectFilled(previewPos, previewPos + previewSize, current.ToImU32(), tokens.radius.small);
        draw->AddRect(previewPos, previewPos + previewSize,
                      WithAlpha(tokens.border, 0.78f).ToImU32(),
                      tokens.radius.small, 1.0f, ImDrawFlags_RoundCornersAll);

        char hexText[16];
        snprintf(hexText, sizeof(hexText), "#%02X%02X%02X",
                 (int)std::lround(std::clamp(color[0], 0.0f, 1.0f) * 255.0f),
                 (int)std::lround(std::clamp(color[1], 0.0f, 1.0f) * 255.0f),
                 (int)std::lround(std::clamp(color[2], 0.0f, 1.0f) * 255.0f));
        const ImVec2 hexSize = ImGui::CalcTextSize(hexText);
        draw->AddText(ImVec2(previewPos.x + width - hexSize.x - 12.0f,
                             previewPos.y + (previewHeight - hexSize.y) * 0.5f),
                      WithAlpha(GuiColor(1.0f, 1.0f, 1.0f, 1.0f), 0.94f).ToImU32(), hexText);

        ImGui::Dummy(ImVec2(width, 8.0f));

        if (desiredOpen)
        {
            const float popupWidth = 260.0f;
            const ImVec2 popupPos(previewPos.x + width - popupWidth,
                                  previewPos.y + previewHeight + 6.0f);
            if (requestOpen)
            {
                ImGui::OpenPopup("##color_popup");
            }
            ImGui::SetNextWindowPos(popupPos);
            ImGui::SetNextWindowSize(ImVec2(popupWidth, 0.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, tokens.radius.popup);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg,
                                  MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.12f).ToImU32());
            ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(tokens.border, 0.78f).ToImU32());

            if (ImGui::BeginPopup("##color_popup"))
            {
                ImDrawList* popupDraw = ImGui::GetWindowDrawList();
                float r = std::clamp(color[0], 0.0f, 1.0f);
                float g = std::clamp(color[1], 0.0f, 1.0f);
                float b = std::clamp(color[2], 0.0f, 1.0f);
                float a = std::clamp(color[3], 0.0f, 1.0f);
                float h = 0.0f, s = 0.0f, v = 0.0f;
                RgbToHsv(r, g, b, h, s, v);
                float* hueStore = storage->GetFloatRef(stateId ^ 0x3C6EF372u, h);
                if (s > 0.001f && v > 0.001f)
                {
                    *hueStore = h;
                }
                else
                {
                    h = *hueStore;
                }

                // SV 方块：底色纯色相 + 顶部白渐变 + 底部黑渐变
                const ImVec2 svPos = ImGui::GetCursorScreenPos();
                const ImVec2 svSize(240.0f, 140.0f);
                ImGui::InvisibleButton("##sv", svSize);
                const bool svActive = ImGui::IsItemActive();
                if (svActive || ImGui::IsItemClicked())
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    s = std::clamp((mouse.x - svPos.x) / svSize.x, 0.0f, 1.0f);
                    v = std::clamp(1.0f - (mouse.y - svPos.y) / svSize.y, 0.0f, 1.0f);
                    HsvToRgb(h, s, v, r, g, b);
                    color[0] = r;
                    color[1] = g;
                    color[2] = b;
                    changed = true;
                }

                float hueR = 1.0f, hueG = 1.0f, hueB = 1.0f;
                HsvToRgb(h, 1.0f, 1.0f, hueR, hueG, hueB);
                const ImU32 huePure = IM_COL32(static_cast<int>(hueR * 255.0f),
                                               static_cast<int>(hueG * 255.0f),
                                               static_cast<int>(hueB * 255.0f), 255);
                popupDraw->AddRectFilled(svPos, svPos + svSize, huePure, 4.0f);
                popupDraw->AddRectFilledMultiColor(svPos, svPos + svSize,
                                              IM_COL32_WHITE, IM_COL32_WHITE,
                                              IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
                popupDraw->AddRectFilledMultiColor(svPos, svPos + svSize,
                                              IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                              IM_COL32_BLACK, IM_COL32_BLACK);
                const ImVec2 svCursor(svPos.x + s * svSize.x, svPos.y + (1.0f - v) * svSize.y);
                popupDraw->AddCircle(svCursor, 6.0f, IM_COL32_WHITE, 20, 2.0f);
                popupDraw->AddCircle(svCursor, 7.5f, IM_COL32(0, 0, 0, 120), 20, 1.0f);
                popupDraw->AddRect(svPos, svPos + svSize, WithAlpha(tokens.border, 0.60f).ToImU32(), 4.0f);
                ImGui::Dummy(ImVec2(240.0f, 6.0f));

                // 色相条
                const ImVec2 huePos = ImGui::GetCursorScreenPos();
                const ImVec2 hueSize(240.0f, 14.0f);
                ImGui::InvisibleButton("##hue", hueSize);
                if (ImGui::IsItemActive() || ImGui::IsItemClicked())
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    h = std::clamp((mouse.x - huePos.x) / hueSize.x, 0.0f, 1.0f);
                    *hueStore = h;
                    HsvToRgb(h, s, v, r, g, b);
                    color[0] = r;
                    color[1] = g;
                    color[2] = b;
                    changed = true;
                }
                const ImU32 hueStops[7] = {
                    IM_COL32(255, 0, 0, 255), IM_COL32(255, 255, 0, 255),
                    IM_COL32(0, 255, 0, 255), IM_COL32(0, 255, 255, 255),
                    IM_COL32(0, 0, 255, 255), IM_COL32(255, 0, 255, 255),
                    IM_COL32(255, 0, 0, 255)
                };
                for (int index = 0; index < 6; ++index)
                {
                    const float x0 = huePos.x + hueSize.x * static_cast<float>(index) / 6.0f;
                    const float x1 = huePos.x + hueSize.x * static_cast<float>(index + 1) / 6.0f;
                    popupDraw->AddRectFilledMultiColor(ImVec2(x0, huePos.y),
                                                  ImVec2(x1, huePos.y + hueSize.y),
                                                  hueStops[index], hueStops[index + 1],
                                                  hueStops[index + 1], hueStops[index]);
                }
                const float hueX = huePos.x + h * hueSize.x;
                popupDraw->AddRectFilled(ImVec2(hueX - 2.0f, huePos.y - 2.0f),
                                    ImVec2(hueX + 2.0f, huePos.y + hueSize.y + 2.0f),
                                    IM_COL32_WHITE, 1.0f);
                popupDraw->AddRect(huePos, huePos + hueSize,
                              WithAlpha(tokens.border, 0.60f).ToImU32(), 3.0f);
                ImGui::Dummy(ImVec2(240.0f, 6.0f));

                // Alpha 条：棋盘格 + 透明→不透明渐变
                if (options.showAlpha)
                {
                    const ImVec2 alphaPos = ImGui::GetCursorScreenPos();
                    const ImVec2 alphaSize(240.0f, 14.0f);
                    ImGui::InvisibleButton("##alpha", alphaSize);
                    if (ImGui::IsItemActive() || ImGui::IsItemClicked())
                    {
                        const ImVec2 mouse = ImGui::GetIO().MousePos;
                        a = std::clamp((mouse.x - alphaPos.x) / alphaSize.x, 0.0f, 1.0f);
                        color[3] = a;
                        changed = true;
                    }
                    const float cell = 7.0f;
                    for (float y = 0.0f; y < alphaSize.y; y += cell)
                    {
                        for (float x = 0.0f; x < alphaSize.x; x += cell)
                        {
                            const bool light = (static_cast<int>(x / cell) + static_cast<int>(y / cell)) % 2 == 0;
                            const ImU32 checker = light ? IM_COL32(210, 210, 210, 255)
                                                        : IM_COL32(160, 160, 160, 255);
                            popupDraw->AddRectFilled(
                                ImVec2(alphaPos.x + x, alphaPos.y + y),
                                ImVec2(std::min(alphaPos.x + x + cell, alphaPos.x + alphaSize.x),
                                       std::min(alphaPos.y + y + cell, alphaPos.y + alphaSize.y)),
                                checker);
                        }
                    }
                    const int cr = static_cast<int>(r * 255.0f);
                    const int cg = static_cast<int>(g * 255.0f);
                    const int cb = static_cast<int>(b * 255.0f);
                    popupDraw->AddRectFilledMultiColor(alphaPos, alphaPos + alphaSize,
                                                  IM_COL32(cr, cg, cb, 0), IM_COL32(cr, cg, cb, 255),
                                                  IM_COL32(cr, cg, cb, 255), IM_COL32(cr, cg, cb, 0));
                    const float alphaX = alphaPos.x + a * alphaSize.x;
                    popupDraw->AddRectFilled(ImVec2(alphaX - 2.0f, alphaPos.y - 2.0f),
                                        ImVec2(alphaX + 2.0f, alphaPos.y + alphaSize.y + 2.0f),
                                        IM_COL32_WHITE, 1.0f);
                    popupDraw->AddRect(alphaPos, alphaPos + alphaSize,
                                  WithAlpha(tokens.border, 0.60f).ToImU32(), 3.0f);
                    ImGui::Dummy(ImVec2(240.0f, 6.0f));
                }

                // 预设色板
                static const GuiColor kDefaultSwatches[8] = {
                    GuiColor(0.22f, 0.44f, 0.88f, 1.0f), GuiColor(0.20f, 0.76f, 0.58f, 1.0f),
                    GuiColor(0.98f, 0.62f, 0.15f, 1.0f), GuiColor(0.86f, 0.28f, 0.44f, 1.0f),
                    GuiColor(0.92f, 0.20f, 0.22f, 1.0f), GuiColor(0.96f, 0.80f, 0.22f, 1.0f),
                    GuiColor(0.20f, 0.72f, 0.92f, 1.0f), GuiColor(0.62f, 0.36f, 0.92f, 1.0f)
                };
                const GuiColor* swatches = options.swatches.empty()
                                               ? kDefaultSwatches : options.swatches.data();
                const int swatchCount = options.swatches.empty()
                                            ? 8 : static_cast<int>(options.swatches.size());
                const float swatchSize = 20.0f;
                const float swatchGap = 6.0f;
                for (int index = 0; index < swatchCount; ++index)
                {
                    if (index > 0)
                    {
                        ImGui::SameLine(0.0f, swatchGap);
                    }
                    ImGui::PushID(index);
                    const ImVec2 swatchPos = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton("##swatch", ImVec2(swatchSize, swatchSize));
                    if (ImGui::IsItemClicked())
                    {
                        color[0] = swatches[index].r;
                        color[1] = swatches[index].g;
                        color[2] = swatches[index].b;
                        changed = true;
                    }
                    popupDraw->AddRectFilled(swatchPos, swatchPos + ImVec2(swatchSize, swatchSize),
                                        swatches[index].ToImU32(), 4.0f);
                    popupDraw->AddRect(swatchPos, swatchPos + ImVec2(swatchSize, swatchSize),
                                  WithAlpha(tokens.border, 0.70f).ToImU32(), 4.0f);
                    ImGui::PopID();
                }
                ImGui::Dummy(ImVec2(240.0f, 4.0f));

                // RGBA 滑块（复用现有通道滑块视觉）
                changed |= ChannelSliderRow("##cp_r", "R", &color[0],
                                            GuiColor(0.92f, 0.20f, 0.22f), tokens);
                changed |= ChannelSliderRow("##cp_g", "G", &color[1],
                                            GuiColor(0.15f, 0.74f, 0.40f), tokens);
                changed |= ChannelSliderRow("##cp_b", "B", &color[2],
                                            GuiColor(0.20f, 0.46f, 0.92f), tokens);
                if (options.showAlpha)
                {
                    changed |= ChannelSliderRow("##cp_a", "A", &color[3],
                                                GuiColor(0.55f, 0.55f, 0.60f), tokens);
                }
                ImGui::EndPopup();
            }
            else
            {
                if (!controlled)
                {
                    *openState = 0;
                }
                else if (options.onOpenChange)
                {
                    options.onOpenChange(false);
                }
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }

        if (changed && options.onChange)
        {
            options.onChange(color);
        }
        ImGui::PopID();
        return changed;
    }


    // 日期选择器：年、月、日三个相邻小框，各自带左右箭头独立调整
    // 月、日步进自动进位或借位，值发生变化时返回 true
    bool GuiDatePicker(const char* label, int* year, int* month, int* day)
    {
        return GuiDatePickerEx(label, year, month, day, GuiDatePickerOptions());
    }

    bool GuiDatePickerEx(const char* label, int* year, int* month, int* day,
                         const GuiDatePickerOptions& options)
    {
        if (year == nullptr || month == nullptr || day == nullptr)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark();
        bool changed = false;
        ImGui::PushID(label);

        int y = std::clamp(*year, 1900, 2200);
        int m = std::clamp(*month, 1, 12);
        int d = std::clamp(*day, 1, DaysInMonth(y, m));
        if (y != *year || m != *month || d != *day)
        {
            *year = y;
            *month = m;
            *day = d;
            changed = true;
        }

        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float fieldHeight = ImGui::GetFrameHeight();
        const ImVec2 fieldPos = ImGui::GetCursorScreenPos();
        const ImVec2 fieldSize(width, fieldHeight);

        ImGui::InvisibleButton("##date_field", fieldSize);
        const bool fieldClicked = ImGui::IsItemClicked();
        const bool fieldHovered = ImGui::IsItemHovered();

        const ImGuiID stateId = ImGui::GetID("##datepicker_state");
        ImGuiStorage* storage = ImGui::GetStateStorage();
        int* openState = storage->GetIntRef(stateId ^ 0x6A09E667u, options.open ? 1 : 0);
        const bool controlled = static_cast<bool>(options.onOpenChange);
        const bool open = controlled ? options.open : (*openState != 0);
        bool desiredOpen = open;
        bool requestOpen = false;
        if (fieldClicked)
        {
            const bool next = !open;
            if (controlled)
            {
                options.onOpenChange(next);
            }
            else
            {
                *openState = next ? 1 : 0;
                desiredOpen = next;
                requestOpen = next;
            }
        }
        if (controlled)
        {
            int* lastOpen = storage->GetIntRef(stateId ^ 0x7F4A7C15u, 0);
            if (options.open && *lastOpen == 0)
            {
                requestOpen = true;
            }
            *lastOpen = options.open ? 1 : 0;
        }

        // 字段：日期文本 + 日历图标
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const GuiColor fieldColor = fieldHovered ? tokens.surfaceHover : tokens.surface;
        draw->AddRectFilled(fieldPos, fieldPos + fieldSize, fieldColor.ToImU32(), tokens.radius.control);
        draw->AddRect(fieldPos, fieldPos + fieldSize,
                      WithAlpha(tokens.border, 0.78f).ToImU32(),
                      tokens.radius.control, 1.0f, ImDrawFlags_RoundCornersAll);

        char dateText[16];
        snprintf(dateText, sizeof(dateText), "%04d-%02d-%02d", y, m, d);
        const ImVec2 textSize = ImGui::CalcTextSize(dateText);
        draw->AddText(ImVec2(fieldPos.x + 12.0f, fieldPos.y + (fieldHeight - textSize.y) * 0.5f),
                      tokens.text.ToImU32(), dateText);

        const ImVec2 iconExtent = ImGui::CalcTextSize(ICON_FA_CALENDAR_DAYS);
        draw->AddText(ImVec2(fieldPos.x + width - iconExtent.x - 12.0f,
                             fieldPos.y + (fieldHeight - iconExtent.y) * 0.5f),
                      tokens.primary.ToImU32(), ICON_FA_CALENDAR_DAYS);


        if (desiredOpen)
        {
            const float popupWidth = 260.0f;
            const float rowHeight = 28.0f;
            const float popupHeight = rowHeight * 5.0f + 16.0f;
            const ImVec2 popupPos(fieldPos.x + width - popupWidth,
                                  options.openUp ? (fieldPos.y - popupHeight - 4.0f)
                                                 : (fieldPos.y + fieldHeight + 4.0f));
            if (requestOpen)
            {
                ImGui::OpenPopup("##date_popup");
            }
            ImGui::SetNextWindowPos(popupPos);
            ImGui::SetNextWindowSize(ImVec2(popupWidth, 0.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, tokens.radius.popup);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg,
                                  MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.12f).ToImU32());
            ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(tokens.border, 0.78f).ToImU32());

            if (ImGui::BeginPopup("##date_popup"))
            {
                ImDrawList* popupDraw = ImGui::GetWindowDrawList();
                static const char* kMonthNames[12] = {
                    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                };
                const float gap = 8.0f;
                const float monthWidth = 92.0f;
                const float dayWidth = 60.0f;
                const float yearWidth = 76.0f;
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                const float columnHeight = rowHeight * 5.0f;

                // 月份列
                {
                    const ImVec2 columnPos(origin.x, origin.y);
                    popupDraw->AddRectFilled(columnPos,
                                        ImVec2(columnPos.x + monthWidth, columnPos.y + columnHeight),
                                        WithAlpha(tokens.surfaceHover, 0.35f).ToImU32(), 6.0f);
                    for (int offset = -2; offset <= 2; ++offset)
                    {
                        int value = m + offset;
                        while (value < 1) { value += 12; }
                        while (value > 12) { value -= 12; }

                        const ImVec2 rowPos(columnPos.x,
                                            columnPos.y + static_cast<float>(offset + 2) * rowHeight);
                        ImGui::PushID(offset + 100);
                        ImGui::SetCursorScreenPos(rowPos);
                        ImGui::InvisibleButton("##row", ImVec2(monthWidth, rowHeight));
                        const bool rowHovered = ImGui::IsItemHovered();
                        const bool rowClicked = ImGui::IsItemClicked();
                        ImGui::PopID();

                        const bool center = (offset == 0);
                        if (center)
                        {
                            popupDraw->AddRectFilled(rowPos, rowPos + ImVec2(monthWidth, rowHeight),
                                                WithAlpha(tokens.primary, 0.24f).ToImU32(), 5.0f);
                        }
                        else if (rowHovered)
                        {
                            popupDraw->AddRectFilled(rowPos, rowPos + ImVec2(monthWidth, rowHeight),
                                                WithAlpha(tokens.text, 0.08f).ToImU32(), 5.0f);
                        }

                        const char* label = kMonthNames[value - 1];
                        const ImVec2 labelSize = ImGui::CalcTextSize(label);
                        popupDraw->AddText(ImVec2(rowPos.x + (monthWidth - labelSize.x) * 0.5f,
                                             rowPos.y + (rowHeight - labelSize.y) * 0.5f),
                                      (center ? tokens.primary : tokens.text).ToImU32(), label);

                        if (rowClicked)
                        {
                            m = value;
                            d = std::min(d, DaysInMonth(y, m));
                            *month = m;
                            *day = d;
                            changed = true;
                        }
                    }
                    if (ImGui::IsMouseHoveringRect(
                            columnPos, ImVec2(columnPos.x + monthWidth, columnPos.y + columnHeight)) &&
                        ImGui::GetIO().MouseWheel != 0.0f)
                    {
                        m = (ImGui::GetIO().MouseWheel > 0.0f) ? (m % 12) + 1 : ((m + 10) % 12) + 1;
                        d = std::min(d, DaysInMonth(y, m));
                        *month = m;
                        *day = d;
                        changed = true;
                    }
                }

                // 日期列
                {
                    const ImVec2 columnPos(origin.x + monthWidth + gap, origin.y);
                    popupDraw->AddRectFilled(columnPos,
                                        ImVec2(columnPos.x + dayWidth, columnPos.y + columnHeight),
                                        WithAlpha(tokens.surfaceHover, 0.35f).ToImU32(), 6.0f);
                    const int maxDay = DaysInMonth(y, m);
                    for (int offset = -2; offset <= 2; ++offset)
                    {
                        int value = d + offset;
                        while (value < 1) { value += maxDay; }
                        while (value > maxDay) { value -= maxDay; }

                        const ImVec2 rowPos(columnPos.x,
                                            columnPos.y + static_cast<float>(offset + 2) * rowHeight);
                        ImGui::PushID(offset + 200);
                        ImGui::SetCursorScreenPos(rowPos);
                        ImGui::InvisibleButton("##row", ImVec2(dayWidth, rowHeight));
                        const bool rowHovered = ImGui::IsItemHovered();
                        const bool rowClicked = ImGui::IsItemClicked();
                        ImGui::PopID();

                        const bool center = (offset == 0);
                        if (center)
                        {
                            popupDraw->AddRectFilled(rowPos, rowPos + ImVec2(dayWidth, rowHeight),
                                                WithAlpha(tokens.primary, 0.24f).ToImU32(), 5.0f);
                        }
                        else if (rowHovered)
                        {
                            popupDraw->AddRectFilled(rowPos, rowPos + ImVec2(dayWidth, rowHeight),
                                                WithAlpha(tokens.text, 0.08f).ToImU32(), 5.0f);
                        }

                        char label[8];
                        snprintf(label, sizeof(label), "%02d", value);
                        const ImVec2 labelSize = ImGui::CalcTextSize(label);
                        popupDraw->AddText(ImVec2(rowPos.x + (dayWidth - labelSize.x) * 0.5f,
                                             rowPos.y + (rowHeight - labelSize.y) * 0.5f),
                                      (center ? tokens.primary : tokens.text).ToImU32(), label);

                        if (rowClicked)
                        {
                            d = value;
                            *day = d;
                            changed = true;
                        }
                    }
                    if (ImGui::IsMouseHoveringRect(
                            columnPos, ImVec2(columnPos.x + dayWidth, columnPos.y + columnHeight)) &&
                        ImGui::GetIO().MouseWheel != 0.0f)
                    {
                        d = (ImGui::GetIO().MouseWheel > 0.0f) ? (d % maxDay) + 1
                                                              : ((d + maxDay - 2) % maxDay) + 1;
                        *day = d;
                        changed = true;
                    }
                }

                // 年份列
                {
                    const ImVec2 columnPos(origin.x + monthWidth + gap + dayWidth + gap, origin.y);
                    popupDraw->AddRectFilled(columnPos,
                                        ImVec2(columnPos.x + yearWidth, columnPos.y + columnHeight),
                                        WithAlpha(tokens.surfaceHover, 0.35f).ToImU32(), 6.0f);
                    for (int offset = -2; offset <= 2; ++offset)
                    {
                        const int value = std::clamp(y + offset, 1900, 2200);
                        const ImVec2 rowPos(columnPos.x,
                                            columnPos.y + static_cast<float>(offset + 2) * rowHeight);
                        ImGui::PushID(offset + 300);
                        ImGui::SetCursorScreenPos(rowPos);
                        ImGui::InvisibleButton("##row", ImVec2(yearWidth, rowHeight));
                        const bool rowHovered = ImGui::IsItemHovered();
                        const bool rowClicked = ImGui::IsItemClicked();
                        ImGui::PopID();

                        const bool center = (offset == 0);
                        if (center)
                        {
                            popupDraw->AddRectFilled(rowPos, rowPos + ImVec2(yearWidth, rowHeight),
                                                WithAlpha(tokens.primary, 0.24f).ToImU32(), 5.0f);
                        }
                        else if (rowHovered)
                        {
                            popupDraw->AddRectFilled(rowPos, rowPos + ImVec2(yearWidth, rowHeight),
                                                WithAlpha(tokens.text, 0.08f).ToImU32(), 5.0f);
                        }

                        char label[8];
                        snprintf(label, sizeof(label), "%04d", value);
                        const ImVec2 labelSize = ImGui::CalcTextSize(label);
                        popupDraw->AddText(ImVec2(rowPos.x + (yearWidth - labelSize.x) * 0.5f,
                                             rowPos.y + (rowHeight - labelSize.y) * 0.5f),
                                      (center ? tokens.primary : tokens.text).ToImU32(), label);

                        if (rowClicked)
                        {
                            y = value;
                            d = std::min(d, DaysInMonth(y, m));
                            *year = y;
                            *day = d;
                            changed = true;
                        }
                    }
                    if (ImGui::IsMouseHoveringRect(
                            columnPos, ImVec2(columnPos.x + yearWidth, columnPos.y + columnHeight)) &&
                        ImGui::GetIO().MouseWheel != 0.0f)
                    {
                        y = std::clamp(y + (ImGui::GetIO().MouseWheel > 0.0f ? 1 : -1), 1900, 2200);
                        d = std::min(d, DaysInMonth(y, m));
                        *year = y;
                        *day = d;
                        changed = true;
                    }
                }

                ImGui::SetCursorScreenPos(origin);
                ImGui::Dummy(ImVec2(popupWidth - 16.0f, popupHeight - 16.0f));
                ImGui::EndPopup();
            }
            else
            {
                if (!controlled)
                {
                    *openState = 0;
                }
                else if (options.onOpenChange)
                {
                    options.onOpenChange(false);
                }
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }

        ImGui::PopID();
        return changed;
    }


    // 时间选择器：时、分两段合并为一行显示，左右箭头以 minuteStep 分钟为步进调整
    // 跨 60 分钟自动进位或借位到小时，值发生变化时返回 true
    bool GuiTimePicker(const char* label, int* hour, int* minute, int minuteStep)
    {
        if (hour == nullptr || minute == nullptr)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark();
        const int safeMinuteStep = std::clamp(minuteStep, 1, 30); // timepicker.h:66 的分钟步进范围
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
            // 以 minuteStep 分钟为步进：超出范围时循环归一，并同步进位 / 借位到小时
            currentMinute += step * safeMinuteStep;
            while (currentMinute > 59)
            {
                currentMinute -= 60;
                if (++currentHour > 23)
                {
                    currentHour = 0;
                }
            }
            while (currentMinute < 0)
            {
                currentMinute += 60;
                if (--currentHour < 0)
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

    // =========================================================================
    // 输入框变体（EUI input.h:58-61/81-85：多行 / 滚动条 / 占位符 / 回车 / 焦点）
    // 背景与边框自绘，输入控件叠在其上；空值且未激活时绘制占位文本
    // =========================================================================
    bool GuiInputTextEx(const char* label, std::string* value, const ImVec2& size,
                        const GuiInputOptions& options)
    {
        if (value == nullptr)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        const float width = size.x > 0.0f ? size.x : 220.0f;
        const float inset = std::max(0.0f, options.inset);
        const float lineHeight = std::max(20.0f, tokens.control.field);
        const int visibleLines = std::max(1, options.visibleLines);
        const float height = options.multiline
                                 ? (size.y > 0.0f ? size.y : lineHeight * static_cast<float>(visibleLines) + 10.0f)
                                 : (size.y > 0.0f ? size.y : lineHeight);

        ImGui::PushID(label != nullptr ? label : "##gui_input_ex");

        // 可选字体 / 字号（对应 EUI input.h 的 fontFamily / fontSize）
        const bool pushedFont = (options.font != nullptr || options.fontSize > 0.0f);
        if (pushedFont)
        {
            ImGui::PushFont(options.font, options.fontSize);
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + width, pos.y + height);
        const float rounding = tokens.radius.small;
        const float drawAlpha = options.disabled ? 0.50f : 1.0f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImGui::BeginDisabled(options.disabled);

        // 背景（先画，输入控件叠在其上）
        drawList->AddRectFilled(pos, maxPos, WithAlpha(tokens.surface, drawAlpha).ToImU32(), rounding);

        // 输入控件
        const float inputY = options.multiline
                                 ? inset * 0.5f
                                 : (height - ImGui::GetFrameHeight()) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(pos.x + inset, pos.y + inputY));
        ImGui::PushItemWidth(width - inset * 2.0f);

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
        if (options.password)
        {
            flags |= ImGuiInputTextFlags_Password;
        }
        if (options.readOnly)
        {
            flags |= ImGuiInputTextFlags_ReadOnly;
        }
        if (!options.scrollbar && options.multiline)
        {
            flags |= ImGuiInputTextFlags_NoHorizontalScroll;
        }

        bool edited = false;
        bool submitted = false;
        if (options.multiline)
        {
            edited = ImGui::InputTextMultiline("##input_multiline", value,
                                               ImVec2(width - inset * 2.0f, height - inset), flags);
        }
        else
        {
            submitted = ImGui::InputText("##input_single", value,
                                         flags | ImGuiInputTextFlags_EnterReturnsTrue);
            edited = submitted || ImGui::IsItemEdited();
        }

        const bool active = ImGui::IsItemActive();
        ImGui::PopItemWidth();

        // 长度限制：在编辑后截断（std::string 版本无需自定义回调）
        if (options.maxLength > 0 && static_cast<int>(value->size()) > options.maxLength)
        {
            value->resize(static_cast<std::size_t>(options.maxLength));
            edited = true;
        }
        if (edited && options.onChange)
        {
            options.onChange(*value);
        }

        // 边框：聚焦时用 primary 高亮，否则 border 常规色
        const GuiColor borderColor = active ? tokens.primary : WithAlpha(tokens.border, 0.70f);
        drawList->AddRect(pos, maxPos, WithAlpha(borderColor, borderColor.a * drawAlpha).ToImU32(),
                          rounding, active ? 1.5f : 1.0f, ImDrawFlags_RoundCornersAll);

        // 占位符：空值且未聚焦时绘制
        if (options.placeholder != nullptr && options.placeholder[0] != '\0' &&
            value->empty() && !active)
        {
            ImFont* font = ImGui::GetFont();
            if (font != nullptr)
            {
                const float fontSize = ImGui::GetFontSize();
                const float placeY = options.multiline
                                         ? pos.y + inset
                                         : pos.y + (height - fontSize) * 0.5f;
                const ImVec2 placePos(pos.x + inset, placeY);
                drawList->AddText(font, fontSize, placePos,
                                  WithAlpha(tokens.text, 0.38f * drawAlpha).ToImU32(),
                                  options.placeholder);
            }
        }

        // 焦点回调：跨帧比较上一帧焦点状态（ImGuiStorage 保存，避免堆分配）
        if (options.onFocus)
        {
            const ImGuiID focusKey = ImGui::GetID("##input_focus");
            const int previous = ImGui::GetStateStorage()->GetInt(focusKey, active ? 1 : 0);
            const int currentFlag = active ? 1 : 0;
            if (previous != currentFlag)
            {
                ImGui::GetStateStorage()->SetInt(focusKey, currentFlag);
                options.onFocus(active);
            }
        }

        if (submitted && options.onEnter)
        {
            options.onEnter();
        }

        ImGui::EndDisabled();
        if (pushedFont)
        {
            ImGui::PopFont();
        }

        // 推进光标到控件下方
        ImGui::SetCursorScreenPos(ImVec2(pos.x, maxPos.y));
        ImGui::PopID();
        return edited;
    }

}
