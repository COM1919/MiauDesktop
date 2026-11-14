// GuiScroll.cpp
// 滚动与虚拟化控件的实现（声明见 GuiBase.h）
// 移植自 EUI-NEO components/scroll.h / scrollview.h / virtuallist.h / virtualmasonry.h
// EUI-NEO 是保留模式 DSL：此处把滚动条胶囊、滑块三态色、可见区间算法平移到 ImGui
// 立即模式——交互命中交给 InvisibleButton，外观全部由 ImDrawList 自绘；
// 跨帧状态（hover 动画、滚动条抓取偏移）走 ImGuiStorage，绘制路径零堆分配。

#include "GUIBase.h"

#include <cstdio>
#include <map>
#include <vector>

namespace GuiBase
{
    namespace
    {
        // =========================================================================
        // 尺寸与状态常量
        // =========================================================================

        // 滑块最小像素长度：移植自 scroll.h:67-68 的 min(高度, metrics.control.switchHeight)
        constexpr float kThumbMinPixels = 24.0f;
        // 滚动条与视口内容之间的间隙
        constexpr float kBarGap = 4.0f;
        // hover / 三态色的指数平滑速度（越大收敛越快）
        constexpr float kStateSmoothSpeed = 18.0f;
        // 滚轮单格步进：scrollview.h:83 的 metrics.spacing.overlay
        constexpr float kWheelStep = 48.0f;
        // 滚动视图嵌套上限：固定长度静态栈，避免每帧堆分配
        constexpr int kScrollViewMaxDepth = 8;
        // 布局缓存失效判定的浮点容差
        constexpr float kLayoutEpsilon = 0.01f;

        // ImGuiStorage key 盐值：由控件 ID 异或派生，互不冲突
        constexpr ImGuiID kSaltBarGrab = 0x165667B1u;
        constexpr ImGuiID kSaltBarHover = 0x85EBCA6Bu;
        constexpr ImGuiID kSaltRowHover = 0x27D4EB2Fu;
        constexpr ImGuiID kSaltContentHeight = 0x9E3779B9u;

        // =========================================================================
        // 通用小工具
        // =========================================================================

        // 与帧率无关的指数平滑（dt 上限避免掉帧时跳变）
        float SmoothTowards(float current, float target, float speed)
        {
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
            const float amount = 1.0f - std::exp(-speed * dt);
            return current + (target - current) * amount;
        }

        // 胶囊圆角：取横截面一半，且不超过长度的四分之一，避免短滑块圆角溢出
        float CapsuleRounding(float crossSize, float length)
        {
            const float limit = std::max(0.0f, std::min(crossSize, length) * 0.5f);
            return std::min(crossSize * 0.5f, limit);
        }

