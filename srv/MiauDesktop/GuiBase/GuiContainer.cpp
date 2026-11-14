// GuiContainer.cpp
// 容器与布局控件的实现（声明见 GuiBase.h）
// 移植自 EUI-NEO components/navbar.h / sidebar.h / panel.h / layout_debug.h
// EUI-NEO 是保留模式 DSL：此处按同样的视觉层级（栏底色、导航项高亮、抽屉滑入、
// 面板软阴影、调试覆盖层描边）把结构平移到 ImGui 立即模式——
// 交互命中交给 InvisibleButton，外观全部由 ImDrawList 自绘。

#include "GUIBase.h"

#include <cstdio>
#include <map>

namespace GuiBase
{
    namespace
    {
        // =========================================================================
        // 状态存储与常量
        // =========================================================================

        // hover 高亮的指数平滑速度
        constexpr float kSmoothSpeed = 18.0f;
        // 面板容器嵌套上限（固定长度静态栈，零堆分配）
        constexpr int kPanelMaxDepth = 8;
        // 抽屉滑入阴影：sidebar.h:79 的模糊 34 / 水平偏移 -10
        constexpr float kSidebarShadowBlur = 34.0f;
        constexpr float kSidebarShadowOffsetX = -10.0f;

        // 与帧率无关的指数平滑
        float SmoothTowards(float current, float target, float speed)
        {
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
            const float amount = 1.0f - std::exp(-speed * dt);
            return current + (target - current) * amount;
        }

        // 按像素字号居中绘制单行文本（图标与文字共用）
        void DrawCenteredText(ImDrawList* drawList, ImFont* font, float fontSize, const char* text,
                              ImVec2 min, ImVec2 max, ImU32 color)
        {
            if (drawList == nullptr || font == nullptr || text == nullptr || text[0] == '\0')
            {
                return;
            }
            const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            const ImVec2 textPos(min.x + (max.x - min.x - textSize.x) * 0.5f,
                                 min.y + (max.y - min.y - textSize.y) * 0.5f);
            drawList->AddText(font, fontSize, textPos, color, text);
        }

        // 导航栏底色：navbar.h:163-167（暗色在 background 上压暗 0.24，亮色直接用 surface）
        GuiColor NavbarColor(const GuiThemeTokens& tokens)
        {
            return tokens.isDark
                       ? MixColor(tokens.background, GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.24f)
                       : tokens.surface;
        }

        // 面板容器嵌套状态
        struct PanelState
        {
            ImVec2 pos;
            ImVec2 size;
            bool pushedClip = false;
        };

        PanelState g_panelStack[kPanelMaxDepth];
        int g_panelDepth = 0;
        int g_panelOrphanEnds = 0; // Begin 被提前拒绝时的 End 计数（上限保护）

        // 抽屉滑入进度（每个抽屉一份动画状态）
        std::map<ImGuiID, GuiAnimatedValue<float>> g_sidebarProgress;
    }

