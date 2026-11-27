// GuiNavigation.cpp
// 导航与容器控件的实现（声明见 BaseUI/GUIBase.h）
// 移植自 EUI-NEO components/tabs.h / carousel.h / datatable.h，
// 保留 EUI 暗色主题的视觉层级，改用 ImGui 立即模式 + ImDrawList 自绘

#include "GUIBase.h"

#include <cstdlib>
#include <unordered_map>

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
            1.0f, 0);

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
    // 移植自 EUI-NEO v0.6.3 components/datatable.h：
    //   - 固定行高虚拟化：只绘制可见行，滚动偏移走 GuiVirtualList
    //   - 共享数据源 + revision：筛选 / 排序索引只在源、条件或 revision 变化时重建
    //   - 稳定排序（可切换数值比较）与全列关键词筛选
    //   - 鼠标拖动列边界调整相邻两列宽度
    // 旧 GuiDatatable 入口保留为非交互兼容模式。
    // =========================================================================
    namespace
    {
        // 表格跨帧状态：索引 / 列宽 / 滚动偏移 / 上一次筛选排序条件
        struct GuiTableRuntime
        {
            const void* source = nullptr;
            std::size_t rowCount = 0;
            std::uint64_t revision = 0;
            std::string filter;
            int sortColumn = -1;
            bool descending = false;
            bool numeric = false;
            std::vector<int> order;
            std::vector<float> widths;
            std::vector<float> suppliedWidths;
            float available = -1.0f;
            float offset = 0.0f;
            int lastFrame = 0;
        };

        // 表头行命中区域：GuiTableHeadersRow() 记录，GuiTableHeaderColumnMenu() 用来判断
        // 「鼠标位于表头行任意位置」（IsItemHovered() 只反映最后提交的表头项）
        struct GuiTableHeaderHit
        {
            ImRect rect;
            int frame = 0;
        };

        GuiTableHeaderHit& GetGuiTableHeaderHit(ImGuiID id)
        {
            static std::unordered_map<ImGuiID, GuiTableHeaderHit> hits;
            const int frame = ImGui::GetFrameCount();
            static int lastCleanupFrame = 0;
            if (frame - lastCleanupFrame > 600)
            {
                for (auto it = hits.begin(); it != hits.end();)
                {
                    if (frame - it->second.frame > 2)
                    {
                        it = hits.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                lastCleanupFrame = frame;
            }
            return hits[id];
        };

        GuiTableRuntime& GetGuiTableRuntime(ImGuiID id)
        {
            static std::unordered_map<ImGuiID, GuiTableRuntime> runtimes;
            const int frame = ImGui::GetFrameCount();
            // 窗口关闭后释放对应的表格状态（按帧惰性清理，不在绘制路径做全量扫描）
            static int lastCleanupFrame = 0;
            if (frame - lastCleanupFrame > 600)
            {
                for (auto it = runtimes.begin(); it != runtimes.end();)
                {
                    if (frame - it->second.lastFrame > 600)
                    {
                        it = runtimes.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                lastCleanupFrame = frame;
            }
            GuiTableRuntime& runtime = runtimes[id];
            runtime.lastFrame = frame;
            return runtime;
        }

        const std::string& TableCellAt(const std::vector<std::string>& row, int column)
        {
            static const std::string empty;
            return (column >= 0 && static_cast<std::size_t>(column) < row.size())
                       ? row[static_cast<std::size_t>(column)]
                       : empty;
        }

        // 筛选 + 排序索引重建（条件未变化时直接复用）
        void RefreshGuiTableOrder(GuiTableRuntime& runtime,
                                  const std::vector<std::vector<std::string>>& rows,
                                  const std::string& filter, int sortColumn, bool descending,
                                  bool numeric, std::uint64_t revision, int columnCount)
        {
            if (sortColumn < 0 || sortColumn >= columnCount)
            {
                sortColumn = -1;
            }

            if (runtime.source == &rows && runtime.rowCount == rows.size() &&
                runtime.revision == revision && runtime.filter == filter &&
                runtime.sortColumn == sortColumn && runtime.descending == descending &&
                runtime.numeric == numeric)
            {
                return;
            }

            runtime.source = &rows;
            runtime.rowCount = rows.size();
            runtime.revision = revision;
            runtime.filter = filter;
            runtime.sortColumn = sortColumn;
            runtime.descending = descending;
            runtime.numeric = numeric;
            runtime.order.clear();
            runtime.order.reserve(rows.size());

            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                if (filter.empty())
                {
                    runtime.order.push_back(static_cast<int>(index));
                    continue;
                }
                for (const std::string& cell : rows[index])
                {
                    if (cell.find(filter) != std::string::npos)
                    {
                        runtime.order.push_back(static_cast<int>(index));
                        break;
                    }
                }
            }

            if (sortColumn >= 0)
            {
                // 数值排序：整格可解析为有限数时按大小比较，数值行排在文本行之前
                std::stable_sort(runtime.order.begin(), runtime.order.end(),
                                 [&rows, sortColumn, descending, numeric](int left, int right)
                                 {
                                     const std::string& a = TableCellAt(rows[static_cast<std::size_t>(left)], sortColumn);
                                     const std::string& b = TableCellAt(rows[static_cast<std::size_t>(right)], sortColumn);
                                     int comparison = a.compare(b);
                                     if (numeric)
                                     {
                                         char* aEnd = nullptr;
                                         char* bEnd = nullptr;
                                         const double aValue = std::strtod(a.c_str(), &aEnd);
                                         const double bValue = std::strtod(b.c_str(), &bEnd);
                                         const bool aNumber = aEnd != a.c_str() && *aEnd == '\0' && std::isfinite(aValue);
                                         const bool bNumber = bEnd != b.c_str() && *bEnd == '\0' && std::isfinite(bValue);
                                         if (aNumber && bNumber)
                                         {
                                             comparison = (aValue < bValue) ? -1 : ((aValue > bValue) ? 1 : 0);
                                         }
                                         else if (aNumber != bNumber)
                                         {
                                             comparison = aNumber ? -1 : 1;
                                         }
                                     }
                                     return descending ? comparison > 0 : comparison < 0;
                                 });
            }

            runtime.offset = 0.0f;
        }

        // 列宽：初始值按供给宽度 / 均分，可用宽度变化时等比缩放，最后夹到最小宽度
        void EnsureGuiTableWidths(GuiTableRuntime& runtime, int columnCount,
                                  const std::vector<float>& supplied, float available, float minimum)
        {
            if (columnCount <= 0)
            {
                runtime.widths.clear();
                return;
            }

            const bool columnsChanged = runtime.widths.size() != static_cast<std::size_t>(columnCount);
            const bool suppliedChanged = runtime.suppliedWidths != supplied;
            if (columnsChanged || suppliedChanged)
            {
                runtime.widths.assign(static_cast<std::size_t>(columnCount),
                                      available / static_cast<float>(columnCount));
                for (int column = 0; column < columnCount; ++column)
                {
                    if (column < static_cast<int>(supplied.size()) &&
                        std::isfinite(supplied[static_cast<std::size_t>(column)]) &&
                        supplied[static_cast<std::size_t>(column)] > 0.0f)
                    {
                        runtime.widths[static_cast<std::size_t>(column)] =
                            supplied[static_cast<std::size_t>(column)];
                    }
                }
                runtime.suppliedWidths = supplied;
                runtime.available = available;
            }
            else if (runtime.available > 0.0f && std::fabs(runtime.available - available) > 0.5f)
            {
                const float scale = available / runtime.available;
                for (float& width : runtime.widths)
                {
                    width = std::max(1.0f, width * scale);
                }
                runtime.available = available;
            }

            // 归一化到可用宽度
            float total = 0.0f;
            for (float width : runtime.widths)
            {
                total += std::max(1.0f, width);
            }
            if (total > 0.0f)
            {
                for (float& width : runtime.widths)
                {
                    width = std::max(1.0f, width) * available / total;
                }
            }

            // 最小宽度夹取：不足部分从超出最小宽度的列按比例回收
            const float minWidth = std::min(std::max(1.0f, minimum), available / static_cast<float>(columnCount));
            float excess = 0.0f;
            for (float& width : runtime.widths)
            {
                if (width < minWidth)
                {
                    excess += minWidth - width;
                    width = minWidth;
                }
            }
            if (excess > 0.0f)
            {
                float spare = 0.0f;
                for (float width : runtime.widths)
                {
                    spare += width - minWidth;
                }
                const float ratio = spare > 0.0f ? std::max(0.0f, (spare - excess) / spare) : 0.0f;
                for (float& width : runtime.widths)
                {
                    width = minWidth + (width - minWidth) * ratio;
                }
            }

            runtime.available = available;
        }
    }

    // 兼容入口：非交互（不排序 / 不调列宽），高度自适应全部行
    void GuiDatatable(const char* label,
                      const char* const* headers, int headerCount,
                      const std::vector<std::vector<std::string>>& rows)
    {
        GuiDatatableOptions options;
        options.sortable = false;
        options.resizable = false;
        GuiDatatableEx(label, headers, headerCount, rows, ImVec2(0.0f, 0.0f), options);
    }

    void GuiDatatableEx(const char* label,
                        const char* const* headers, int headerCount,
                        const std::vector<std::vector<std::string>>& rows,
                        const ImVec2& size, const GuiDatatableOptions& options)
    {
        if (headers == nullptr || headerCount <= 0)
        {
            return;
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        const float headerHeight = 40.0f;
        const float rowHeight = std::max(1.0f, options.rowHeight);
        const float radius = tokens.radius.card;
        const float barWidth = 8.0f;
        const float barGap = 4.0f;

        ImGui::PushID(label != nullptr ? label : "##gui_datatable");

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float tableWidth = (size.x > 0.0f) ? size.x : availableWidth;
        if (tableWidth <= 1.0f)
        {
            ImGui::PopID();
            return;
        }

        GuiTableRuntime& runtime = GetGuiTableRuntime(ImGui::GetID("##state"));

        // 受控排序优先；否则使用控件内部状态（点击表头更新）
        int sortColumn = (options.sortColumn >= 0) ? options.sortColumn : runtime.sortColumn;
        bool descending = (options.sortColumn >= 0) ? options.sortDescending : runtime.descending;

        RefreshGuiTableOrder(runtime, rows, options.filter, sortColumn, descending,
                             options.numericSort, options.revision, headerCount);

        // 先按行数确定滚动性与内容宽度（与 GuiVirtualList 的扣除规则保持一致）
        const float totalRowsHeight = static_cast<float>(runtime.order.size()) * rowHeight;
        const float bodyHeight = (size.y > 0.0f) ? std::max(0.0f, size.y - headerHeight) : totalRowsHeight;
        const bool scrollable = totalRowsHeight > bodyHeight + 0.5f;
        const float contentWidth = std::max(1.0f, tableWidth - (scrollable ? barWidth + barGap : 0.0f));
        const float tableHeight = headerHeight + bodyHeight;

        EnsureGuiTableWidths(runtime, headerCount, options.columnWidths, contentWidth,
                             options.minColumnWidth);

        // === 表头交互：点击排序 / 拖动列边界 ===
        {
            const ImVec2 savedCursor = ImGui::GetCursorScreenPos();

            if (options.sortable)
            {
                float x = pos.x;
                for (int column = 0; column < headerCount; ++column)
                {
                    ImGui::PushID(column);
                    ImGui::SetCursorScreenPos(ImVec2(x, pos.y));
                    ImGui::InvisibleButton("##header",
                                           ImVec2(runtime.widths[static_cast<std::size_t>(column)],
                                                  headerHeight));
                    const bool hovered = ImGui::IsItemHovered();
                    if (hovered)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }
                    if (ImGui::IsItemClicked())
                    {
                        const bool nextDescending = (sortColumn == column) ? !descending : false;
                        if (options.onSort)
                        {
                            options.onSort(column, nextDescending);
                        }
                        if (options.sortColumn < 0)
                        {
                            // 非受控：本帧立即生效
                            runtime.sortColumn = column;
                            runtime.descending = nextDescending;
                            sortColumn = column;
                            descending = nextDescending;
                        }
                    }
                    ImGui::PopID();
                    x += runtime.widths[static_cast<std::size_t>(column)];
                }
            }

            if (options.resizable)
            {
                float x = pos.x;
                for (int column = 0; column + 1 < headerCount; ++column)
                {
                    x += runtime.widths[static_cast<std::size_t>(column)];
                    ImGui::PushID(1000 + column);
                    ImGui::SetCursorScreenPos(ImVec2(x - 4.0f, pos.y));
                    ImGui::InvisibleButton("##colresize", ImVec2(8.0f, headerHeight));
                    const bool hovered = ImGui::IsItemHovered();
                    const bool active = ImGui::IsItemActive();
                    if (hovered || active)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                    }
                    if (active)
                    {
                        const float delta = ImGui::GetIO().MouseDelta.x;
                        if (delta != 0.0f)
                        {
                            float& leftWidth = runtime.widths[static_cast<std::size_t>(column)];
                            float& rightWidth = runtime.widths[static_cast<std::size_t>(column + 1)];
                            const float sum = leftWidth + rightWidth;
                            const float minimum = std::min(options.minColumnWidth, sum * 0.5f);
                            const float nextLeft = std::clamp(leftWidth + delta, minimum, sum - minimum);
                            leftWidth = nextLeft;
                            rightWidth = sum - nextLeft;
                            if (options.onColumnWidthsChange)
                            {
                                options.onColumnWidthsChange(runtime.widths);
                            }
                        }
                    }
                    ImGui::PopID();
                }
            }

            ImGui::SetCursorScreenPos(savedCursor);
        }

        // 表头点击可能刚改变排序：再刷一次索引（行数不变，滚动性不变）
        RefreshGuiTableOrder(runtime, rows, options.filter, sortColumn, descending,
                             options.numericSort, options.revision, headerCount);

        // === 数据体：虚拟化滚动 + 可见行 ===
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + headerHeight));
        GuiVirtualScrollOptions scrollOptions;
        scrollOptions.scrollbarWidth = barWidth;
        scrollOptions.scrollbarGap = barGap;
        scrollOptions.wheelStep = rowHeight * 2.0f;
        const GuiVirtualRange range = GuiVirtualList("##body", ImVec2(tableWidth, bodyHeight),
                                                     static_cast<int>(runtime.order.size()), rowHeight,
                                                     &runtime.offset, 1.0f, GuiScrollStyle(), scrollOptions);

        const float bodyTop = pos.y + headerHeight;
        const float bodyBottom = bodyTop + bodyHeight;

        if (!runtime.order.empty())
        {
            // 行内容裁剪到数据体，避免滚动时压到表头 / 表格外
            drawList->PushClipRect(ImVec2(pos.x, bodyTop), ImVec2(pos.x + tableWidth, bodyBottom), true);
            for (int view = range.first; view <= range.last; ++view)
            {
                if (view < 0 || view >= static_cast<int>(runtime.order.size()))
                {
                    continue;
                }
                const int sourceRow = runtime.order[static_cast<std::size_t>(view)];
                if (sourceRow < 0 || sourceRow >= static_cast<int>(rows.size()))
                {
                    continue;
                }

                const float rowY = bodyTop + static_cast<float>(view) * rowHeight - runtime.offset;
                const ImVec2 rowMin(pos.x, rowY);
                const ImVec2 rowMax(pos.x + contentWidth, rowY + rowHeight);
                if (rowMax.y <= bodyTop || rowMin.y >= bodyBottom)
                {
                    continue;
                }

                const bool lastView = (view == static_cast<int>(runtime.order.size()) - 1);
                const GuiColor rowColor = (view % 2 != 0)
                                              ? MixColor(tokens.surface, tokens.surfaceHover, 0.30f)
                                              : tokens.surface;
                const ImDrawFlags rowFlags = lastView
                                                 ? (ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight)
                                                 : ImDrawFlags_None;
                drawList->AddRectFilled(rowMin, rowMax, rowColor.ToImU32(), radius, rowFlags);

                // 行命中：用原始行号作为 ID，排序后仍稳定；点击回传原始行号
                ImGui::PushID(sourceRow);
                ImGui::SetCursorScreenPos(rowMin);
                const bool clicked = ImGui::InvisibleButton("##row", ImVec2(contentWidth, rowHeight));
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();

                if (hovered)
                {
                    drawList->AddRectFilled(rowMin, rowMax,
                                            WithAlpha(tokens.surfaceHover, 0.55f).ToImU32(), radius, rowFlags);
                }
                if (clicked && options.onRowClick)
                {
                    options.onRowClick(static_cast<std::size_t>(sourceRow));
                }

                const std::vector<std::string>& row = rows[static_cast<std::size_t>(sourceRow)];
                float cellX = pos.x;
                for (int column = 0; column < headerCount; ++column)
                {
                    const std::string& text = TableCellAt(row, column);
                    if (!text.empty())
                    {
                        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
                        drawList->AddText(ImVec2(cellX + 16.0f, rowY + (rowHeight - textSize.y) * 0.5f),
                                          tokens.text.ToImU32(), text.c_str());
                    }
                    cellX += runtime.widths[static_cast<std::size_t>(column)];
                }

                if (!lastView)
                {
                    drawList->AddLine(ImVec2(pos.x, rowMax.y - 1.0f),
                                      ImVec2(pos.x + contentWidth, rowMax.y - 1.0f),
                                      WithAlpha(tokens.border, 0.22f).ToImU32());
                }
            }
            drawList->PopClipRect();
        }
        else
        {
            const char* message = options.filter.empty() ? "暂无数据" : "无匹配数据";
            const ImVec2 extent = ImGui::CalcTextSize(message);
            drawList->AddText(ImVec2(pos.x + (contentWidth - extent.x) * 0.5f,
                                     bodyTop + std::max(0.0f, (bodyHeight - extent.y) * 0.5f)),
                              WithAlpha(tokens.text, 0.45f).ToImU32(), message);
        }

        // === 表头绘制（最后绘制，保证覆盖行内容） ===
        const GuiColor headerColor = MixColor(tokens.surfaceHover, tokens.surface, 0.32f);
        const GuiColor dividerColor = WithAlpha(tokens.border, 0.42f);
        const GuiColor borderColor = WithAlpha(tokens.border, 0.72f);

        drawList->AddRectFilled(pos, ImVec2(pos.x + tableWidth, pos.y + headerHeight),
                                headerColor.ToImU32(), radius,
                                ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight);
        drawList->AddLine(ImVec2(pos.x, pos.y + headerHeight - 1.0f),
                          ImVec2(pos.x + tableWidth, pos.y + headerHeight - 1.0f),
                          dividerColor.ToImU32());

        float headerX = pos.x;
        for (int column = 0; column < headerCount; ++column)
        {
            const char* text = headers[column] != nullptr ? headers[column] : "";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            drawList->AddText(ImVec2(headerX + 16.0f, pos.y + (headerHeight - textSize.y) * 0.5f),
                              tokens.primary.ToImU32(), text);

            if (sortColumn == column)
            {
                const char* icon = descending ? ICON_FA_ARROW_DOWN : ICON_FA_ARROW_UP;
                const ImVec2 iconSize = ImGui::CalcTextSize(icon);
                const float iconX = headerX + runtime.widths[static_cast<std::size_t>(column)] - iconSize.x - 12.0f;
                drawList->AddText(ImVec2(iconX, pos.y + (headerHeight - iconSize.y) * 0.5f),
                                  WithAlpha(tokens.primary, 0.86f).ToImU32(), icon);
            }
            headerX += runtime.widths[static_cast<std::size_t>(column)];
        }

        // 外框
        drawList->AddRect(pos, ImVec2(pos.x + tableWidth, pos.y + tableHeight),
                          borderColor.ToImU32(), radius, 1.0f, 0);

        // 推进布局光标到表格底部（GuiVirtualList 的 Dummy 只到数据体底部）
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + tableHeight));
        ImGui::PopID();
    }

    // =========================================================================
    // 轮播图文项（EUI carousel.h:58/65-67：图文卡片项 + 堆叠参数）
    // 中心卡片完整显示，左右相邻卡片按 parallax 缩小、按 overlap 叠在其下；
    // 点击箭头或相邻卡片切换，只绘制 ±1 张邻居（远侧不提交图元）
    // =========================================================================
    bool GuiCarouselEx(const char* label, const GuiCarouselItem* items, int itemCount,
                       int* selectedIndex, float width, float height,
                       const GuiCarouselOptions& options)
    {
        if (items == nullptr || itemCount <= 0 || selectedIndex == nullptr ||
            width <= 0.0f || height <= 0.0f)
        {
            return false;
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + width, pos.y + height);
        const float centerX = pos.x + width * 0.5f;
        const float centerY = pos.y + height * 0.5f;

        const float ratio = std::clamp(options.cardWidthRatio, 0.32f, 1.0f);
        const float overlap = std::clamp(options.overlap, 0.0f, 0.60f);
        const float parallax = std::max(0.0f, options.parallax);
        const float cardWidth = std::max(40.0f, width * ratio);
        const float cardHeight = std::max(40.0f, height * 0.84f);
        const float stepX = std::max(20.0f, cardWidth * (1.0f - overlap));
        const float neighborScale = std::max(0.45f, 1.0f - 0.20f * parallax);

        ImGui::PushID(label != nullptr ? label : "##gui_carousel_ex");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImGui::Dummy(ImVec2(width, height));

        const int originalIndex = *selectedIndex;
        const int target = std::clamp(originalIndex, 0, itemCount - 1);
        int pendingIndex = target;

        // 位移动画：animIndex 沿环形最短路径平滑靠近 target。
        // 绘制与命中都基于本帧固定的 animIndex，点击只记录 pendingIndex，
        // 避免切换途中修改当前索引导致同一张卡片被重复提交（ImGui ID 冲突红框）。
        const float cycle = static_cast<float>(itemCount);
        float* animIndex = ImGui::GetStateStorage()->GetFloatRef(
            ImGui::GetID("##carousel_anim"), static_cast<float>(target));
        {
            float delta = static_cast<float>(target) - *animIndex;
            if (itemCount > 1)
            {
                delta = std::fmod(delta, cycle);
                if (delta > cycle * 0.5f)
                {
                    delta -= cycle;
                }
                else if (delta <= -cycle * 0.5f)
                {
                    delta += cycle;
                }
            }
            const float speed = 1.0f / std::max(0.02f, options.duration);
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
            *animIndex += delta * (1.0f - std::exp(-speed * dt));

            // 收敛后吸附到整数目标，避免无限逼近
            float remaining = static_cast<float>(target) - *animIndex;
            if (itemCount > 1)
            {
                remaining = std::fmod(remaining, cycle);
                if (remaining > cycle * 0.5f)
                {
                    remaining -= cycle;
                }
                else if (remaining <= -cycle * 0.5f)
                {
                    remaining += cycle;
                }
            }
            if (std::fabs(remaining) < 0.001f)
            {
                *animIndex = static_cast<float>(target);
            }
        }

        // 收集可见卡片（中心 ±1.5 槽位），按距离从远到近绘制，中心最后画在最上层
        struct CarouselSlot
        {
            float slot = 0.0f;
            int index = 0;
        };
        CarouselSlot visible[4];
        int visibleCount = 0;
        for (int index = 0; index < itemCount; ++index)
        {
            float slot = static_cast<float>(index) - *animIndex;
            if (itemCount > 1)
            {
                slot = std::fmod(slot, cycle);
                if (slot > cycle * 0.5f)
                {
                    slot -= cycle;
                }
                else if (slot <= -cycle * 0.5f)
                {
                    slot += cycle;
                }
            }

            const float distance = std::fabs(slot);
            if (distance > 1.5f)
            {
                continue;
            }

            // 插入排序：距离大的先绘制
            int insertAt = visibleCount;
            while (insertAt > 0 && std::fabs(visible[insertAt - 1].slot) < distance)
            {
                visible[insertAt] = visible[insertAt - 1];
                --insertAt;
            }
            if (visibleCount < static_cast<int>(sizeof(visible) / sizeof(visible[0])))
            {
                visible[insertAt] = CarouselSlot{ slot, index };
                ++visibleCount;
            }
        }

        for (int drawn = 0; drawn < visibleCount; ++drawn)
        {
            const float slot = visible[drawn].slot;
            const int index = visible[drawn].index;
            const float distance = std::fabs(slot);
            const float amount = std::min(1.0f, distance);

            // 卡片滑到 ±1.5 槽位时淡出，避免环形边界处瞬间翻到另一侧
            const float edgeAlpha = std::clamp((1.5f - distance) / 0.3f, 0.0f, 1.0f);
            if (edgeAlpha <= 0.0f)
            {
                continue;
            }

            const float scale = 1.0f + (neighborScale - 1.0f) * amount;
            const float cardW = cardWidth * scale;
            const float cardH = cardHeight * scale;
            const float cardCenterX = centerX + slot * stepX;
            const ImVec2 cardMin(cardCenterX - cardW * 0.5f, centerY - cardH * 0.5f);
            const ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);
            if (cardMax.x <= pos.x || cardMin.x >= maxPos.x)
            {
                continue;
            }

            // 命中：每张卡片每帧只提交一次 InvisibleButton
            ImGui::PushID(index);
            const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(cardMin);
            const bool clicked = ImGui::InvisibleButton("##card", ImVec2(cardW, cardH));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::SetCursorScreenPos(savedCursor);
            ImGui::PopID();

            if (clicked && index != target)
            {
                pendingIndex = index;
            }

            const float rounding = tokens.radius.section;
            const GuiCarouselItem& item = items[index];

            // 阴影：中心 20/10/0.30，邻居 14/6/0.20，按槽位连续插值
            GuiShadowStyle shadow;
            shadow.blur = 20.0f + (14.0f - 20.0f) * amount;
            shadow.offsetX = 0.0f;
            shadow.offsetY = 10.0f + (6.0f - 10.0f) * amount;
            shadow.color = GuiColor(0.0f, 0.0f, 0.0f,
                                    (0.30f + (0.20f - 0.30f) * amount) * edgeAlpha);
            DrawGuiSoftShadow(drawList, cardMin, ImVec2(cardW, cardH), shadow, rounding);

            // 卡片主体：纹理或表面色占位
            if (item.texture != nullptr)
            {
                drawList->AddImageRounded((ImTextureID)item.texture, cardMin, cardMax,
                                          ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                          IM_COL32(255, 255, 255, static_cast<int>(255.0f * edgeAlpha)),
                                          rounding);
            }
            else
            {
                drawList->AddRectFilled(cardMin, cardMax,
                                        WithAlpha(tokens.surfaceHover, edgeAlpha).ToImU32(), rounding);
            }
            if (hovered && index != target)
            {
                drawList->AddRectFilled(cardMin, cardMax,
                                        IM_COL32(255, 255, 255, static_cast<int>(14.0f * edgeAlpha)),
                                        rounding);
            }
            drawList->AddRect(cardMin, cardMax,
                              WithAlpha(tokens.border,
                                        (0.60f + (0.35f - 0.60f) * amount) * edgeAlpha).ToImU32(),
                              rounding, ImDrawFlags_RoundCornersAll, 1.0f);

            // 底部渐变 + 标题 / 副标题
            const float overlayHeight = cardH * 0.38f;
            const ImVec2 overlayMin(cardMin.x, cardMax.y - overlayHeight);
            const int overlayAlpha = static_cast<int>(180.0f * edgeAlpha);
            drawList->AddRectFilledMultiColor(overlayMin, cardMax,
                                              IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                              IM_COL32(0, 0, 0, overlayAlpha),
                                              IM_COL32(0, 0, 0, overlayAlpha));

            ImFont* font = ImGui::GetFont();
            if (font != nullptr)
            {
                float textBottom = cardMax.y - 10.0f;
                if (item.subtitle != nullptr && item.subtitle[0] != '\0')
                {
                    const float subtitleSize = tokens.typography.label * scale;
                    const ImVec2 extent = font->CalcTextSizeA(subtitleSize, cardW - 16.0f,
                                                              cardW - 16.0f, item.subtitle);
                    drawList->AddText(font, subtitleSize,
                                      ImVec2(cardMin.x + (cardW - extent.x) * 0.5f,
                                             textBottom - extent.y),
                                      IM_COL32(255, 255, 255, static_cast<int>(200.0f * edgeAlpha)),
                                      item.subtitle);
                    textBottom -= extent.y + 2.0f;
                }
                if (item.title != nullptr && item.title[0] != '\0')
                {
                    const float titleSize = tokens.typography.subtitle * scale;
                    const ImVec2 extent = font->CalcTextSizeA(titleSize, cardW - 16.0f,
                                                              cardW - 16.0f, item.title);
                    drawList->AddText(font, titleSize,
                                      ImVec2(cardMin.x + (cardW - extent.x) * 0.5f,
                                             textBottom - extent.y),
                                      IM_COL32(255, 255, 255, static_cast<int>(250.0f * edgeAlpha)),
                                      item.title);
                }
            }
        }

        // === 左右箭头（首尾环绕） ===
        const float buttonSize = std::clamp(height * 0.14f, 26.0f, 40.0f);
        const float halfButton = buttonSize * 0.5f;
        const ImVec2 leftCenter(pos.x + tokens.spacing.section + halfButton, centerY);
        const ImVec2 rightCenter(maxPos.x - tokens.spacing.section - halfButton, centerY);

        auto drawArrow = [&](int buttonIndex, const ImVec2& buttonCenter, const char* icon) -> bool
        {
            ImGui::PushID(1000 + buttonIndex);
            const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(buttonCenter.x - halfButton, buttonCenter.y - halfButton));
            const bool arrowClicked = ImGui::InvisibleButton("##arrow", ImVec2(buttonSize, buttonSize));
            const bool arrowHovered = ImGui::IsItemHovered();
            const bool arrowActive = ImGui::IsItemActive();
            ImGui::SetCursorScreenPos(savedCursor);
            ImGui::PopID();

            const GuiColor background = arrowActive ? tokens.surfaceActive
                                                    : (arrowHovered ? tokens.surfaceHover : tokens.surface);
            drawList->AddCircleFilled(buttonCenter, halfButton, WithAlpha(background, 0.92f).ToImU32(), 24);
            drawList->AddCircle(buttonCenter, halfButton, WithAlpha(tokens.border, 0.62f).ToImU32(), 24, 1.0f);

            ImFont* font = ImGui::GetFont();
            if (font != nullptr)
            {
                const float iconSize = tokens.typography.option;
                const ImVec2 extent = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
                drawList->AddText(font, iconSize,
                                  ImVec2(buttonCenter.x - extent.x * 0.5f,
                                         buttonCenter.y - extent.y * 0.5f),
                                  tokens.text.ToImU32(), icon);
            }
            return arrowClicked;
        };

        // 箭头点击只记录目标索引，绘制仍使用本帧固定的 animIndex
        const bool wrap = (itemCount > 1);
        if (wrap && drawArrow(0, leftCenter, ICON_FA_CHEVRON_LEFT))
        {
            pendingIndex = (target - 1 + itemCount) % itemCount;
        }
        if (wrap && drawArrow(1, rightCenter, ICON_FA_CHEVRON_RIGHT))
        {
            pendingIndex = (target + 1) % itemCount;
        }

        const bool changed = (pendingIndex != originalIndex);
        if (changed)
        {
            *selectedIndex = pendingIndex;
        }

        // 底部指示点
        const float dotSpacing = 14.0f;
        const float dotsWidth = static_cast<float>(itemCount - 1) * dotSpacing;
        if (dotsWidth + 24.0f <= width)
        {
            const float dotY = maxPos.y - 10.0f;
            float dotX = centerX - dotsWidth * 0.5f;
            for (int index = 0; index < itemCount; ++index)
            {
                if (index == pendingIndex)
                {
                    drawList->AddRectFilled(ImVec2(dotX - 9.0f, dotY - 3.0f),
                                            ImVec2(dotX + 9.0f, dotY + 3.0f),
                                            tokens.primary.ToImU32(), 3.0f);
                }
                else
                {
                    drawList->AddCircleFilled(ImVec2(dotX, dotY), 3.0f,
                                              WithAlpha(tokens.text, 0.32f).ToImU32(), 12);
                }
                dotX += dotSpacing;
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(pos.x, maxPos.y));
        ImGui::PopID();
        return changed;
    }

    // =========================================================================
    // 可定制数据表格
    // 在 ImGui 表格之上做一层薄封装：列定义 / 尺寸 / flags 组装由这里负责，
    // 单元格内容由调用方用 GuiText / GuiImage / GuiTableCellProgress 自绘。
    // 列索引统一按「可见列序号」编号（跳过 visible=false 的列）。
    // =========================================================================
    bool BeginGuiTable(const char* label, const GuiTableColumn* columns, int columnCount,
                       const GuiTableOptions& options)
    {
        if (columns == nullptr || columnCount <= 0)
        {
            return false;
        }

        int visibleCount = 0;
        for (int index = 0; index < columnCount; ++index)
        {
            if (columns[index].visible)
            {
                ++visibleCount;
            }
        }
        if (visibleCount <= 0)
        {
            return false;
        }

        ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp;
        if (options.resizable) tableFlags |= ImGuiTableFlags_Resizable;
        if (options.sortable) tableFlags |= ImGuiTableFlags_Sortable;
        if (options.rowBackground) tableFlags |= ImGuiTableFlags_RowBg;
        if (options.bordersInnerV) tableFlags |= ImGuiTableFlags_BordersInnerV;
        if (options.scrollY) tableFlags |= ImGuiTableFlags_ScrollY;
        if (options.padOuterX) tableFlags |= ImGuiTableFlags_PadOuterX;

        ImGui::PushID(label != nullptr ? label : "##gui_table");
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 size(options.size.x > 0.0f ? options.size.x : avail.x,
                          options.size.y > 0.0f ? options.size.y : avail.y);

        if (!ImGui::BeginTable("##table", visibleCount, tableFlags, size))
        {
            ImGui::PopID();
            return false;
        }

        for (int index = 0; index < columnCount; ++index)
        {
            const GuiTableColumn& column = columns[index];
            if (!column.visible)
            {
                continue;
            }

            const char* header = (column.header != nullptr) ? column.header : "";
            ImGuiTableColumnFlags columnFlags = column.flags;
            if (column.width > 0.0f)
            {
                columnFlags |= ImGuiTableColumnFlags_WidthFixed;
                ImGui::TableSetupColumn(header, columnFlags, column.width);
            }
            else
            {
                columnFlags |= ImGuiTableColumnFlags_WidthStretch;
                ImGui::TableSetupColumn(header, columnFlags);
            }
        }

        return true;
    }

    void EndGuiTable()
    {
        ImGui::EndTable();
        ImGui::PopID();
    }

    void GuiTableHeadersRow()
    {
        const ImVec2 top = ImGui::GetCursorScreenPos();
        ImGui::TableHeadersRow();
        const ImVec2 bottom = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        GuiTableHeaderHit& hit = GetGuiTableHeaderHit(ImGui::GetID("##gui_table_header"));
        hit.rect = ImRect(top, ImVec2(top.x + width, bottom.y));
        hit.frame = ImGui::GetFrameCount();
    }

    bool GuiTableConsumeSort(int* column, bool* ascending)
    {
        ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
        if (specs == nullptr || !specs->SpecsDirty || specs->SpecsCount <= 0)
        {
            return false;
        }

        const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
        if (column != nullptr)
        {
            *column = static_cast<int>(spec.ColumnIndex);
        }
        if (ascending != nullptr)
        {
            *ascending = (spec.SortDirection == ImGuiSortDirection_Ascending);
        }
        specs->SpecsDirty = false;
        return true;
    }

    void GuiTableNextRow()
    {
        ImGui::TableNextRow();
    }

    void GuiTableCell(int column, bool alignTextToFramePadding)
    {
        ImGui::TableSetColumnIndex(column);
        if (alignTextToFramePadding)
        {
            ImGui::AlignTextToFramePadding();
        }
    }

    void GuiTableCellProgress(float value, float width, const GuiColor& fill, const GuiColor& track)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float barHeight = 6.0f;
        const float y = pos.y + (ImGui::GetTextLineHeight() - barHeight) * 0.5f;
        DrawGuiProgress(drawList, ImVec2(pos.x, y), ImVec2(width, barHeight),
                        std::clamp(value, 0.0f, 1.0f), fill, track);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width + ImGui::GetStyle().ItemSpacing.x);
    }

    void GuiTableRows(int rowCount, const std::function<void(int)>& rowCallback)
    {
        if (rowCount <= 0 || !rowCallback)
        {
            return;
        }

        ImGuiListClipper clipper;
        clipper.Begin(rowCount);
        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
            {
                rowCallback(index);
            }
        }
        clipper.End();
    }

    void GuiTableHeaderColumnMenu(const char* label, const GuiTableColumnToggle* items, int itemCount)
    {
        if (items == nullptr || itemCount <= 0)
        {
            return;
        }

        const char* popupId = (label != nullptr && label[0] != '\0') ? label : "##gui_table_columns";
        // 表头行任意位置右键都可打开（GuiTableHeadersRow 记录了整行命中区域）
        const GuiTableHeaderHit& hit = GetGuiTableHeaderHit(ImGui::GetID("##gui_table_header"));
        const bool headerHovered = (hit.frame == ImGui::GetFrameCount()) &&
                                   ImGui::IsMouseHoveringRect(hit.rect.Min, hit.rect.Max);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && (headerHovered || ImGui::IsItemHovered()))
        {
            ImGui::OpenPopup(popupId);
        }

        if (!ImGui::BeginPopup(popupId))
        {
            return;
        }

        const GuiThemeTokens tokens = Theme::Dark();
        GuiText("##columns_title", "列设置", 0.0f, WithAlpha(tokens.text, 0.72f));
        ImGui::Separator();
        for (int index = 0; index < itemCount; ++index)
        {
            if (items[index].value == nullptr)
            {
                continue;
            }
            GuiCheckbox(items[index].label != nullptr ? items[index].label : "", items[index].value);
        }
        ImGui::EndPopup();
    }
    }