        // =========================================================================
        // 滚动条核心绘制（绝对定位版本）
        // 视觉：圆角胶囊轨道 + 内部圆角胶囊滑块（normal / hover / pressed 三态色）
        // 交互：按在滑块上 → 拖拽跟随（记录抓取偏移）；按在轨道空白 → 跳转并对齐滑块中心
        // 返回 true 表示本帧 offset 发生了变化；不改变调用方的光标位置
        // =========================================================================
        bool DrawScrollBarAt(ImGuiID barId, float* offset, float viewportSize, float contentSize,
                             const ImVec2& barMin, const ImVec2& barMax, bool horizontal,
                             const GuiScrollStyle& style, const GuiThemeTokens& tokens,
                             float wheelStep)
        {
            if (offset == nullptr || viewportSize <= 0.0f || contentSize <= viewportSize)
            {
                return false;
            }

            const float barLength = horizontal ? (barMax.x - barMin.x) : (barMax.y - barMin.y);
            const float crossSize = horizontal ? (barMax.y - barMin.y) : (barMax.x - barMin.x);
            if (barLength <= 0.0f || crossSize <= 0.0f)
            {
                return false;
            }

            const float maxOffset = contentSize - viewportSize;
            float current = std::clamp(*offset, 0.0f, maxOffset);

            // 滑块长度按视口占比缩放，并保留最小可抓取长度
            const float thumbLength = std::clamp(barLength * (viewportSize / contentSize),
                                                 std::min(barLength, kThumbMinPixels), barLength);
            const float travel = std::max(0.0f, barLength - thumbLength);

            ImGuiStorage* storage = ImGui::GetStateStorage();
            float* grabOffset = storage->GetFloatRef(barId ^ kSaltBarGrab, 0.0f);
            float* hoverAnim = storage->GetFloatRef(barId ^ kSaltBarHover, 0.0f);

            // 命中区域：整条滚动条（保存并恢复光标，保证不干扰调用方布局）
            const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
            ImGui::PushOverrideID(barId);
            ImGui::SetCursorScreenPos(barMin);
            ImGui::InvisibleButton("##gui_scrollbar_hit",
                                   ImVec2(barMax.x - barMin.x, barMax.y - barMin.y));
            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();
            const bool activated = ImGui::IsItemActivated();
            ImGui::PopID();
            ImGui::SetCursorScreenPos(savedCursor);

            *hoverAnim = SmoothTowards(*hoverAnim, (hovered || active) ? 1.0f : 0.0f, kStateSmoothSpeed);

            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const float pointer = horizontal ? (mouse.x - barMin.x) : (mouse.y - barMin.y);
            const float thumbPos = (maxOffset > 0.0f) ? (current / maxOffset) * travel : 0.0f;

            bool changed = false;

            // 按下瞬间：落在滑块上则记录抓取偏移，落在空白处则跳转并让滑块中心跟随指针
            if (activated)
            {
                if (pointer >= thumbPos && pointer <= thumbPos + thumbLength)
                {
                    *grabOffset = pointer - thumbPos;
                }
                else
                {
                    *grabOffset = thumbLength * 0.5f;
                }
            }

            // 拖拽期间（或刚跳转到空白处）offset 跟随指针
            if ((active || activated) && travel > 0.0f)
            {
                const float target = std::clamp(pointer - *grabOffset, 0.0f, travel);
                const float newOffset = (target / travel) * maxOffset;
                if (std::fabs(newOffset - current) > kLayoutEpsilon)
                {
                    current = newOffset;
                    changed = true;
                }
            }

            // 滚轮：指针悬停在滚动条上时步进
            if (hovered && wheelStep > 0.0f)
            {
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f)
                {
                    current = std::clamp(current - wheel * wheelStep, 0.0f, maxOffset);
                    changed = true;
                }
            }

            // === 绘制 ===
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float rounding = CapsuleRounding(crossSize, barLength);
            drawList->AddRectFilled(barMin, barMax, style.track.ToImU32(), rounding);

            // 滑块三态色：按下 → pressed，hover → hover，其余 → normal
            GuiColor thumbColor = style.thumb;
            if (active || activated)
            {
                thumbColor = style.thumbPressed;
            }
            else if (hovered)
            {
                thumbColor = style.thumbHover;
            }
            // 动画只作用于 hover 亮度，避免按下态被平滑拖慢
            const float hoverBoost = std::clamp(*hoverAnim, 0.0f, 1.0f) * 0.08f;
            thumbColor = MixColor(thumbColor, WithAlpha(tokens.text, thumbColor.a), hoverBoost);

            const float finalThumbPos = (maxOffset > 0.0f) ? (current / maxOffset) * travel : 0.0f;
            const ImVec2 thumbMin = horizontal ? ImVec2(barMin.x + finalThumbPos, barMin.y)
                                               : ImVec2(barMin.x, barMin.y + finalThumbPos);
            const ImVec2 thumbMax = horizontal ? ImVec2(thumbMin.x + thumbLength, barMax.y)
                                               : ImVec2(barMax.x, thumbMin.y + thumbLength);
            if (thumbMax.x > thumbMin.x && thumbMax.y > thumbMin.y)
            {
                drawList->AddRectFilled(thumbMin, thumbMax, thumbColor.ToImU32(),
                                        CapsuleRounding(crossSize, thumbLength));
            }

            if (changed)
            {
                *offset = current;
            }
            return changed;
        }

        // =========================================================================
        // 滚动视图嵌套状态（固定长度静态栈，零堆分配）
        // =========================================================================
        struct ScrollViewState
        {
            ImGuiID id = 0;
            float* offset = nullptr;
            ImVec2 pos;                 // 视口左上角（屏幕坐标）
            ImVec2 size;                // 视口尺寸
            float contentTopLocal = 0.0f; // 内容起点（子窗口局部坐标 Y）
            float barWidth = 0.0f;      // 已扣除的滚动条宽度
            GuiScrollStyle style;
            bool pushed = false;        // 是否压入了样式变量（需要成对 Pop）
        };