    // =========================================================================
    // 导航栏
    // 竖向结构（navbar.h:58-146）：顶部品牌区（图标 / 标题 / 副标题）→ 纵向导航项列表
    // → 可选 footer 按钮。选中项为 primary 底 + 高亮边框 + 左侧指示条，
    // 未选中项 hover 时底色向亮侧平滑过渡。
    // =========================================================================
    bool GuiNavbar(const char* label, const GuiNavbarItem* items, int itemCount,
                   int* selectedIndex, const GuiNavbarOptions& options)
    {
        if (selectedIndex == nullptr || itemCount < 0)
        {
            return false;
        }

        const GuiThemeTokens tokens = options.dark ? Theme::Dark() : Theme::Light();
        const float availWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float width = (options.width > 0.0f) ? options.width : std::max(80.0f, availWidth);
        const float height = (options.height > 0.0f) ? options.height : 320.0f;
        if (width <= 0.0f || height <= 0.0f)
        {
            return false;
        }

        const bool compact = options.compact;
        const float compactWidth = tokens.control.navigation + tokens.spacing.micro;
        const float navWidth = compact
                                   ? compactWidth
                                   : std::max(compactWidth, width - tokens.spacing.header * 2.0f);
        const float sideInset = compact ? std::max(0.0f, (width - navWidth) * 0.5f)
                                        : tokens.spacing.header;

        const float titleHeight = compact ? 0.0f : tokens.control.segmented;
        const bool hasSubtitle = (!compact) && options.subtitle != nullptr && options.subtitle[0] != '\0';
        const float subtitleHeight = hasSubtitle ? tokens.control.switchHeight : 0.0f;
        const float brandGap = tokens.typography.label;
        const float navHeight = tokens.control.navigation;
        const float navGap = tokens.typography.label;
        const float titleGap = compact ? 0.0f : brandGap;
        const float subtitleGap = hasSubtitle ? brandGap : 0.0f;

        const float iconTop = tokens.spacing.header;
        const float iconHeight = tokens.control.menuItem;
        const float titleTop = iconTop + iconHeight + titleGap;
        const float subtitleTop = titleTop + titleHeight + subtitleGap;
        const float navTop = subtitleTop + subtitleHeight + brandGap;

        ImGui::PushID(label != nullptr ? label : "##gui_navbar");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 maxPos(pos.x + width, pos.y + height);

        // === 栏背景 + 边框（radius.section）===
        const float rounding = tokens.radius.section;
        drawList->AddRectFilled(pos, maxPos, NavbarColor(tokens).ToImU32(), rounding);
        drawList->AddRect(pos, maxPos, WithAlpha(tokens.border, 0.62f).ToImU32(),
                          rounding, ImDrawFlags_RoundCornersAll, 1.0f);

        ImFont* font = ImGui::GetFont();

        // === 品牌区 ===
        if (options.brandIcon != nullptr && options.brandIcon[0] != '\0')
        {
            DrawCenteredText(drawList, font, tokens.typography.headline, options.brandIcon,
                             ImVec2(pos.x + sideInset, pos.y + iconTop),
                             ImVec2(pos.x + sideInset + navWidth, pos.y + iconTop + iconHeight),
                             tokens.primary.ToImU32());
        }
        if (!compact && options.brandTitle != nullptr && options.brandTitle[0] != '\0')
        {
            DrawCenteredText(drawList, font, tokens.typography.displayCompact, options.brandTitle,
                             ImVec2(pos.x + sideInset, pos.y + titleTop),
                             ImVec2(pos.x + sideInset + navWidth, pos.y + titleTop + titleHeight),
                             WithAlpha(tokens.text, 0.98f).ToImU32());
        }
        if (hasSubtitle)
        {
            DrawCenteredText(drawList, font, tokens.typography.label, options.subtitle,
                             ImVec2(pos.x + sideInset, pos.y + subtitleTop),
                             ImVec2(pos.x + sideInset + navWidth, pos.y + subtitleTop + subtitleHeight),
                             WithAlpha(tokens.text, 0.72f).ToImU32());
        }

        // === 导航项 ===
        // 匹配语义：任一项 value 非 0 时按 value 匹配，否则退化为按下标匹配
        bool useValueMatch = false;
        for (int index = 0; index < itemCount; ++index)
        {
            if (items[index].value != 0)
            {
                useValueMatch = true;
                break;
            }
        }

        int current = *selectedIndex;
        bool changed = false;

        for (int index = 0; index < itemCount; ++index)
        {
            const GuiNavbarItem& item = items[index];
            const float itemTop = pos.y + navTop + static_cast<float>(index) * (navHeight + navGap);
            const ImVec2 itemMin(pos.x + sideInset, itemTop);
            const ImVec2 itemMax(itemMin.x + navWidth, itemTop + navHeight);
            if (itemMax.y > maxPos.y)
            {
                break; // 栏高不足时截断，避免画到栏外
            }

            const bool active = useValueMatch ? (item.value == current) : (index == current);

            ImGui::PushID(index);
            ImGui::SetCursorScreenPos(itemMin);
            const bool clicked = ImGui::InvisibleButton("##nav_item", ImVec2(navWidth, navHeight));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            // hover 平滑：跨帧状态走 ImGuiStorage
            float* hoverAnim = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##nav_hover"), 0.0f);
            *hoverAnim = SmoothTowards(*hoverAnim, hovered ? 1.0f : 0.0f, kSmoothSpeed);

            // 底色：active → primary，其它 → surface，hover 向亮侧混合
            GuiColor background = active ? tokens.primary : tokens.surface;
            if (!active && *hoverAnim > 0.001f)
            {
                background = MixColor(background, tokens.surfaceHover, 0.75f * *hoverAnim);
            }

            const float itemRounding = tokens.radius.card;
            drawList->AddRectFilled(itemMin, itemMax, background.ToImU32(), itemRounding);
            drawList->AddRect(itemMin, itemMax,
                              WithAlpha(active ? tokens.primary : tokens.border,
                                        active ? 0.58f : 0.70f).ToImU32(),
                              itemRounding, ImDrawFlags_RoundCornersAll, 1.0f);

            // 选中指示条：左侧 4px primary 竖条
            if (active)
            {
                drawList->AddRectFilled(ImVec2(itemMin.x + 2.0f, itemMin.y + 10.0f),
                                        ImVec2(itemMin.x + 2.0f + tokens.spacing.tiny, itemMax.y - 10.0f),
                                        tokens.primary.ToImU32(), tokens.radius.micro);
            }

            // 图标 + 文字：active 时用近白色，未选中用 primary 图标 + 常规文字
            const GuiColor iconColor = active ? GuiColor(0.94f, 0.97f, 1.0f, 1.0f) : tokens.primary;
            const GuiColor labelColor = active ? GuiColor(0.94f, 0.97f, 1.0f, 1.0f)
                                               : WithAlpha(tokens.text, 0.92f);
            const float iconSize = compact ? tokens.spacing.large : tokens.spacing.section;

            if (compact)
            {
                // 紧凑模式：只有图标，居中
                DrawCenteredText(drawList, font, iconSize,
                                 (item.icon != nullptr) ? item.icon : item.label,
                                 itemMin, itemMax, iconColor.ToImU32());
            }
            else
            {
                const float iconBoxWidth = iconSize + tokens.spacing.compact;
                if (item.icon != nullptr && item.icon[0] != '\0')
                {
                    DrawCenteredText(drawList, font, iconSize, item.icon,
                                     ImVec2(itemMin.x + tokens.spacing.control, itemMin.y),
                                     ImVec2(itemMin.x + tokens.spacing.control + iconBoxWidth, itemMax.y),
                                     iconColor.ToImU32());
                }

                if (item.label != nullptr && item.label[0] != '\0' && font != nullptr)
                {
                    const float labelSize = tokens.typography.input;
                    const ImVec2 labelText = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, item.label);
                    const float labelX = itemMin.x + tokens.spacing.control + iconBoxWidth;
                    drawList->AddText(font, labelSize,
                                      ImVec2(labelX, itemMin.y + (navHeight - labelText.y) * 0.5f),
                                      labelColor.ToImU32(), item.label);
                }
            }

            if (clicked && !active)
            {
                current = useValueMatch ? item.value : index;
                changed = true;
            }
        }

