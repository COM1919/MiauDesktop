// GuiOverlay.cpp
// 浮层与反馈控件的实现（声明见 BaseUI/GUIBase.h）
// 移植自 EUI-NEO components/tooltip.h / dialog.h / contextmenu.h / toast.h
// EUI-NEO 为保留模式 DSL，此处按 ImGui 立即模式重写，视觉风格与 EUI 暗色主题保持一致

#include "GUIBase.h"

#include <algorithm>
#include <string>

namespace GuiBase
{
    // 悬浮提示：在当前鼠标位置（或指定位置）附近绘制一个小浮层
    // EUI 视觉：surface 混黑背景 + 圆角 + 边框 + 软阴影 + 提示文字
    // 通过 ForegroundDrawList 绘制，确保浮层置顶
    void GuiTooltip(const char* text, const ImVec2& position)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        GuiThemeTokens tokens = Theme::Dark();

        // 定位：position 为 (-1, -1) 时跟随鼠标并偏移 (12, 12)
        ImVec2 anchor = position;
        if (position.x < 0.0f && position.y < 0.0f)
        {
            anchor = io.MousePos + ImVec2(12.0f, 12.0f);
        }

        ImFont* font = ImGui::GetFont();
        const float fontSize = tokens.typography.hint;

        // 文字尺寸加内边距得到浮层尺寸
        const float paddingX = 12.0f;
        const float paddingY = 7.0f;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        ImVec2 boxSize(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);

        // 屏幕边界保护：右侧放不下时翻转到左侧，底部放不下时上移
        ImVec2 pos = anchor;
        if (pos.x + boxSize.x > io.DisplaySize.x)
        {
            pos.x = anchor.x - boxSize.x - 24.0f;
        }
        if (pos.y + boxSize.y > io.DisplaySize.y)
        {
            pos.y = io.DisplaySize.y - boxSize.y - 8.0f;
        }
        pos.x = std::max(8.0f, pos.x);
        pos.y = std::max(8.0f, pos.y);