        ScrollViewState g_scrollViewStack[kScrollViewMaxDepth];
        int g_scrollViewDepth = 0;
        int g_scrollViewOrphanEnds = 0; // Begin 未压栈时的 End 计数（上限保护）

        // =========================================================================
        // 虚拟瀑布流布局缓存（按控件 ID 保存，仅在参数变化时重算）
        // =========================================================================
        struct MasonryCell
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            int column = 0;
        };

        struct MasonryLayout
        {
            std::vector<MasonryCell> cells;              // 按 index 顺序
            std::vector<std::vector<int>> columnIndices; // 每列内 index 顺序（y 单调递增）
            float totalHeight = 0.0f;
            float itemWidth = 0.0f;
            float gap = -1.0f;
            int columns = 0;
            int itemCount = -1;
            ImVec2 viewMin;   // 本帧视口左上角
            ImVec2 viewSize;  // 本帧视口尺寸
            float offset = 0.0f;
        };

        std::map<ImGuiID, MasonryLayout> g_masonryLayouts;
        ImGuiID g_lastMasonryKey = 0;

        // 重建瀑布流布局：最短列优先（virtualmasonry.h:209-228）
        void BuildMasonryLayout(MasonryLayout& layout, int itemCount, int columns, float gap,
                                float contentWidth,
                                const std::function<float(int, float)>& heightOf)
        {
            layout.cells.clear();
            layout.columnIndices.clear();
            layout.totalHeight = 0.0f;
            layout.gap = gap;
            layout.columns = columns;
            layout.itemCount = itemCount;

            if (itemCount <= 0 || columns <= 0 || contentWidth <= 0.0f)
            {
                layout.itemWidth = 0.0f;
                return;
            }

            const float totalGap = gap * static_cast<float>(columns - 1);
            const float itemWidth = std::max(1.0f, (contentWidth - totalGap) / static_cast<float>(columns));
            layout.itemWidth = itemWidth;

            std::vector<float> columnHeights(static_cast<std::size_t>(columns), 0.0f);
            std::vector<int> columnCounts(static_cast<std::size_t>(columns), 0);
            layout.cells.resize(static_cast<std::size_t>(itemCount));
            layout.columnIndices.resize(static_cast<std::size_t>(columns));

            for (int index = 0; index < itemCount; ++index)
            {
                // 选当前最短的列
                int column = 0;
                for (int candidate = 1; candidate < columns; ++candidate)
                {
                    if (columnHeights[static_cast<std::size_t>(candidate)] <
                        columnHeights[static_cast<std::size_t>(column)])
                    {
                        column = candidate;
                    }
                }

                const float height = std::max(1.0f, heightOf ? heightOf(index, itemWidth) : itemWidth);
                MasonryCell& cell = layout.cells[static_cast<std::size_t>(index)];
                cell.x = static_cast<float>(column) * (itemWidth + gap);
                cell.y = columnHeights[static_cast<std::size_t>(column)];
                cell.width = itemWidth;
                cell.height = height;
                cell.column = column;
                layout.columnIndices[static_cast<std::size_t>(column)].push_back(index);

                columnHeights[static_cast<std::size_t>(column)] = cell.y + height + gap;
                ++columnCounts[static_cast<std::size_t>(column)];
            }

            for (float height : columnHeights)
            {
                layout.totalHeight = std::max(layout.totalHeight, height);
            }
            layout.totalHeight = std::max(0.0f, layout.totalHeight - gap);
        }

        // 在某一列内二分出与 [top, bottom] 相交的 cell 索引范围（该列内 y 单调递增）
        void ColumnVisibleRange(const MasonryLayout& layout, int column, float top, float bottom,
                                int& outFirst, int& outLast)
        {
            outFirst = 0;
            outLast = -1;
            if (column < 0 || column >= static_cast<int>(layout.columnIndices.size()))
            {
                return;
            }
            const std::vector<int>& indices = layout.columnIndices[static_cast<std::size_t>(column)];
            if (indices.empty())
            {
                return;
            }

            const int count = static_cast<int>(indices.size());

            // 最后一个 cell 的底边已在上方 → 无可见项
            const MasonryCell& lastCell = layout.cells[static_cast<std::size_t>(indices[static_cast<std::size_t>(count - 1)])];
            if (lastCell.y + lastCell.height < top)
            {
                return;
            }
            // 第一个 cell 的顶边已在下方 → 无可见项
            const MasonryCell& firstCell = layout.cells[static_cast<std::size_t>(indices[0])];
            if (firstCell.y > bottom)
            {
                return;
            }

            // lower_bound：第一个 y + height >= top 的位置
            int low = 0;
            int high = count - 1;
            int startSlot = count - 1;
            while (low <= high)
            {
                const int mid = low + (high - low) / 2;
                const MasonryCell& cell = layout.cells[static_cast<std::size_t>(indices[static_cast<std::size_t>(mid)])];
                if (cell.y + cell.height < top)
                {
                    low = mid + 1;
                }
                else
                {
                    startSlot = mid;
                    high = mid - 1;
                }
            }

            // upper_bound：最后一个 y <= bottom 的位置
            int endSlot = startSlot;
            low = startSlot;
            high = count - 1;
            while (low <= high)
            {
                const int mid = low + (high - low) / 2;
                const MasonryCell& cell = layout.cells[static_cast<std::size_t>(indices[static_cast<std::size_t>(mid)])];
                if (cell.y <= bottom)
                {
                    endSlot = mid;
                    low = mid + 1;
                }
                else
                {
                    high = mid - 1;
                }
            }

            outFirst = indices[static_cast<std::size_t>(startSlot)];
            outLast = indices[static_cast<std::size_t>(endSlot)];
        }
    }

    // =========================================================================
    // 滚动条样式
    // 默认值取自 scroll.h:15-21 的 ScrollStyle（暗色 / 亮色两套透明度），
    // 粗细取 theme.h 的 ControlSizeTokens.scrollbar
    // =========================================================================
    GuiScrollStyle::GuiScrollStyle()
        : GuiScrollStyle(Theme::Dark())
    {
    }

    GuiScrollStyle::GuiScrollStyle(const GuiThemeTokens& tokens)
    {
        track = WithAlpha(tokens.surfaceHover, tokens.isDark ? 0.34f : 0.46f);
        thumb = WithAlpha(tokens.text, tokens.isDark ? 0.34f : 0.28f);
        thumbHover = WithAlpha(tokens.text, tokens.isDark ? 0.46f : 0.38f);
        thumbPressed = WithAlpha(tokens.primary, 0.76f);
        radius = tokens.radius.full;
        thickness = tokens.control.scrollbar;
    }

    // =========================================================================
    // 独立滚动条
    // size 中与滚动方向垂直的一维为 0 时取 style.thickness；返回 offset 是否变化
    // =========================================================================
    bool GuiScrollBar(const char* label, float* offset, float viewportSize, float contentSize,
                      const ImVec2& size, bool horizontal, const GuiScrollStyle& style)
    {
        if (offset == nullptr || viewportSize <= 0.0f || contentSize <= viewportSize)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌

        const float barWidth = (size.x > 0.0f) ? size.x : style.thickness;
        const float barHeight = (size.y > 0.0f) ? size.y : style.thickness;
        if (barWidth <= 0.0f || barHeight <= 0.0f)
        {
            return false;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImGuiID barId = ImGui::GetID(label != nullptr ? label : "##gui_scrollbar");
        const bool changed = DrawScrollBarAt(barId, offset, viewportSize, contentSize,
                                             pos, ImVec2(pos.x + barWidth, pos.y + barHeight),
                                             horizontal, style, tokens, kWheelStep);

        // 占位推进光标（与滚动条尺寸一致，保证后续布局不被遮挡）
        ImGui::Dummy(ImVec2(barWidth, barHeight));
        return changed;
    }

    // =========================================================================
    // 滚动视图
    // Begin 用 BeginChild 提供裁剪与滚动内容区（宽度已扣除滚动条），
    // End 按内容高度绘制滚动条、处理滚轮并 clamp offset
    // =========================================================================
    bool BeginGuiScrollView(const char* label, const ImVec2& size, float* offset,
                            const GuiScrollStyle& style, ImGuiWindowFlags flags)
    {
        if (offset == nullptr || size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float barWidth = style.thickness;
        const ImGuiID viewId = ImGui::GetID(label != nullptr ? label : "##gui_scrollview");

        // 用上一帧测得的内容高度决定是否预留滚动条宽度：
        // 首帧按全宽布局，第二帧起因内容宽度稳定而不再重排（消除滚动条出现时的抖动）
        const float cachedContentHeight =
            ImGui::GetStateStorage()->GetFloat(viewId ^ kSaltContentHeight, 0.0f);
        const bool reservingBar = (cachedContentHeight > size.y + 0.5f);
        const float contentWidth = std::max(10.0f, size.x - (reservingBar ? (barWidth + kBarGap) : 0.0f));

        // 子窗口内不额外留白，滚动完全由滚动条驱动
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);

        const bool visible = ImGui::BeginChild(label != nullptr ? label : "##gui_scrollview",
                                              ImVec2(contentWidth, size.y), ImGuiChildFlags_None,
                                              flags | ImGuiWindowFlags_NoScrollbar |
                                                  ImGuiWindowFlags_NoScrollWithMouse);

        if (g_scrollViewDepth < kScrollViewMaxDepth)
        {
            ScrollViewState& state = g_scrollViewStack[g_scrollViewDepth];
            state.id = viewId;
            state.offset = offset;
            state.pos = pos;
            state.size = size;
            state.barWidth = barWidth;
            state.style = style;
            state.contentTopLocal = ImGui::GetCursorPosY();
            state.pushed = true;
            ++g_scrollViewDepth;
        }
        else
        {
            // 超出嵌套上限：退化为普通裁剪区（仍可正常 End）
            ++g_scrollViewOrphanEnds;
        }

        return visible;
    }

    void EndGuiScrollView()
    {
        if (g_scrollViewDepth <= 0)
        {
            if (g_scrollViewOrphanEnds > 0 && g_scrollViewOrphanEnds <= 64)
            {
                --g_scrollViewOrphanEnds;
                ImGui::EndChild();
                ImGui::PopStyleVar(3);
            }
            return;
        }

        ScrollViewState& state = g_scrollViewStack[g_scrollViewDepth - 1];
        ScrollViewState local = state;
        --g_scrollViewDepth;

        // 内容高度：子窗口内已布局内容的底部 + 底部内边距
        const float contentBottom = ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y;
        const float contentHeight = std::max(0.0f, contentBottom - local.contentTopLocal);

        ImGui::EndChild();
        if (local.pushed)
        {
            ImGui::PopStyleVar(3);
        }

        // 写回内容高度：下一帧 Begin 据此决定是否预留滚动条宽度
        ImGui::GetStateStorage()->SetFloat(local.id ^ kSaltContentHeight, contentHeight);

        if (local.offset == nullptr)
        {
            return;
        }

        GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        const float viewportHeight = local.size.y;
        const float maxOffset = std::max(0.0f, contentHeight - viewportHeight);
        float current = std::clamp(*local.offset, 0.0f, maxOffset);

        // 滚轮：指针在视口内时步进（滚轮由本控件独占，子窗口已禁用滚动）
        const ImVec2 viewMax(local.pos.x + local.size.x, local.pos.y + local.size.y);
        if (ImGui::IsMouseHoveringRect(local.pos, viewMax, false) && maxOffset > 0.0f)
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                current = std::clamp(current - wheel * kWheelStep, 0.0f, maxOffset);
            }
        }

        // 滚动条：贴在视口右侧，绝对定位绘制（不推进父窗口光标）
        if (maxOffset > 0.0f)
        {
            const ImVec2 barMin(local.pos.x + local.size.x - local.barWidth, local.pos.y);
            DrawScrollBarAt(local.id ^ 0x5BF03635u, &current, viewportHeight, contentHeight,
                            barMin, viewMax, false, local.style, tokens, kWheelStep);
        }

        *local.offset = current;
    }

    // =========================================================================
    // 虚拟列表（固定行高）
    // 只对可见区间（含 overscan 行）绘制行；提交滚轮与滚动条交互后返回区间
    // =========================================================================
    GuiVirtualRange GuiVirtualList(const char* label, const ImVec2& size, int itemCount,
                                   float rowHeight, float* offset, float overscanRows,
                                   const GuiScrollStyle& style)
    {
        GuiVirtualRange range;
        if (offset == nullptr || size.x <= 0.0f || size.y <= 0.0f)
        {
            return range;
        }

        GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌

        const int safeItemCount = std::max(0, itemCount);
        const float row = std::max(1.0f, rowHeight);
        const float totalHeight = static_cast<float>(safeItemCount) * row;
        const float maxOffset = std::max(0.0f, totalHeight - size.y);
        float current = std::clamp(*offset, 0.0f, maxOffset);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
        const bool scrollable = maxOffset > 0.0f;

        // 行内容区右边界扣除滚动条，避免行背景/命中区覆盖滚动条
        const float contentRight = maxPos.x - (scrollable ? (style.thickness + kBarGap) : 0.0f);

        range.min = pos;
        range.max = ImVec2(contentRight, maxPos.y);

        // 占位：整块视口（行控件在之后创建，会正常抢占 hover/点击）
        ImGui::PushID(label != nullptr ? label : "##gui_virtuallist");
        ImGui::Dummy(ImVec2(size.x, size.y));

        // 滚轮：指针在视口内时步进
        if (scrollable && ImGui::IsMouseHoveringRect(pos, maxPos, false))
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                current = std::clamp(current - wheel * kWheelStep, 0.0f, maxOffset);
            }
        }

        // 滚动条
        if (scrollable)
        {
            const ImVec2 barMin(maxPos.x - style.thickness, pos.y);
            DrawScrollBarAt(ImGui::GetID("##gui_vl_bar"), &current, size.y, totalHeight,
                            barMin, maxPos, false, style, tokens, kWheelStep);
        }

        // 可见区间（virtuallist.h:85-102）：first/last 为闭区间
        if (safeItemCount > 0)
        {
            const float overscanPixels = size.y * std::max(0.0f, overscanRows);
            const float firstPixel = std::max(0.0f, current - overscanPixels);
            const float lastPixel = std::min(totalHeight, current + size.y + overscanPixels);

            const int firstIndex = std::clamp(
                static_cast<int>(std::floor(firstPixel / row)), 0, safeItemCount - 1);
            int lastIndex = static_cast<int>(std::ceil(lastPixel / row)) + 1;
            lastIndex = std::clamp(lastIndex, firstIndex, safeItemCount - 1);

            range.first = firstIndex;
            range.last = lastIndex;
        }
        range.offset = current;

        ImGui::PopID();

        *offset = current;
        return range;
    }

    // =========================================================================
    // 虚拟列表行
    // 视觉：交替行底色 + hover 高亮 + 选中行 primary 淡底与左侧指示条
    // 交互：矩形由 range.first / rowHeight 推出，InvisibleButton 命中
    // =========================================================================
    bool GuiVirtualListRow(const char* label, const GuiVirtualRange& range, int index,
                           float rowHeight, bool selected)
    {
        if (index < 0 || range.last < range.first)
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌

        const float row = std::max(1.0f, rowHeight);
        const ImVec2 rowMin(range.min.x, range.min.y + row * static_cast<float>(index) - range.offset);
        const ImVec2 rowMax(range.max.x, rowMin.y + row);
        if (rowMax.y <= range.min.y || rowMin.y >= range.max.y)
        {
            return false;
        }

        // 跨越视口边界的行按视口裁剪命中区
        const ImVec2 hitMin(std::max(rowMin.x, range.min.x), std::max(rowMin.y, range.min.y));
        const ImVec2 hitMax(std::min(rowMax.x, range.max.x), std::min(rowMax.y, range.max.y));
        if (hitMax.x <= hitMin.x || hitMax.y <= hitMin.y)
        {
            return false;
        }

        ImGui::PushID(label != nullptr ? label : "##gui_vl_row");
        ImGui::PushID(index);
        const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(hitMin);
        const bool clicked = ImGui::InvisibleButton("##row", ImVec2(hitMax.x - hitMin.x, hitMax.y - hitMin.y));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::SetCursorScreenPos(savedCursor);

        // hover 平滑（跨帧状态走 ImGuiStorage）
        float* hoverAnim = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##row_hover") ^ kSaltRowHover, 0.0f);
        *hoverAnim = SmoothTowards(*hoverAnim, hovered ? 1.0f : 0.0f, kStateSmoothSpeed);
        ImGui::PopID();
        ImGui::PopID();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(range.min, range.max, true);

        // 行底色：奇数行淡表面色，hover 叠加高亮，选中行再叠 primary
        GuiColor background = ((index & 1) == 0)
                                  ? WithAlpha(tokens.surface, 0.55f)
                                  : WithAlpha(tokens.surfaceHover, 0.30f);
        if (*hoverAnim > 0.001f)
        {
            background = MixColor(background, WithAlpha(tokens.surfaceHover, background.a), *hoverAnim);
        }
        if (selected)
        {
            background = MixColor(background, WithAlpha(tokens.primary, 0.16f), 1.0f);
        }

        const float rounding = tokens.radius.small;
        drawList->AddRectFilled(rowMin, rowMax, background.ToImU32(), rounding);

        if (selected)
        {
            drawList->AddRectFilled(ImVec2(rowMin.x + 2.0f, rowMin.y + 4.0f),
                                    ImVec2(rowMin.x + 5.0f, rowMax.y - 4.0f),
                                    tokens.primary.ToImU32(), 1.5f);
        }

        // 索引文本（栈缓冲，零堆分配）
        char textBuffer[32];
        snprintf(textBuffer, sizeof(textBuffer), "#%d", index);
        drawList->AddText(ImVec2(rowMin.x + 12.0f, rowMin.y + (row - ImGui::GetTextLineHeight()) * 0.5f),
                          WithAlpha(tokens.text, selected ? 0.92f : 0.72f).ToImU32(), textBuffer);

        drawList->PopClipRect();
        return clicked;
    }

    // =========================================================================
    // 虚拟瀑布流
    // 列高前缀按 itemCount / columns / gap / 宽度变化重算（O(n)，其余帧 O(列数·log n)），
    // 只把可见 cell 交给 GuiVirtualMasonryCell 绘制
    // =========================================================================
    GuiMasonryRange GuiVirtualMasonry(const char* label, const ImVec2& size, int itemCount,
                                      int columns, float gap, float* offset,
                                      const std::function<float(int, float)>& heightOf,
                                      float overscanRows, const GuiScrollStyle& style)
    {
        GuiMasonryRange range;
        if (offset == nullptr || size.x <= 0.0f || size.y <= 0.0f)
        {
            return range;
        }

        GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌

        const int safeItemCount = std::max(0, itemCount);
        const int safeColumns = std::max(1, columns);
        const float safeGap = std::max(0.0f, gap);
        const float barWidth = style.thickness + kBarGap;
        const float contentWidth = std::max(10.0f, size.x - barWidth);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + size.x, pos.y + size.y);

        // 布局缓存 key 必须在 PushID 之前取：Cell 在同一作用域用 GetID(label) 查表
        const ImGuiID layoutKey = ImGui::GetID(label != nullptr ? label : "##gui_virtualmasonry");

        ImGui::PushID(label != nullptr ? label : "##gui_virtualmasonry");
        ImGui::Dummy(ImVec2(size.x, size.y));

        MasonryLayout& layout = g_masonryLayouts[layoutKey];
        g_lastMasonryKey = layoutKey;

        // 仅在参数变化时重算布局（浮点用容差比较）
        const bool layoutDirty = layout.itemCount != safeItemCount ||
                                 layout.columns != safeColumns ||
                                 std::fabs(layout.gap - safeGap) > kLayoutEpsilon ||
                                 std::fabs(layout.itemWidth * static_cast<float>(safeColumns) +
                                               safeGap * static_cast<float>(safeColumns - 1) -
                                               contentWidth) > 6.0f;
        if (layoutDirty)
        {
            BuildMasonryLayout(layout, safeItemCount, safeColumns, safeGap, contentWidth, heightOf);
        }

        const float totalHeight = layout.totalHeight;
        const float maxOffset = std::max(0.0f, totalHeight - size.y);
        float current = std::clamp(*offset, 0.0f, maxOffset);

        // 滚轮
        if (maxOffset > 0.0f && ImGui::IsMouseHoveringRect(pos, maxPos, false))
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                current = std::clamp(current - wheel * kWheelStep, 0.0f, maxOffset);
            }
        }

        // 滚动条
        if (maxOffset > 0.0f)
        {
            const ImVec2 barMin(maxPos.x - style.thickness, pos.y);
            DrawScrollBarAt(ImGui::GetID("##gui_masonry_bar"), &current, size.y, totalHeight,
                            barMin, maxPos, false, style, tokens, kWheelStep);
        }

        range.min = pos;
        range.max = ImVec2(pos.x + contentWidth, maxPos.y);
        range.offset = current;

        // 可见区间：逐列二分后取全局 index 的 min / max（cell 内部再精确剔除）
        if (safeItemCount > 0 && !layout.cells.empty())
        {
            const float overscanPixels = size.y * std::max(0.0f, overscanRows);
            const float visibleTop = std::max(0.0f, current - overscanPixels);
            const float visibleBottom = current + size.y + overscanPixels;

            int globalFirst = safeItemCount;
            int globalLast = -1;
            for (int column = 0; column < static_cast<int>(layout.columnIndices.size()); ++column)
            {
                int columnFirst = 0;
                int columnLast = -1;
                ColumnVisibleRange(layout, column, visibleTop, visibleBottom, columnFirst, columnLast);
                if (columnLast < columnFirst)
                {
                    continue;
                }
                globalFirst = std::min(globalFirst, columnFirst);
                globalLast = std::max(globalLast, columnLast);
            }

            if (globalLast >= globalFirst)
            {
                range.first = std::clamp(globalFirst, 0, safeItemCount - 1);
                range.last = std::clamp(globalLast, range.first, safeItemCount - 1);
            }
        }

        // 缓存本帧视口信息，供 GuiVirtualMasonryCell 换算屏幕矩形
        layout.viewMin = pos;
        layout.viewSize = size;
        layout.offset = current;

        ImGui::PopID();

        *offset = current;
        return range;
    }

    // =========================================================================
    // 虚拟瀑布流 cell
    // 矩形取自上一次 GuiVirtualMasonry 的布局缓存；不在视口内的 cell 直接跳过
    // 视觉：Card 圆角底色 + 边框 + hover 平滑高亮 + 选中 primary 描边 + 序号文本
    // =========================================================================
    bool GuiVirtualMasonryCell(const char* label, int index, bool selected)
    {
        if (index < 0)
        {
            return false;
        }

        ImGuiID layoutKey = ImGui::GetID(label != nullptr ? label : "##gui_virtualmasonry");
        auto it = g_masonryLayouts.find(layoutKey);
        if (it == g_masonryLayouts.end())
        {
            // label 与 GuiVirtualMasonry 不一致时回退到最近一次布局
            if (g_lastMasonryKey == 0)
            {
                return false;
            }
            layoutKey = g_lastMasonryKey;
            it = g_masonryLayouts.find(layoutKey);
            if (it == g_masonryLayouts.end())
            {
                return false;
            }
        }

        MasonryLayout& layout = it->second;
        if (index >= static_cast<int>(layout.cells.size()))
        {
            return false;
        }

        GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌

        const MasonryCell& cell = layout.cells[static_cast<std::size_t>(index)];
        const ImVec2 viewMin = layout.viewMin;
        const ImVec2 viewMax(viewMin.x + layout.viewSize.x, viewMin.y + layout.viewSize.y);
        const ImVec2 cellMin(viewMin.x + cell.x, viewMin.y + cell.y - layout.offset);
        const ImVec2 cellMax(cellMin.x + cell.width, cellMin.y + cell.height);

        // 视口外的 cell 不提交任何图元与命中区
        if (cellMax.y <= viewMin.y || cellMin.y >= viewMax.y)
        {
            return false;
        }

        ImGui::PushID(index);
        const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(cellMin);
        const bool clicked = ImGui::InvisibleButton("##cell", ImVec2(cell.width, cell.height));
        const bool hovered = ImGui::IsItemHovered() && ImGui::IsMouseHoveringRect(viewMin, viewMax, false);
        ImGui::SetCursorScreenPos(savedCursor);

        float* hoverAnim = ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("##cell_hover") ^ kSaltRowHover, 0.0f);
        *hoverAnim = SmoothTowards(*hoverAnim, hovered ? 1.0f : 0.0f, kStateSmoothSpeed);
        ImGui::PopID();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(viewMin, viewMax, true);

        const float rounding = tokens.radius.card;
        GuiColor background = MixColor(WithAlpha(tokens.surface, 0.92f), tokens.surfaceHover, 0.35f * *hoverAnim);
        if (selected)
        {
            background = MixColor(background, tokens.primary, 0.18f);
        }
        drawList->AddRectFilled(cellMin, cellMax, background.ToImU32(), rounding);
        drawList->AddRect(cellMin, cellMax,
                          WithAlpha(selected ? tokens.primary : tokens.border,
                                    selected ? 0.85f : 0.55f).ToImU32(),
                          rounding, ImDrawFlags_RoundCornersAll, 1.0f);

        char textBuffer[32];
        snprintf(textBuffer, sizeof(textBuffer), "#%d", index);
        drawList->AddText(ImVec2(cellMin.x + 10.0f, cellMin.y + 8.0f),
                          WithAlpha(tokens.text, selected ? 0.92f : 0.70f).ToImU32(), textBuffer);

        drawList->PopClipRect();
        return clicked;
    }
}