        // === footer 按钮（navbar.h:222-234：贴栏底，y = 高度 - overlay - menuItem）===
        bool footerClicked = false;
        if (options.footerText != nullptr && options.footerText[0] != '\0')
        {
            const float footerTop = std::max(navTop, height - tokens.spacing.overlay - tokens.control.menuItem);
            const ImVec2 footerMin(pos.x + sideInset, pos.y + footerTop);
            const ImVec2 footerMax(footerMin.x + navWidth, footerMin.y + navHeight);
            if (footerMax.y <= maxPos.y)
            {
                ImGui::PushID("##nav_footer");
                ImGui::SetCursorScreenPos(footerMin);
                footerClicked = ImGui::InvisibleButton("##nav_footer_hit", ImVec2(navWidth, navHeight));
                const bool footerHovered = ImGui::IsItemHovered();
                ImGui::PopID();

                GuiColor footerBg = MixColor(tokens.surface, tokens.surfaceHover, footerHovered ? 0.75f : 0.0f);
                drawList->AddRectFilled(footerMin, footerMax, footerBg.ToImU32(), tokens.radius.card);
                drawList->AddRect(footerMin, footerMax, WithAlpha(tokens.border, 0.70f).ToImU32(),
                                  tokens.radius.card, ImDrawFlags_RoundCornersAll, 1.0f);

                const float iconSize = compact ? tokens.spacing.large : tokens.spacing.section;
                const float iconBoxWidth = iconSize + tokens.spacing.compact;
                if (options.footerIcon != nullptr && options.footerIcon[0] != '\0')
                {
                    DrawCenteredText(drawList, font, iconSize, options.footerIcon,
                                     ImVec2(footerMin.x + tokens.spacing.control, footerMin.y),
                                     ImVec2(footerMin.x + tokens.spacing.control + iconBoxWidth, footerMax.y),
                                     tokens.primary.ToImU32());
                }
                if (!compact && font != nullptr)
                {
                    const float labelSize = tokens.typography.input;
                    const ImVec2 labelText = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, options.footerText);
                    drawList->AddText(font, labelSize,
                                      ImVec2(footerMin.x + tokens.spacing.control + iconBoxWidth,
                                             footerMin.y + (navHeight - labelText.y) * 0.5f),
                                      WithAlpha(tokens.text, 0.92f).ToImU32(), options.footerText);
                }
            }
        }
        if (options.footerClicked != nullptr)
        {
            *options.footerClicked = footerClicked;
        }

        // 推进光标到栏底部
        ImGui::SetCursorScreenPos(ImVec2(pos.x, maxPos.y));
        ImGui::PopID();

        if (changed)
        {
            *selectedIndex = current;
        }
        return changed;
    }

    // =========================================================================
    // 右侧抽屉
    // 全屏 scrim（点击关闭）+ 面板按 duration 从右侧滑入，
    // 面板含 eyebrow 小标题 / 大标题 / 右上角关闭按钮，内容由回调填充。
    // =========================================================================
    bool GuiSidebar(const char* label, bool* open, const GuiSidebarOptions& options,
                    const std::function<void()>& content)
    {
        if (open == nullptr)
        {
            return false;
        }

        const GuiThemeTokens tokens = options.dark ? Theme::Dark() : Theme::Light();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float width = (options.width > 0.0f) ? options.width : std::max(120.0f, avail.x);
        const float height = (options.height > 0.0f) ? options.height : std::max(120.0f, avail.y);
        if (width <= 0.0f || height <= 0.0f)
        {
            return false;
        }

        ImGui::PushID(label != nullptr ? label : "##gui_sidebar");
        const ImGuiID sidebarId = ImGui::GetID("##gui_sidebar_anim");

        // 滑入进度：GuiAnimatedValue + duration（首次调用直接吸附到目标）
        GuiAnimatedValue<float>& progressAnim = g_sidebarProgress[sidebarId];
        progressAnim.SetTarget(*open ? 1.0f : 0.0f,
                               GuiTransition::Make(std::max(0.01f, options.duration), GuiEase::OutCubic));
        progressAnim.Tick(ImGui::GetIO().DeltaTime);
        const float progress = std::clamp(progressAnim.Value(), 0.0f, 1.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 maxPos(pos.x + width, pos.y + height);

        // 面板宽度：sidebar.h:46-47 的 clamp（不小于 compact*10，且不超过视口宽度）
        const float panelWidth = std::min(options.drawerWidth,
                                          std::max(tokens.control.compact * 10.0f,
                                                   width - tokens.control.switchHeight));
        const float panelHeight = height;

        bool openChanged = false;

        // === scrim：点击关闭 ===
        if (progress > 0.001f)
        {
            const int scrimAlpha = static_cast<int>(std::clamp(0.35f * progress, 0.0f, 1.0f) * 255.0f);
            drawList->AddRectFilled(pos, maxPos, IM_COL32(0, 0, 0, scrimAlpha));

            // 遮罩命中区只覆盖面板左侧（面板自身的按钮优先响应）
            const float panelLeft = maxPos.x - panelWidth * progress;
            if (panelLeft > pos.x)
            {
                ImGui::SetCursorScreenPos(pos);
                ImGui::InvisibleButton("##scrim", ImVec2(panelLeft - pos.x, height));
                if (ImGui::IsItemActivated())
                {
                    *open = false;
                    openChanged = true;
                }
            }
        }

        // === 面板 ===
        if (progress > 0.001f)
        {
            const float panelLeft = maxPos.x - panelWidth + (1.0f - progress) * panelWidth;
            const ImVec2 panelMin(panelLeft, pos.y);
            const ImVec2 panelMax(panelLeft + panelWidth, pos.y + panelHeight);

            // 背景 + 软阴影（复用 Card 渲染；阴影按 sidebar.h:79 的 blur 34 / offsetX -10）
            GuiCardStyle cardStyle(tokens);
            cardStyle.radius = tokens.radius.section;
            cardStyle.padding = tokens.spacing.large;
            cardStyle.opacity = progress;
            cardStyle.shadow.blur = kSidebarShadowBlur;
            cardStyle.shadow.offsetX = kSidebarShadowOffsetX;
            cardStyle.shadow.offsetY = 0.0f;
            cardStyle.shadow.color = tokens.isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.34f)
                                                   : GuiColor(0.10f, 0.14f, 0.22f, 0.16f);
            DrawGuiCardBackground(drawList, panelMin, ImVec2(panelWidth, panelHeight), cardStyle);

            // 头部：eyebrow + 标题 + 关闭按钮（sidebar.h:170-222）
            const float headerHeight = tokens.spacing.overlay * 2.0f + tokens.spacing.compact;
            const float closeSize = tokens.control.segmented + tokens.spacing.micro;
            const float closeX = std::max(panelMin.x, panelMax.x - closeSize - tokens.spacing.large);
            ImFont* font = ImGui::GetFont();

            if (options.eyebrow != nullptr && options.eyebrow[0] != '\0' && font != nullptr)
            {
                drawList->AddText(font, tokens.typography.option,
                                  ImVec2(panelMin.x + tokens.spacing.large,
                                         panelMin.y + tokens.spacing.large),
                                  WithAlpha(tokens.primary, 0.90f * progress).ToImU32(),
                                  options.eyebrow);
            }
            if (options.title != nullptr && options.title[0] != '\0' && font != nullptr)
            {
                drawList->AddText(font, tokens.typography.hero,
                                  ImVec2(panelMin.x + tokens.spacing.large,
                                         panelMin.y + tokens.spacing.large + tokens.control.control),
                                  WithAlpha(tokens.text, 0.98f * progress).ToImU32(),
                                  options.title);
            }

            // 关闭按钮
            if (*open)
            {
                const ImVec2 closeMin(closeX, panelMin.y + tokens.spacing.large);
                const ImVec2 closeMax(closeMin.x + closeSize, closeMin.y + closeSize);
                ImGui::SetCursorScreenPos(closeMin);
                ImGui::InvisibleButton("##close", ImVec2(closeSize, closeSize));
                const bool closeHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemActivated())
                {
                    *open = false;
                    openChanged = true;
                }
                if (closeHovered)
                {
                    drawList->AddRectFilled(closeMin, closeMax, tokens.surfaceActive.ToImU32(),
                                            tokens.radius.popup);
                }
                DrawCenteredText(drawList, font, tokens.typography.heading, ICON_FA_XMARK,
                                 closeMin, closeMax, WithAlpha(tokens.text, progress).ToImU32());
            }

            // 内容区
            if (content)
            {
                const ImVec2 contentMin(panelMin.x + tokens.spacing.large, panelMin.y + headerHeight);
                const ImVec2 contentMax(panelMax.x - tokens.spacing.large, panelMax.y - tokens.spacing.large);
                if (contentMax.x > contentMin.x && contentMax.y > contentMin.y)
                {
                    const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
                    ImGui::PushClipRect(contentMin, contentMax, true);
                    ImGui::SetCursorScreenPos(contentMin);
                    content();
                    ImGui::PopClipRect();
                    ImGui::SetCursorScreenPos(savedCursor);
                }
            }
        }

        // 推进光标到视口底部
        ImGui::SetCursorScreenPos(ImVec2(pos.x, maxPos.y));
        ImGui::PopID();

        return openChanged;
    }

    // =========================================================================
    // 面板容器
    // Begin 绘制 Card 背景（阴影 → 填充 → 边框）并把光标与裁剪推进到内容区，
    // End 恢复裁剪并把光标推进到面板下方。支持嵌套（8 层静态栈）。
    // =========================================================================
    bool BeginGuiPanel(const char* label, const ImVec2& size, const GuiCardStyle& style)
    {
        if (size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImGui::PushID(label != nullptr ? label : "##gui_panel");

        // 背景：阴影 → 填充 → 边框
        DrawGuiCardBackground(drawList, pos, size, style);

        // 内容区：按 padding 内缩，裁剪到内容区
        const float padding = std::max(0.0f, style.padding);
        const ImVec2 contentMin(pos.x + padding, pos.y + padding);
        const ImVec2 contentMax(pos.x + size.x - padding, pos.y + size.y - padding);
        const bool hasContent = (contentMax.x > contentMin.x && contentMax.y > contentMin.y);
        if (hasContent)
        {
            ImGui::PushClipRect(contentMin, contentMax, true);
            ImGui::SetCursorScreenPos(contentMin);
        }

        if (g_panelDepth < kPanelMaxDepth)
        {
            PanelState& state = g_panelStack[g_panelDepth];
            state.pos = pos;
            state.size = size;
            state.pushedClip = hasContent;
            ++g_panelDepth;
        }
        else
        {
            ++g_panelOrphanEnds;
        }

        return hasContent;
    }

    void EndGuiPanel()
    {
        if (g_panelDepth <= 0)
        {
            if (g_panelOrphanEnds > 0 && g_panelOrphanEnds <= 64)
            {
                --g_panelOrphanEnds;
                ImGui::PopID();
            }
            return;
        }

        PanelState state = g_panelStack[g_panelDepth - 1];
        --g_panelDepth;

        if (state.pushedClip)
        {
            ImGui::PopClipRect();
        }
        // 光标推进到面板下方，后续控件不会与面板重叠
        ImGui::SetCursorScreenPos(ImVec2(state.pos.x, state.pos.y + state.size.y));
        ImGui::PopID();
    }

    // =========================================================================
    // 布局调试样式
    // 默认值取自 layout_debug.h:23-32 的四组调试色与描边/标签尺寸
    // =========================================================================
    GuiLayoutDebugStyle::GuiLayoutDebugStyle()
    {
        frame = GuiColor(0.96f, 0.32f, 0.38f, 0.64f);
        padding = GuiColor(0.95f, 0.70f, 0.20f, 0.34f);
        content = GuiColor(0.28f, 0.58f, 0.98f, 0.56f);
        label = GuiColor(0.92f, 0.95f, 1.0f, 0.88f);
    }

    GuiLayoutDebugStyle::GuiLayoutDebugStyle(const GuiThemeTokens& tokens)
        : GuiLayoutDebugStyle()
    {
        stroke = tokens.spacing.hairline;
        labelFontSize = tokens.typography.caption;
        labelLineHeight = tokens.typography.caption + tokens.typography.lineGap;
        labelX = tokens.spacing.compact;
        labelY = tokens.spacing.small;
        labelHeight = tokens.typography.control;
    }

    // =========================================================================
    // 布局调试覆盖层
    // 逐条对应 layout_debug.h:42-78：外框四边 → inset 内边距填充 → 内容区内描边
    // → 左上角尺寸标签；inset 需 clamp 到 min(宽, 高) * 0.5
    // =========================================================================
    void GuiLayoutDebugOverlay(const char* label, const ImVec2& size, float inset,
                               const char* text, const GuiLayoutDebugStyle& style)
    {
        const float safeWidth = std::max(0.0f, size.x);
        const float safeHeight = std::max(0.0f, size.y);
        if (safeWidth <= 0.0f || safeHeight <= 0.0f)
        {
            return;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float safeInset = std::min(std::max(0.0f, inset), std::min(safeWidth, safeHeight) * 0.5f);
        const float contentWidth = std::max(0.0f, safeWidth - safeInset * 2.0f);
        const float contentHeight = std::max(0.0f, safeHeight - safeInset * 2.0f);
        const float stroke = std::max(1.0f, style.stroke);

        ImGui::PushID(label != nullptr ? label : "##gui_layout_debug");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 frameColor = style.frame.ToImU32();
        const ImU32 contentColor = style.content.ToImU32();

        // 外框：上下左右四条细线
        drawList->AddRectFilled(pos, ImVec2(pos.x + safeWidth, pos.y + stroke), frameColor);
        drawList->AddRectFilled(pos, ImVec2(pos.x + stroke, pos.y + safeHeight), frameColor);
        drawList->AddRectFilled(ImVec2(pos.x + safeWidth - stroke, pos.y),
                                ImVec2(pos.x + safeWidth, pos.y + safeHeight), frameColor);
        drawList->AddRectFilled(ImVec2(pos.x, pos.y + safeHeight - stroke),
                                ImVec2(pos.x + safeWidth, pos.y + safeHeight), frameColor);

        // 内边距填充（alpha 乘 0.07）+ 内容区描边
        if (safeInset > 0.0f)
        {
            const ImU32 paddingColor = WithAlpha(style.padding, style.padding.a * 0.07f).ToImU32();
            drawList->AddRectFilled(pos, ImVec2(pos.x + safeWidth, pos.y + safeInset), paddingColor);
            drawList->AddRectFilled(pos, ImVec2(pos.x + safeInset, pos.y + safeHeight), paddingColor);
            drawList->AddRectFilled(ImVec2(pos.x + safeWidth - safeInset, pos.y),
                                    ImVec2(pos.x + safeWidth, pos.y + safeHeight), paddingColor);
            drawList->AddRectFilled(ImVec2(pos.x, pos.y + safeHeight - safeInset),
                                    ImVec2(pos.x + safeWidth, pos.y + safeHeight), paddingColor);

            const ImVec2 innerMin(pos.x + safeInset, pos.y + safeInset);
            drawList->AddRectFilled(innerMin, ImVec2(innerMin.x + contentWidth, innerMin.y + stroke), contentColor);
            drawList->AddRectFilled(innerMin, ImVec2(innerMin.x + stroke, innerMin.y + contentHeight), contentColor);
            drawList->AddRectFilled(ImVec2(innerMin.x + contentWidth - stroke, innerMin.y),
                                    ImVec2(innerMin.x + contentWidth, innerMin.y + contentHeight), contentColor);
            drawList->AddRectFilled(ImVec2(innerMin.x, innerMin.y + contentHeight - stroke),
                                    ImVec2(innerMin.x + contentWidth, innerMin.y + contentHeight), contentColor);
        }

        // 标签：未给文本时自动显示尺寸
        ImFont* font = ImGui::GetFont();
        if (font != nullptr)
        {
            char sizeBuffer[64];
            const char* labelText = text;
            if (labelText == nullptr || labelText[0] == '\0')
            {
                snprintf(sizeBuffer, sizeof(sizeBuffer), "%.0f x %.0f", safeWidth, safeHeight);
                labelText = sizeBuffer;
            }
            drawList->AddText(font, style.labelFontSize,
                              ImVec2(pos.x + style.labelX, pos.y + style.labelY),
                              style.label.ToImU32(), labelText);
        }

        ImGui::Dummy(ImVec2(safeWidth, safeHeight));
        ImGui::PopID();
    }
}