        // 按下述 z-order 自绘：软阴影 → 背景圆角矩形 → 边框描边 → 文字
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        GuiColor background = MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f), 0.18f);
        GuiColor border = WithAlpha(tokens.border, 0.76f);
        GuiShadowStyle shadow = GetShadowStyle(GuiShadowLevel::Panel, true);
        const float radius = tokens.radius.tooltip;

        DrawGuiSoftShadow(drawList, pos, boxSize, shadow, radius);
        drawList->AddRectFilled(pos, pos + boxSize, background.ToImU32(), radius, ImDrawFlags_RoundCornersAll);
        drawList->AddRect(pos, pos + boxSize, border.ToImU32(), radius, ImDrawFlags_RoundCornersAll, 1.0f);

        // 文字垂直居中
        ImVec2 textPos = pos + ImVec2(paddingX, (boxSize.y - textSize.y) * 0.5f);
        drawList->AddText(font, fontSize, textPos, tokens.text.ToImU32(), text);
    }

    // 模态对话框：遮罩 + 居中面板（标题 + 内容 + 确定按钮）
    // 点击遮罩或确定按钮都会关闭对话框；点击面板空白不关闭
    // 返回 true 表示对话框被关闭
    bool GuiDialog(const char* title, bool* open, const char* content)
    {
        if (open == nullptr || *open == false)
        {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        GuiThemeTokens tokens = Theme::Dark();

        bool closed = false;
        ImGui::PushID(title);

        // 全屏透明窗口承载模态输入，阻止点击穿透到下层窗口
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::Begin("##GuiDialog", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoNav))
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 遮罩：半透明黑色覆盖全屏
            drawList->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize,
                                    GuiColor(0.0f, 0.0f, 0.0f, 0.46f).ToImU32());

            // 面板几何：固定尺寸并居中
            const float panelWidth = 420.0f;
            const float panelHeight = 220.0f;
            ImVec2 panelPos((io.DisplaySize.x - panelWidth) * 0.5f,
                            (io.DisplaySize.y - panelHeight) * 0.5f);
            ImVec2 panelSize(panelWidth, panelHeight);

            // 全屏透明按钮捕获遮罩点击：点击即关闭（位于面板下层）
            ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
            if (ImGui::InvisibleButton("##backdrop", io.DisplaySize))
            {
                *open = false;
                closed = true;
            }

            // 面板命中区域拦截：点击面板空白不穿透到遮罩（位于遮罩上层）
            ImGui::SetCursorScreenPos(panelPos);
            ImGui::InvisibleButton("##panel.hit", panelSize);

            // 绘制面板：软阴影 → 背景圆角矩形 → 边框描边
            GuiShadowStyle shadow = GetShadowStyle(GuiShadowLevel::Dialog, true);
            const float radius = tokens.radius.section;
            DrawGuiSoftShadow(drawList, panelPos, panelSize, shadow, radius);
            drawList->AddRectFilled(panelPos, panelPos + panelSize,
                                    tokens.surface.ToImU32(), radius, ImDrawFlags_RoundCornersAll);
            drawList->AddRect(panelPos, panelPos + panelSize,
                              WithAlpha(tokens.border, 0.82f).ToImU32(),
                              radius, ImDrawFlags_RoundCornersAll, 1.0f);

            ImFont* font = ImGui::GetFont();

            // 标题：面板左上角内边距处，标题字号
            if (title != nullptr && title[0] != '\0')
            {
                drawList->AddText(font, tokens.typography.heading,
                                  panelPos + ImVec2(tokens.spacing.panel, 24.0f),
                                  tokens.text.ToImU32(), title);
            }

            // 内容：可换行，裁剪在内容区域内，正文色
            const float contentWidth = panelWidth - tokens.spacing.panel * 2.0f;
            ImVec2 contentPos = panelPos + ImVec2(tokens.spacing.panel, 68.0f);
            if (content != nullptr && content[0] != '\0')
            {
                ImVec2 contentMin = contentPos;
                ImVec2 contentMax = panelPos + ImVec2(panelWidth - tokens.spacing.panel,
                                                      panelHeight - 24.0f);
                ImVec4 clipRect(contentMin.x, contentMin.y, contentMax.x, contentMax.y);
                GuiColor bodyColor = WithAlpha(tokens.text, 0.78f);
                drawList->AddText(font, tokens.typography.input, contentPos,
                                  bodyColor.ToImU32(), content, nullptr, contentWidth, &clipRect);
            }

            // 确定按钮：右下角，EUI 主色按钮（primary 底 + hover 提亮 + active 压暗）
            const float buttonWidth = 120.0f;
            const float buttonHeight = 40.0f;
            ImVec2 buttonPos = panelPos + ImVec2(panelWidth - tokens.spacing.panel - buttonWidth,
                                                 panelHeight - tokens.spacing.panel - buttonHeight);
            ImGui::SetCursorScreenPos(buttonPos);
            ImGui::PushStyleColor(ImGuiCol_Button, tokens.primary.ToImVec4());
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  MixColor(tokens.primary, GuiColor(1.0f, 1.0f, 1.0f), 0.14f).ToImVec4());
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  MixColor(tokens.primary, GuiColor(0.0f, 0.0f, 0.0f), 0.16f).ToImVec4());
            ImGui::PushStyleColor(ImGuiCol_Text, GuiColor(1.0f, 1.0f, 1.0f, 1.0f).ToImVec4());
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens.radius.popup);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 8.0f));
            if (ImGui::Button("确定", ImVec2(buttonWidth, buttonHeight)))
            {
                *open = false;
                closed = true;
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();

        return closed;
    }

    // 右键菜单：配合 IsMouseReleased(1) 调用，右键释放时在鼠标位置弹出菜单
    // 点击菜单项选中并返回 true，点击菜单外区域关闭
    bool GuiContextMenu(const char* label, const char* const* items, int itemCount,
                        int* selectedIndex)
    {
        ImGuiIO& io = ImGui::GetIO();

        // 打开状态：当前打开的菜单 label 与其弹出位置
        static std::string s_openLabel;
        static ImVec2 s_openPos(0.0f, 0.0f);

        // 右键释放：打开（或重定位）本菜单
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            s_openLabel = label;
            s_openPos = io.MousePos;
        }

        // 本菜单未处于打开状态：直接返回
        if (s_openLabel != label)
        {
            return false;
        }

        // 无菜单项：不弹菜单
        if (items == nullptr || itemCount <= 0)
        {
            s_openLabel.clear();
            return false;
        }

        bool result = false;
        ImGui::PushID(label);

        // 全屏透明窗口承载模态输入，保证菜单点击与外部关闭可被捕获
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::Begin("##GuiContextMenu", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoNav))
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            GuiThemeTokens tokens = Theme::Dark();

            // 菜单几何：宽度取最长项自适应，高度按项数计算
            const float itemHeight = 32.0f;
            const float inset = tokens.spacing.small;
            float menuWidth = 190.0f;
            for (int i = 0; i < itemCount; i++)
            {
                if (items[i] == nullptr)
                {
                    continue;
                }
                float itemTextWidth = ImGui::CalcTextSize(items[i]).x;
                menuWidth = std::max(menuWidth, itemTextWidth + inset + tokens.spacing.content + 30.0f);
            }
            menuWidth = std::min(menuWidth, io.DisplaySize.x - tokens.spacing.section * 2.0f);
            const float menuHeight = itemHeight * static_cast<float>(itemCount) + inset * 2.0f;

            // 菜单位置：clamp 在屏幕内（EUI 的 clampMenuY / 横向 xInset 逻辑）
            ImVec2 menuPos = s_openPos;
            menuPos.x = std::clamp(menuPos.x, tokens.spacing.compact,
                                   std::max(tokens.spacing.compact, io.DisplaySize.x - menuWidth - tokens.spacing.compact));
            menuPos.y = std::clamp(menuPos.y, 8.0f,
                                   std::max(8.0f, io.DisplaySize.y - menuHeight - 8.0f));

            // 全屏透明按钮捕获外部点击：点击即关闭（位于最底层）
            ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
            if (ImGui::InvisibleButton("##backdrop", io.DisplaySize))
            {
                s_openLabel.clear();
            }

            // 菜单命中区域拦截：点击菜单空白不穿透到遮罩（位于遮罩上层）
            ImGui::SetCursorScreenPos(menuPos);
            ImGui::InvisibleButton("##menu.hit", ImVec2(menuWidth, menuHeight));

            // 绘制菜单背景：软阴影 → 背景圆角矩形 → 边框描边
            GuiColor background = MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f), 0.16f);
            GuiColor border = WithAlpha(tokens.border, 0.82f);
            GuiShadowStyle shadow = GetShadowStyle(GuiShadowLevel::Panel, true);
            const float radius = tokens.radius.card;
            DrawGuiSoftShadow(drawList, menuPos, ImVec2(menuWidth, menuHeight), shadow, radius);
            drawList->AddRectFilled(menuPos, menuPos + ImVec2(menuWidth, menuHeight),
                                    background.ToImU32(), radius, ImDrawFlags_RoundCornersAll);
            drawList->AddRect(menuPos, menuPos + ImVec2(menuWidth, menuHeight),
                              border.ToImU32(), radius, ImDrawFlags_RoundCornersAll, 1.0f);

            // 菜单项：hover 高亮 surfaceHover，按下 surfaceActive，点击选中
            ImFont* font = ImGui::GetFont();
            const float itemRounding = tokens.radius.control;
            for (int i = 0; i < itemCount; i++)
            {
                if (items[i] == nullptr)
                {
                    continue;
                }

                ImVec2 itemPos = menuPos + ImVec2(inset, inset + itemHeight * static_cast<float>(i));
                ImVec2 itemSize(menuWidth - inset * 2.0f, itemHeight);

                ImGui::SetCursorScreenPos(itemPos);
                ImGui::PushID(i);
                bool clicked = ImGui::InvisibleButton("##item", itemSize);
                bool isHovered = ImGui::IsItemHovered();
                bool isActive = ImGui::IsItemActive();

                // 按状态绘制项背景
                GuiColor itemColor = background;
                if (isActive)
                {
                    itemColor = tokens.surfaceActive;
                }
                else if (isHovered)
                {
                    itemColor = tokens.surfaceHover;
                }
                drawList->AddRectFilled(itemPos, itemPos + itemSize,
                                        itemColor.ToImU32(), itemRounding, ImDrawFlags_RoundCornersAll);

                // 项文字：垂直居中，左侧留出内边距
                ImVec2 textSize = ImGui::CalcTextSize(items[i]);
                ImVec2 textPos = itemPos + ImVec2(tokens.spacing.content,
                                                  (itemSize.y - textSize.y) * 0.5f);
                drawList->AddText(font, tokens.typography.option, textPos,
                                  tokens.text.ToImU32(), items[i]);

                if (clicked)
                {
                    *selectedIndex = i;
                    s_openLabel.clear();
                    result = true;
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();

        return result;
    }

    // 轻提示：短暂显示后自动消失，屏幕右下角浮层
    // 首次调用记录消息与开始时间，超过 duration 后不再绘制
    // 通过 ForegroundDrawList 绘制，确保置顶
    void GuiToast(const char* text, float duration)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return;
        }

        // 静态状态：当前消息与其开始时间
        struct ToastState
        {
            std::string message;
            double startTime = 0.0;
            bool active = false;
        };
        static ToastState s_state;

        const double now = ImGui::GetTime();
        double elapsed = s_state.active ? now - s_state.startTime : 0.0;

        // 无活动消息或消息发生变化：记录新消息并开始计时
        if (!s_state.active || s_state.message != text)
        {
            s_state.message = text;
            s_state.startTime = now;
            s_state.active = true;
            elapsed = 0.0;
        }

        // 超过时长：停止绘制
        if (elapsed >= static_cast<double>(duration))
        {
            s_state.active = false;
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        GuiThemeTokens tokens = Theme::Dark();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float fontSize = tokens.typography.label;

        // 尺寸：宽度自适应内容，上限 360
        const float paddingX = 20.0f;
        const float paddingY = 14.0f;
        const float maxWidth = std::min(360.0f, io.DisplaySize.x - tokens.spacing.section * 2.0f);
        const float wrapWidth = std::max(0.0f, maxWidth - paddingX * 2.0f);
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, s_state.message.c_str());
        ImVec2 boxSize(std::min(maxWidth, textSize.x + paddingX * 2.0f),
                       textSize.y + paddingY * 2.0f);

        // 位置：屏幕右下角（EUI toast 定位）
        ImVec2 pos(io.DisplaySize.x - boxSize.x - tokens.spacing.section,
                   io.DisplaySize.y - boxSize.y - tokens.spacing.section);

        // 淡入淡出：进入 0.16 秒渐显，结束前 0.24 秒渐隐
        float alpha = 1.0f;
        const float fadeIn = 0.16f;
        const float fadeOut = 0.24f;
        if (elapsed < fadeIn)
        {
            alpha = static_cast<float>(elapsed / fadeIn);
        }
        const float remaining = std::max(0.0f, duration - static_cast<float>(elapsed));
        if (remaining < fadeOut)
        {
            alpha = std::min(alpha, remaining / fadeOut);
        }

        // 各层颜色应用透明度
        GuiColor background = MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f), 0.18f);
        background.a *= alpha;
        GuiColor border = WithAlpha(tokens.border, 0.82f * alpha);
        GuiColor textColor = tokens.text;
        textColor.a *= alpha;
        GuiShadowStyle shadow = GetShadowStyle(GuiShadowLevel::Panel, true);
        shadow.color.a *= alpha;
        const float radius = tokens.radius.elevated;

        // 按下述 z-order 自绘：软阴影 → 背景圆角矩形 → 边框描边 → 文字
        DrawGuiSoftShadow(drawList, pos, boxSize, shadow, radius);
        drawList->AddRectFilled(pos, pos + boxSize, background.ToImU32(), radius, ImDrawFlags_RoundCornersAll);
        drawList->AddRect(pos, pos + boxSize, border.ToImU32(), radius, ImDrawFlags_RoundCornersAll, 1.0f);

        // 文字垂直居中，超宽自动换行
        ImVec2 textPos = pos + ImVec2(paddingX, (boxSize.y - textSize.y) * 0.5f);
        drawList->AddText(font, fontSize, textPos, textColor.ToImU32(),
                          s_state.message.c_str(), nullptr, wrapWidth);
    }

    // =========================================================================
    // 轻提示变体（EUI toast.h:52-55/65-66：标题 / 图标 / 手动关闭 / 自动关闭回调）
    // 图标 + 标题 + 正文三行布局，右上角关闭按钮，超时触发 onAutoDismiss
    // 通过 ForegroundDrawList 绘制并手动做鼠标命中（不占用布局）
    // =========================================================================
    void GuiToastEx(const char* text, float duration, const GuiToastOptions& options)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return;
        }

        // 变体专用的静态状态（与 GuiToast 互不干扰）
        struct ToastVariantState
        {
            std::string message;
            std::string title;
            std::string icon;
            double startTime = 0.0;
            bool active = false;
            bool autoDismissFired = false;
        };
        static ToastVariantState s_variant;

        const double now = ImGui::GetTime();
        if (!s_variant.active || s_variant.message != text)
        {
            s_variant.message = text;
            s_variant.title = (options.title != nullptr) ? options.title : "";
            s_variant.icon = (options.icon != nullptr) ? options.icon : "";
            s_variant.startTime = now;
            s_variant.active = true;
            s_variant.autoDismissFired = false;
        }

        const double elapsed = now - s_variant.startTime;
        if (elapsed >= static_cast<double>(duration))
        {
            if (!s_variant.autoDismissFired)
            {
                s_variant.autoDismissFired = true;
                if (options.onAutoDismiss)
                {
                    options.onAutoDismiss();
                }
            }
            s_variant.active = false;
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        GuiThemeTokens tokens = Theme::Dark();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        if (font == nullptr)
        {
            return;
        }

        const float fontSize = tokens.typography.label;
        const float titleSize = tokens.typography.option;
        const float paddingX = 20.0f;
        const float paddingY = 14.0f;
        const float closeSize = 18.0f;
        const float iconSize = tokens.typography.heading;
        const bool hasIcon = !s_variant.icon.empty();
        const bool hasTitle = !s_variant.title.empty();

        const float maxWidth = std::min(380.0f, io.DisplaySize.x - tokens.spacing.section * 2.0f);
        const float reservedWidth = paddingX * 2.0f + closeSize + 8.0f + (hasIcon ? (iconSize + 10.0f) : 0.0f);
        const float wrapWidth = std::max(60.0f, maxWidth - reservedWidth);

        const ImVec2 textExtent = font->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, s_variant.message.c_str());
        const ImVec2 titleExtent = hasTitle
                                       ? font->CalcTextSizeA(titleSize, FLT_MAX, wrapWidth, s_variant.title.c_str())
                                       : ImVec2(0.0f, 0.0f);
        const float textWidth = std::max(textExtent.x, titleExtent.x);
        const float textHeight = textExtent.y + (hasTitle ? (titleExtent.y + tokens.spacing.tiny) : 0.0f);

        const ImVec2 boxSize(std::min(maxWidth, textWidth + reservedWidth),
                             textHeight + paddingY * 2.0f);
        const ImVec2 pos(io.DisplaySize.x - boxSize.x - tokens.spacing.section,
                         io.DisplaySize.y - boxSize.y - tokens.spacing.section);

        // 淡入淡出
        float alpha = 1.0f;
        const float fadeIn = 0.16f;
        const float fadeOut = 0.24f;
        if (elapsed < fadeIn)
        {
            alpha = static_cast<float>(elapsed / fadeIn);
        }
        const float remaining = std::max(0.0f, duration - static_cast<float>(elapsed));
        if (remaining < fadeOut)
        {
            alpha = std::min(alpha, remaining / fadeOut);
        }

        GuiColor background = MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f), 0.18f);
        background.a *= alpha;
        GuiShadowStyle shadow = GetShadowStyle(GuiShadowLevel::Panel, true);
        shadow.color.a *= alpha;
        const float radius = tokens.radius.elevated;

        DrawGuiSoftShadow(drawList, pos, boxSize, shadow, radius);
        drawList->AddRectFilled(pos, pos + boxSize, background.ToImU32(), radius, ImDrawFlags_RoundCornersAll);
        drawList->AddRect(pos, pos + boxSize, WithAlpha(tokens.border, 0.82f * alpha).ToImU32(),
                          radius, ImDrawFlags_RoundCornersAll, 1.0f);

        // 图标
        float contentX = pos.x + paddingX;
        const float contentTop = pos.y + paddingY;
        if (hasIcon)
        {
            const ImVec2 iconExtent = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, s_variant.icon.c_str());
            drawList->AddText(font, iconSize,
                              ImVec2(contentX, contentTop + (textHeight - iconExtent.y) * 0.5f),
                              WithAlpha(tokens.primary, alpha).ToImU32(), s_variant.icon.c_str());
            contentX += iconSize + 10.0f;
        }

        // 标题 + 正文
        float textY = contentTop;
        if (hasTitle)
        {
            drawList->AddText(font, titleSize, ImVec2(contentX, textY),
                              WithAlpha(tokens.text, alpha).ToImU32(), s_variant.title.c_str());
            textY += titleExtent.y + tokens.spacing.tiny;
        }
        drawList->AddText(font, fontSize, ImVec2(contentX, textY),
                          WithAlpha(tokens.text, 0.86f * alpha).ToImU32(),
                          s_variant.message.c_str(), nullptr, wrapWidth);

        // 关闭按钮：手动命中（不推进布局）
        const ImVec2 closeMin(pos.x + boxSize.x - closeSize - 8.0f, pos.y + 8.0f);
        const ImVec2 closeMax(closeMin.x + closeSize, closeMin.y + closeSize);
        const ImVec2 closeCenter((closeMin.x + closeMax.x) * 0.5f, (closeMin.y + closeMax.y) * 0.5f);
        const bool closeHovered = ImGui::IsMouseHoveringRect(closeMin, closeMax, false);
        if (closeHovered)
        {
            drawList->AddCircleFilled(closeCenter, closeSize * 0.5f,
                                      WithAlpha(tokens.text, 0.12f * alpha).ToImU32(), 16);
        }
        const ImVec2 closeIconExtent =
            font->CalcTextSizeA(tokens.typography.caption, FLT_MAX, 0.0f, ICON_FA_XMARK);
        drawList->AddText(font, tokens.typography.caption,
                          ImVec2(closeCenter.x - closeIconExtent.x * 0.5f,
                                 closeCenter.y - closeIconExtent.y * 0.5f),
                          WithAlpha(tokens.text, 0.72f * alpha).ToImU32(), ICON_FA_XMARK);

        if (closeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            s_variant.active = false;
            if (options.onDismiss)
            {
                options.onDismiss();
            }
        }
    }

    // =========================================================================
    // Tooltip 绑定（EUI tooltip.h:37-40：source 关联 + anchor / bounds）
    // 必须紧跟目标控件之后调用，内部用 IsItemHovered 读取该控件的悬停状态；
    // anchor 为 (-1,-1) 时定位到控件下方居中；bounds 非零时把浮层限制在其内
    // =========================================================================
    void GuiTooltipFor(const char* sourceId, const char* text, const ImVec2& anchor,
                       const ImVec2& bounds)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return;
        }

        // 目标控件必须处于悬停状态（ImGui 要求中间不插入其他 item）
        if (!ImGui::IsItemHovered())
        {
            return;
        }

        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImVec2 position = anchor;
        if (position.x < 0.0f || position.y < 0.0f)
        {
            position = ImVec2((itemMin.x + itemMax.x) * 0.5f, itemMax.y + 8.0f);
        }

        // bounds 限制：把锚点收进给定矩形，避免浮层被裁掉
        if (bounds.x > 0.0f && bounds.y > 0.0f)
        {
            position.x = std::clamp(position.x, itemMin.x, itemMin.x + bounds.x);
            position.y = std::clamp(position.y, itemMin.y, itemMin.y + bounds.y);
        }

        ImGui::PushID(sourceId != nullptr ? sourceId : "##gui_tooltip_for");
        GuiTooltip(text, position);
        ImGui::PopID();
    }

    // =========================================================================
    // 对话框变体（EUI dialog.h:64-65/75-77：主/次按钮文字 + 内容回调）
    // 面板结构与 GuiDialog 一致，额外支持次级按钮与自定义内容
    // =========================================================================
    bool GuiDialogEx(const char* title, bool* open, const char* message, const GuiDialogOptions& options)
    {
        if (open == nullptr || !(*open))
        {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        GuiThemeTokens tokens = Theme::Dark();
        bool closed = false;

        ImGui::PushID(title);
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::Begin("##GuiDialogEx", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNav))
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 遮罩
            drawList->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize,
                                    GuiColor(0.0f, 0.0f, 0.0f, 0.46f).ToImU32());

            const float panelWidth = 420.0f;
            const float panelHeight = options.content ? 280.0f : 220.0f;
            const ImVec2 panelPos((io.DisplaySize.x - panelWidth) * 0.5f,
                                  (io.DisplaySize.y - panelHeight) * 0.5f);
            const ImVec2 panelSize(panelWidth, panelHeight);

            // 遮罩点击关闭
            ImGui::SetCursorScreenPos(ImVec2(0.0f, 0.0f));
            if (ImGui::InvisibleButton("##backdrop", io.DisplaySize))
            {
                *open = false;
                closed = true;
            }

            // 面板命中区（阻止穿透）
            ImGui::SetCursorScreenPos(panelPos);
            ImGui::InvisibleButton("##panel.hit", panelSize);

            // 面板外观
            GuiShadowStyle shadow = GetShadowStyle(GuiShadowLevel::Dialog, true);
            const float radius = tokens.radius.section;
            DrawGuiSoftShadow(drawList, panelPos, panelSize, shadow, radius);
            drawList->AddRectFilled(panelPos, panelPos + panelSize, tokens.surface.ToImU32(),
                                    radius, ImDrawFlags_RoundCornersAll);
            drawList->AddRect(panelPos, panelPos + panelSize, WithAlpha(tokens.border, 0.82f).ToImU32(),
                              radius, ImDrawFlags_RoundCornersAll, 1.0f);

            ImFont* font = ImGui::GetFont();
            if (font != nullptr && title != nullptr && title[0] != '\0')
            {
                drawList->AddText(font, tokens.typography.heading,
                                  panelPos + ImVec2(tokens.spacing.panel, 24.0f),
                                  tokens.text.ToImU32(), title);
            }

            // 内容：优先 content 回调，其次纯文本消息
            const float contentWidth = panelWidth - tokens.spacing.panel * 2.0f;
            const ImVec2 contentPos = panelPos + ImVec2(tokens.spacing.panel, 68.0f);
            const ImVec2 contentMax = panelPos + ImVec2(panelWidth - tokens.spacing.panel,
                                                        panelHeight - 76.0f);
            if (options.content)
            {
                const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
                ImGui::PushClipRect(contentPos, contentMax, true);
                ImGui::SetCursorScreenPos(contentPos);
                options.content();
                ImGui::PopClipRect();
                ImGui::SetCursorScreenPos(savedCursor);
            }
            else if (font != nullptr && message != nullptr && message[0] != '\0')
            {
                const ImVec4 clipRect(contentPos.x, contentPos.y, contentMax.x, contentMax.y);
                drawList->AddText(font, tokens.typography.input, contentPos,
                                  WithAlpha(tokens.text, 0.78f).ToImU32(), message, nullptr,
                                  contentWidth, &clipRect);
            }

            // 按钮区：次级按钮在左，主按钮在右
            const float buttonWidth = 108.0f;
            const float buttonHeight = 40.0f;
            const float buttonGap = tokens.spacing.content;
            const bool hasSecondary = (options.secondaryText != nullptr && options.secondaryText[0] != '\0');
            float buttonX = panelPos.x + panelWidth - tokens.spacing.panel -
                            (hasSecondary ? (buttonWidth * 2.0f + buttonGap) : buttonWidth);
            const float buttonY = panelPos.y + panelHeight - tokens.spacing.panel - buttonHeight;

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens.radius.popup);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 8.0f));

            if (hasSecondary)
            {
                ImGui::SetCursorScreenPos(ImVec2(buttonX, buttonY));
                ImGui::PushStyleColor(ImGuiCol_Button, tokens.surface.ToImVec4());
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, tokens.surfaceHover.ToImVec4());
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, tokens.surfaceActive.ToImVec4());
                ImGui::PushStyleColor(ImGuiCol_Text, tokens.text.ToImVec4());
                if (ImGui::Button(options.secondaryText, ImVec2(buttonWidth, buttonHeight)))
                {
                    if (options.onSecondary)
                    {
                        options.onSecondary();
                    }
                    *open = false;
                    closed = true;
                }
                ImGui::PopStyleColor(4);
                buttonX += buttonWidth + buttonGap;
            }

            ImGui::SetCursorScreenPos(ImVec2(buttonX, buttonY));
            ImGui::PushStyleColor(ImGuiCol_Button, tokens.primary.ToImVec4());
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  MixColor(tokens.primary, GuiColor(1.0f, 1.0f, 1.0f), 0.14f).ToImVec4());
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  MixColor(tokens.primary, GuiColor(0.0f, 0.0f, 0.0f), 0.16f).ToImVec4());
            ImGui::PushStyleColor(ImGuiCol_Text, GuiColor(1.0f, 1.0f, 1.0f, 1.0f).ToImVec4());
            const char* primaryText = (options.primaryText != nullptr && options.primaryText[0] != '\0')
                                          ? options.primaryText
                                          : "确定";
            if (ImGui::Button(primaryText, ImVec2(buttonWidth, buttonHeight)))
            {
                if (options.onPrimary)
                {
                    options.onPrimary();
                }
                *open = false;
                closed = true;
            }
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();

        return closed;
    }

    // =========================================================================
    // 多级右键菜单（EUI contextmenu.h:98/108：children 嵌套 + onSelectPath）
    // 逐级自绘面板（最多 4 级）：悬停含子项的条目即在右侧展开下一级；
    // 点击叶子项写出 value 并关闭，点击菜单外区域关闭
    // =========================================================================
    bool GuiContextMenuTree(const char* label, const GuiMenuItem* items, int itemCount,
                            int* selectedValue)
    {
        if (label == nullptr || items == nullptr || itemCount <= 0)
        {
            return false;
        }

        constexpr int kMaxMenuDepth = 4;
        static std::string s_treeOpenLabel;
        static ImVec2 s_treeOpenPos(0.0f, 0.0f);

        ImGuiIO& io = ImGui::GetIO();
        GuiThemeTokens tokens = Theme::Dark();

        // 右键释放：打开或重定位本菜单
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            s_treeOpenLabel = label;
            s_treeOpenPos = io.MousePos;
        }
        if (s_treeOpenLabel != label)
        {
            return false;
        }

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        if (font == nullptr)
        {
            return false;
        }

        const float itemHeight = 30.0f;
        const float panelPadding = 6.0f;
        const float panelWidth = 190.0f;
        const float rounding = tokens.radius.popup;
        const float fontSize = tokens.typography.option;

        bool selected = false;
        bool anyHovered = false;
        bool clickedInside = false;

        const GuiMenuItem* levelItems = items;
        int levelCount = itemCount;
        ImVec2 levelPos = s_treeOpenPos;

        for (int depth = 0; depth < kMaxMenuDepth && levelItems != nullptr && levelCount > 0; ++depth)
        {
            const float panelHeight = panelPadding * 2.0f + itemHeight * static_cast<float>(levelCount);
            ImVec2 panelMin = levelPos;
            if (panelMin.x + panelWidth > io.DisplaySize.x)
            {
                panelMin.x = std::max(0.0f, io.DisplaySize.x - panelWidth - 4.0f);
            }
            if (panelMin.y + panelHeight > io.DisplaySize.y)
            {
                panelMin.y = std::max(0.0f, io.DisplaySize.y - panelHeight - 4.0f);
            }
            const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + panelHeight);

            GuiShadowStyle shadow = GetShadowStyle(GuiShadowLevel::Panel, true);
            DrawGuiSoftShadow(drawList, panelMin, ImVec2(panelWidth, panelHeight), shadow, rounding);
            drawList->AddRectFilled(panelMin, panelMax,
                                    MixColor(tokens.surface, GuiColor(0.0f, 0.0f, 0.0f), 0.12f).ToImU32(),
                                    rounding);
            drawList->AddRect(panelMin, panelMax, WithAlpha(tokens.border, 0.82f).ToImU32(),
                              rounding, ImDrawFlags_RoundCornersAll, 1.0f);

            if (ImGui::IsMouseHoveringRect(panelMin, panelMax, false))
            {
                anyHovered = true;
            }

            int hoveredIndex = -1;
            const GuiMenuItem* nextItems = nullptr;
            int nextCount = 0;

            for (int index = 0; index < levelCount; ++index)
            {
                const GuiMenuItem& item = levelItems[index];
                const ImVec2 itemMin(panelMin.x + panelPadding, panelMin.y + panelPadding + itemHeight * index);
                const ImVec2 itemMax(panelMax.x - panelPadding, itemMin.y + itemHeight);
                const bool hovered = ImGui::IsMouseHoveringRect(itemMin, itemMax, false);
                const bool hasChildren = (item.children != nullptr && item.childCount > 0);

                if (hovered)
                {
                    hoveredIndex = index;
                    anyHovered = true;
                    drawList->AddRectFilled(itemMin, itemMax, WithAlpha(tokens.surfaceHover, 0.90f).ToImU32(),
                                            tokens.radius.small);
                }

                if (item.label != nullptr && item.label[0] != '\0')
                {
                    const ImVec2 textExtent = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, item.label);
                    drawList->AddText(font, fontSize,
                                      ImVec2(itemMin.x + 10.0f, itemMin.y + (itemHeight - textExtent.y) * 0.5f),
                                      tokens.text.ToImU32(), item.label);
                }

                if (hasChildren)
                {
                    const float arrowSize = tokens.typography.caption;
                    const ImVec2 arrowExtent = font->CalcTextSizeA(arrowSize, FLT_MAX, 0.0f, ICON_FA_CHEVRON_RIGHT);
                    drawList->AddText(font, arrowSize,
                                      ImVec2(itemMax.x - arrowExtent.x - 6.0f,
                                             itemMin.y + (itemHeight - arrowExtent.y) * 0.5f),
                                      WithAlpha(tokens.text, 0.60f).ToImU32(), ICON_FA_CHEVRON_RIGHT);
                }

                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    clickedInside = true;
                    if (!hasChildren)
                    {
                        if (selectedValue != nullptr)
                        {
                            *selectedValue = item.value;
                        }
                        selected = true;
                    }
                }
            }

            // 悬停到含子项的条目：在右侧展开下一级
            if (hoveredIndex >= 0)
            {
                const GuiMenuItem& hoveredItem = levelItems[hoveredIndex];
                if (hoveredItem.children != nullptr && hoveredItem.childCount > 0)
                {
                    nextItems = hoveredItem.children;
                    nextCount = hoveredItem.childCount;
                    levelPos = ImVec2(panelMax.x - 6.0f,
                                      panelMin.y + panelPadding + itemHeight * static_cast<float>(hoveredIndex));
                }
            }

            levelItems = nextItems;
            levelCount = nextCount;
        }

        // 点击菜单外关闭；选中后同样关闭
        if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !anyHovered) || selected)
        {
            s_treeOpenLabel.clear();
        }
        (void)clickedInside;

        return selected;
    }
}
