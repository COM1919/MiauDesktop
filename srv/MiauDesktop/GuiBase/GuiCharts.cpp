// GuiCharts.cpp
// 图表组件的实现（声明见 BaseUI/GUIBase.h）
// 使用 ImDrawList 绘制柱状图 / 折线图 / 饼图 / 进度条四种图表
// 移植自 EUI-NEO components/barchart.h / linechart.h / piechart.h / progress.h

#include "GUIBase.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <unordered_map>
#include <vector>

namespace GuiBase
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;
    }

    // 图表样式默认构造：使用暗色主题
    GuiChartStyle::GuiChartStyle()
        : GuiChartStyle(Theme::Dark())
    {
    }

    // 从主题令牌构造：按视觉规范填充各项默认值
    GuiChartStyle::GuiChartStyle(const GuiThemeTokens& tokens)
    {
        backgroundColor = tokens.surface;
        textColor = tokens.text;
        // 网格线：暗色用白色 38% 透明度，亮色用黑色 36% 透明度
        gridColor = tokens.isDark ? GuiColor(1.0f, 1.0f, 1.0f, 0.38f)
                                  : GuiColor(0.0f, 0.0f, 0.0f, 0.36f);
        radius = tokens.radius.section;
        shadow = GetShadowStyle(GuiShadowLevel::Chart, tokens.isDark);
        palette[0] = GuiColor(0.22f, 0.50f, 0.88f, 1.0f);  // Blue
        palette[1] = GuiColor(0.20f, 0.76f, 0.58f, 1.0f);  // Green
        palette[2] = GuiColor(0.98f, 0.62f, 0.15f, 1.0f);  // Orange
        palette[3] = GuiColor(0.86f, 0.28f, 0.44f, 1.0f);  // Pink
        titleFontSize = tokens.typography.title;
        labelFontSize = tokens.typography.label;
    }

    // 由图表样式构造 Card 样式：复用背景色 / 圆角 / 阴影，保留默认边框
    GuiCardStyle ChartStyleToCardStyle(const GuiChartStyle& style)
    {
        GuiCardStyle card;
        card.backgroundColor = style.backgroundColor;
        card.radius = style.radius;
        card.shadow = style.shadow;
        return card;
    }

    // 绘制柱状图：背景 + 标题 + 网格线 + 圆角柱子 + 标签
    void DrawGuiBarChart(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                         const float* values, int count,
                         const char* const* labels, const char* title,
                         const GuiChartStyle& style)
    {
        if (drawList == nullptr || values == nullptr || count <= 0)
        {
            return;
        }

        // 背景（阴影 → 圆角矩形 → 边框）
        DrawGuiCardBackground(drawList, pos, size, ChartStyleToCardStyle(style));

        // 标题（位于左上角内边距处）
        if (title != nullptr)
        {
            drawList->AddText(ImGui::GetFont(), style.titleFontSize,
                              pos + ImVec2(20, 18), style.textColor.ToImU32(), title);
        }

        // 布局常量
        const float plotX = 32.0f;
        const float plotY = 70.0f;
        const float plotWidth = std::max(1.0f, size.x - 64.0f);
        const float plotHeight = std::max(1.0f, size.y - 112.0f);
        const float bottomY = pos.y + plotY + plotHeight;

        // 网格线：均匀分布在 plotHeight 内
        for (int i = 0; i <= 4; i++)
        {
            float y = pos.y + plotY + plotHeight * static_cast<float>(i) / 4.0f;
            drawList->AddLine(ImVec2(pos.x + plotX, y),
                              ImVec2(pos.x + plotX + plotWidth, y),
                              style.gridColor.ToImU32(), 1.0f);
        }

        // 柱子
        const float slotWidth = plotWidth / static_cast<float>(count);
        const float barWidth = std::min(32.0f, std::max(18.0f, slotWidth * 0.54f));
        ImFont* font = ImGui::GetFont();
        for (int i = 0; i < count; i++)
        {
            float v = std::clamp(values[i], 0.0f, 1.0f);
            float barHeight = std::max(8.0f, v * plotHeight);
            float barX = pos.x + plotX + slotWidth * static_cast<float>(i) + (slotWidth - barWidth) * 0.5f;
            float barY = bottomY - barHeight;
            float rounding = std::min(10.0f, barWidth * 0.34f);
            ImU32 color = style.palette[i % 4].ToImU32();

            // 仅圆顶的圆角矩形
            drawList->AddRectFilled(ImVec2(barX, barY),
                                    ImVec2(barX + barWidth, barY + barHeight),
                                    color, rounding, ImDrawFlags_RoundCornersTop);

            // 标签（柱子下方，水平居中）
            if (labels != nullptr && labels[i] != nullptr)
            {
                float labelWidth = ImGui::CalcTextSize(labels[i]).x;
                drawList->AddText(font, style.labelFontSize,
                                  ImVec2(barX + barWidth * 0.5f - labelWidth * 0.5f,
                                         pos.y + plotY + plotHeight + 8.0f),
                                  style.textColor.ToImU32(), labels[i]);
            }
        }
    }

    // 绘制折线图：背景 + 标题 + 网格线 + 折线 + 数据点
    void DrawGuiLineChart(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                          const float* values, int count,
                          const char* title,
                          const GuiChartStyle& style, GuiLineStyle lineStyle)
    {
        if (drawList == nullptr || values == nullptr || count <= 0)
        {
            return;
        }

        // 背景
        DrawGuiCardBackground(drawList, pos, size, ChartStyleToCardStyle(style));

        // 标题
        if (title != nullptr)
        {
            drawList->AddText(ImGui::GetFont(), style.titleFontSize,
                              pos + ImVec2(20, 18), style.textColor.ToImU32(), title);
        }

        // 布局常量
        const float plotX = 32.0f;
        const float plotY = 70.0f;
        const float plotWidth = std::max(1.0f, size.x - 64.0f);
        const float plotHeight = std::max(1.0f, size.y - 112.0f);
        const float plotTop = pos.y + plotY;
        const float plotBottom = pos.y + plotY + plotHeight;

        // 网格线
        for (int i = 0; i <= 4; i++)
        {
            float y = pos.y + plotY + plotHeight * static_cast<float>(i) / 4.0f;
            drawList->AddLine(ImVec2(pos.x + plotX, y),
                              ImVec2(pos.x + plotX + plotWidth, y),
                              style.gridColor.ToImU32(), 1.0f);
        }

        // 计算数据点坐标（count-1 段间隔）
        const float slotWidth = count > 1 ? plotWidth / static_cast<float>(count - 1) : 0.0f;
        std::vector<ImVec2> points(static_cast<size_t>(count));
        for (int i = 0; i < count; i++)
        {
            float v = std::clamp(values[i], 0.0f, 1.0f);
            points[static_cast<size_t>(i)] = ImVec2(
                pos.x + plotX + slotWidth * static_cast<float>(i),
                pos.y + plotY + plotHeight * (1.0f - v));
        }

        const ImU32 lineColor = style.palette[0].ToImU32();
        const float lineThickness = 4.0f;

        // 绘制折线
        if (lineStyle == GuiLineStyle::Curve && count > 2)
        {
            // Catmull-Rom 样条插值，每段采样 8 个点
            auto catmullRom = [](const ImVec2& p0, const ImVec2& p1,
                                 const ImVec2& p2, const ImVec2& p3, float t)
            {
                float t2 = t * t;
                float t3 = t2 * t;
                ImVec2 r;
                r.x = 0.5f * ((2 * p1.x) + (-p0.x + p2.x) * t
                              + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2
                              + (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
                r.y = 0.5f * ((2 * p1.y) + (-p0.y + p2.y) * t
                              + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2
                              + (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
                return r;
            };

            std::vector<ImVec2> curve;
            curve.reserve(static_cast<size_t>((count - 1) * 8 + 1));
            curve.push_back(points[0]);
            for (int i = 0; i + 1 < count; i++)
            {
                const ImVec2& p0 = points[static_cast<size_t>(i == 0 ? 0 : i - 1)];
                const ImVec2& p1 = points[static_cast<size_t>(i)];
                const ImVec2& p2 = points[static_cast<size_t>(i + 1)];
                const ImVec2& p3 = points[static_cast<size_t>(i + 2 < count ? i + 2 : count - 1)];
                for (int s = 1; s <= 8; s++)
                {
                    ImVec2 pt = catmullRom(p0, p1, p2, p3, static_cast<float>(s) / 8.0f);
                    pt.y = std::clamp(pt.y, plotTop, plotBottom);
                    curve.push_back(pt);
                }
            }
            drawList->AddPolyline(curve.data(), static_cast<int>(curve.size()),
                                  lineColor, lineThickness, ImDrawFlags_None);
        }
        else if (lineStyle == GuiLineStyle::Step)
        {
            // 阶梯线：每段先水平到下一个 x，再垂直到下一个 y
            std::vector<ImVec2> step;
            step.reserve(static_cast<size_t>(count * 2));
            step.push_back(points[0]);
            for (int i = 0; i + 1 < count; i++)
            {
                const ImVec2& from = points[static_cast<size_t>(i)];
                const ImVec2& to = points[static_cast<size_t>(i + 1)];
                step.push_back(ImVec2(to.x, from.y));
                step.push_back(to);
            }
            drawList->AddPolyline(step.data(), static_cast<int>(step.size()),
                                  lineColor, lineThickness, ImDrawFlags_None);
        }
        else
        {
            // Linear：直接连接各点（Curve 但点数不足时同样回退到直线）
            drawList->AddPolyline(points.data(), count, lineColor, lineThickness, ImDrawFlags_None);
        }

        // 数据点（近似圆，半径 6.5）
        for (int i = 0; i < count; i++)
        {
            drawList->AddCircleFilled(points[static_cast<size_t>(i)], 6.5f, lineColor, 24);
        }
    }

    // 绘制实时折线图：像素桶降采样（首/谷/峰/尾）+ 分段批量折线
    void DrawGuiLineChartLive(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                              const float* values, int count,
                              const char* title, const GuiChartStyle& style,
                              const GuiLineChartLiveOptions& options)
    {
        if (drawList == nullptr || values == nullptr || count <= 0)
        {
            return;
        }

        // 背景
        DrawGuiCardBackground(drawList, pos, size, ChartStyleToCardStyle(style));

        // 标题
        if (title != nullptr)
        {
            drawList->AddText(ImGui::GetFont(), style.titleFontSize,
                              pos + ImVec2(20, 18), style.textColor.ToImU32(), title);
        }

        // 布局常量（与 DrawGuiLineChart 保持一致）
        const float plotX = 32.0f;
        const float plotY = 70.0f;
        const float plotWidth = std::max(1.0f, size.x - 64.0f);
        const float plotHeight = std::max(1.0f, size.y - 112.0f);

        // 网格线
        for (int i = 0; i <= 4; ++i)
        {
            const float y = pos.y + plotY + plotHeight * static_cast<float>(i) / 4.0f;
            drawList->AddLine(ImVec2(pos.x + plotX, y),
                              ImVec2(pos.x + plotX + plotWidth, y),
                              style.gridColor.ToImU32(), 1.0f);
        }

        const std::size_t sampleCount = static_cast<std::size_t>(count);
        const std::size_t first = std::min(options.first, sampleCount);
        const std::size_t last = std::min(options.last, sampleCount);
        if (last <= first)
        {
            return;
        }
        const std::size_t visible = last - first;

        float minimum = options.minimum;
        float maximum = options.maximum;
        if (options.autoRange)
        {
            bool found = false;
            for (std::size_t index = first; index < last; ++index)
            {
                const float value = values[index];
                if (!std::isfinite(value))
                {
                    continue;
                }
                if (!found)
                {
                    minimum = maximum = value;
                    found = true;
                }
                else
                {
                    minimum = std::min(minimum, value);
                    maximum = std::max(maximum, value);
                }
            }
            if (!found)
            {
                minimum = 0.0f;
                maximum = 1.0f;
            }
        }
        if (!std::isfinite(minimum) || !std::isfinite(maximum) || maximum <= minimum)
        {
            minimum = 0.0f;
            maximum = 1.0f;
        }
        const float valueRange = maximum - minimum;

        // 每个水平像素最多一个桶；桶内保留首、谷、峰、尾四个点
        const std::size_t bucketCount = std::min<std::size_t>(
            visible, static_cast<std::size_t>(std::max(1.0f, std::ceil(plotWidth))));

        auto toPoint = [&](std::size_t index, float value) -> ImVec2
        {
            const float x = (visible > 1)
                                ? plotWidth * static_cast<float>(index - first) / static_cast<float>(visible - 1)
                                : plotWidth * 0.5f;
            const float normalized = std::clamp((value - minimum) / valueRange, 0.0f, 1.0f);
            return ImVec2(pos.x + plotX + x, pos.y + plotY + plotHeight * (1.0f - normalized));
        };

        std::vector<ImVec2> points;
        points.reserve(bucketCount * 4 + 4);
        for (std::size_t bucket = 0; bucket < bucketCount; ++bucket)
        {
            const std::size_t begin = first + visible * bucket / bucketCount;
            const std::size_t end = first + visible * (bucket + 1) / bucketCount;
            if (end <= begin)
            {
                continue;
            }

            std::size_t low = begin;
            std::size_t high = begin;
            bool found = false;
            bool gap = false;
            for (std::size_t index = begin; index < end; ++index)
            {
                const float value = values[index];
                if (!std::isfinite(value))
                {
                    gap = true;
                    continue;
                }
                if (!found)
                {
                    low = high = index;
                    found = true;
                }
                else
                {
                    if (value < values[low])
                    {
                        low = index;
                    }
                    if (value > values[high])
                    {
                        high = index;
                    }
                }
            }

            if (!found || gap)
            {
                // 缺失值桶：插入 NaN 断点，折线在此处分段
                points.push_back(ImVec2(std::numeric_limits<float>::quiet_NaN(), 0.0f));
                continue;
            }

            std::size_t indices[4] = { begin, low, high, end - 1 };
            std::sort(indices, indices + 4);
            std::size_t previous = end; // 哨兵：第一个点一定输出
            for (std::size_t index : indices)
            {
                if (index == previous)
                {
                    continue;
                }
                previous = index;
                points.push_back(toPoint(index, values[index]));
            }
        }

        const ImU32 lineColor = style.palette[0].ToImU32();
        const float thickness = std::max(0.5f, options.lineThickness);

        // 分段批量绘制：NaN 作为断点，避免 ImDrawList 收到无效顶点
        std::vector<ImVec2> segment;
        segment.reserve(points.size());
        int validCount = 0;
        auto flush = [&]()
        {
            if (segment.size() >= 2)
            {
                drawList->AddPolyline(segment.data(), static_cast<int>(segment.size()),
                                      lineColor, thickness, ImDrawFlags_None);
            }
            segment.clear();
        };
        for (const ImVec2& point : points)
        {
            if (!std::isfinite(point.x))
            {
                flush();
                continue;
            }
            segment.push_back(point);
            ++validCount;
        }
        flush();

        // 可见区间只有一个采样时画一个小圆点
        if (points.size() == 1 && validCount == 1)
        {
            drawList->AddCircleFilled(points.front(), std::max(2.0f, thickness * 1.5f), lineColor, 16);
        }
    }

    // 绘制饼图：背景 + 扇形 + 外侧标签
    void DrawGuiPieChart(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                         const float* values, int count,
                         const char* const* labels,
                         const GuiChartStyle& style)
    {
        if (drawList == nullptr || values == nullptr || count <= 0)
        {
            return;
        }

        // 背景
        DrawGuiCardBackground(drawList, pos, size, ChartStyleToCardStyle(style));

        // 中心和半径（稍微下移给标题留空间，留出边距）
        float centerX = pos.x + size.x * 0.5f;
        float centerY = pos.y + size.y * 0.5f + 10.0f;
        float radius = std::min(size.x - 48.0f, size.y - 82.0f) * 0.5f;
        if (radius <= 0.0f)
        {
            return;
        }

        // 总值
        float total = 0.0f;
        for (int i = 0; i < count; i++)
        {
            total += std::max(0.0f, values[i]);
        }
        if (total <= 0.0f)
        {
            return;
        }

        // 绘制扇形（起始角 -π/2 对应正上方）
        float startAngle = -kPi * 0.5f;
        for (int i = 0; i < count; i++)
        {
            float amount = std::max(0.0f, values[i]) / total;
            float sweep = amount * 2.0f * kPi;
            float endAngle = startAngle + sweep;
            ImU32 color = style.palette[i % 4].ToImU32();

            drawList->PathLineTo(ImVec2(centerX, centerY));
            drawList->PathArcTo(ImVec2(centerX, centerY), radius, startAngle, endAngle, 32);
            drawList->PathFillConvex(color);

            startAngle = endAngle;
        }

        // 外侧标签
        if (labels == nullptr)
        {
            return;
        }

        ImFont* font = ImGui::GetFont();
        startAngle = -kPi * 0.5f;
        for (int i = 0; i < count; i++)
        {
            float amount = std::max(0.0f, values[i]) / total;
            float sweep = amount * 2.0f * kPi;
            if (labels[i] != nullptr && sweep > 0.0f)
            {
                float midAngle = startAngle + sweep * 0.5f;
                float lx = centerX + (radius + 12.0f) * std::cos(midAngle);
                float ly = centerY + (radius + 12.0f) * std::sin(midAngle);
                float labelWidth = ImGui::CalcTextSize(labels[i]).x;
                drawList->AddText(font, style.labelFontSize,
                                  ImVec2(lx - labelWidth * 0.5f, ly - style.labelFontSize * 0.5f),
                                  style.textColor.ToImU32(), labels[i]);
            }
            startAngle += sweep;
        }
    }

    // 绘制胶囊形进度条：track 底色 + fill 前景色
    // value 会被 clamp 到 [0, 1]，radius = height * 0.5 形成胶囊两端
    void DrawGuiProgress(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                         float value, GuiColor fillColor, GuiColor trackColor)
    {
        if (drawList == nullptr)
        {
            return;
        }

        value = std::clamp(value, 0.0f, 1.0f);
        float radius = size.y * 0.5f;

        // track（底色）
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                trackColor.ToImU32(), radius, ImDrawFlags_RoundCornersAll);

        // fill（前景色）：fillWidth 为 0 时不绘制；小于圆角直径时缩减圆角避免裁剪
        float fillWidth = size.x * value;
        if (fillWidth > 0.0f)
        {
            float fillRounding = std::min(radius, fillWidth * 0.5f);
            drawList->AddRectFilled(pos, ImVec2(pos.x + fillWidth, pos.y + size.y),
                                    fillColor.ToImU32(), fillRounding, ImDrawFlags_RoundCornersAll);
        }
    }

    // =========================================================================
    // 进度条控件（对应 EUI-NEO progress.h 的 ProgressBuilder）
    // 轨道 + 填充 + 帧率无关的值过渡；width = 0 时取可用宽度
    // =========================================================================
    void GuiProgress(const char* label, float value, const GuiProgressOptions& options)
    {
        const GuiThemeTokens tokens = Theme::Dark();
        ImGui::PushID(label != nullptr ? label : "##gui_progress");

        const float width = options.width > 0.0f
                                ? options.width
                                : std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float height = std::max(2.0f, options.height);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float target = std::clamp(value, 0.0f, 1.0f);

        // 值过渡（EUI progress.h:70-77 的 transition）
        const ImGuiID animId = ImGui::GetID("##progress_anim");
        float* anim = ImGui::GetStateStorage()->GetFloatRef(animId, target);
        const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
        const float speed = 1.0f / std::max(0.02f, options.transition.durationSeconds);
        const float amount = options.transition.enabled ? (1.0f - std::exp(-speed * dt)) : 1.0f;
        *anim += (target - *anim) * amount;
        if (std::fabs(target - *anim) < 0.001f)
        {
            *anim = target;
        }

        char labelText[16];
        float labelWidth = 0.0f;
        if (options.showLabel)
        {
            std::snprintf(labelText, sizeof(labelText), "%d%%",
                          static_cast<int>(std::lround(*anim * 100.0f)));
            labelWidth = ImGui::CalcTextSize(labelText).x + tokens.spacing.compact;
        }
        const float barWidth = std::max(1.0f, width - labelWidth);

        const GuiColor trackColor = options.trackColor.a > 0.0f
                                        ? options.trackColor : tokens.surfaceHover;
        const GuiColor fillColor = options.fillColor.a > 0.0f
                                       ? options.fillColor : tokens.primary;
        const float radius = (options.radius >= 0.0f)
                                 ? std::min(options.radius, height * 0.5f)
                                 : height * 0.5f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, ImVec2(pos.x + barWidth, pos.y + height),
                                trackColor.ToImU32(), radius, ImDrawFlags_RoundCornersAll);

        const float fillWidth = barWidth * std::clamp(*anim, 0.0f, 1.0f);
        if (fillWidth > 0.0f)
        {
            const float fillRounding = std::min(radius, fillWidth * 0.5f);
            drawList->AddRectFilled(pos, ImVec2(pos.x + fillWidth, pos.y + height),
                                    fillColor.ToImU32(), fillRounding, ImDrawFlags_RoundCornersAll);
        }

        if (options.showLabel)
        {
            const ImVec2 textSize = ImGui::CalcTextSize(labelText);
            drawList->AddText(ImVec2(pos.x + width - textSize.x,
                                     pos.y + (height - textSize.y) * 0.5f),
                              tokens.text.ToImU32(), labelText);
        }

        ImGui::Dummy(ImVec2(width, height));
        ImGui::PopID();
    }
    namespace
    {
        struct GuiChartAnimState
        {
            std::vector<float> values;
            bool initialized = false;
        };

        std::unordered_map<ImGuiID, GuiChartAnimState>& ChartAnimStates()
        {
            static std::unordered_map<ImGuiID, GuiChartAnimState>* states =
                new std::unordered_map<ImGuiID, GuiChartAnimState>();
            return *states;
        }

        // 图表值过渡：首帧直接采用目标值，后续按 GuiTransition 平滑
        const std::vector<float>& AnimateChartValues(ImGuiID id, const float* values, int count,
                                                     bool animate, const GuiTransition& transition)
        {
            GuiChartAnimState& state = ChartAnimStates()[id];
            if (!state.initialized || static_cast<int>(state.values.size()) != count)
            {
                state.values.assign(values, values + count);
                state.initialized = true;
                return state.values;
            }

            if (animate && transition.enabled)
            {
                const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
                const float speed = 1.0f / std::max(0.02f, transition.durationSeconds);
                const float amount = 1.0f - std::exp(-speed * dt);
                for (int index = 0; index < count; ++index)
                {
                    float& current = state.values[static_cast<std::size_t>(index)];
                    current += (values[index] - current) * amount;
                    if (std::fabs(values[index] - current) < 1e-4f)
                    {
                        current = values[index];
                    }
                }
            }
            else
            {
                for (int index = 0; index < count; ++index)
                {
                    state.values[static_cast<std::size_t>(index)] = values[index];
                }
            }
            return state.values;
        }

        void ChartTooltipForValue(const char* const* labels, int index, float value, bool percent)
        {
            char text[96];
            if (percent)
            {
                if (labels != nullptr && labels[index] != nullptr)
                {
                    std::snprintf(text, sizeof(text), "%s: %.1f%%", labels[index], value * 100.0f);
                }
                else
                {
                    std::snprintf(text, sizeof(text), "%.1f%%", value * 100.0f);
                }
            }
            else
            {
                if (labels != nullptr && labels[index] != nullptr)
                {
                    std::snprintf(text, sizeof(text), "%s: %.2f", labels[index], value);
                }
                else
                {
                    std::snprintf(text, sizeof(text), "%.2f", value);
                }
            }
            GuiTooltip(text);
        }
    }

    void GuiBarChart(const char* label, const float* values, int count,
                     const char* const* labels, const char* title,
                     const GuiChartStyle& style, const GuiChartOptions& options)
    {
        if (values == nullptr || count <= 0)
        {
            return;
        }

        ImGui::PushID(label != nullptr ? label : "##gui_barchart");
        const float width = options.width > 0.0f
                                ? options.width
                                : std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float height = std::max(80.0f, options.height);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, height);
        const ImGuiID chartId = ImGui::GetID("##chart_values");

        const std::vector<float>& animated =
            AnimateChartValues(chartId, values, count, options.animate, options.transition);
        DrawGuiBarChart(ImGui::GetWindowDrawList(), pos, size, animated.data(), count,
                        labels, title, style);

        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##chart_hit", size);
        if (ImGui::IsItemHovered() && options.showTooltip)
        {
            const float plotX = 32.0f;
            const float plotWidth = std::max(1.0f, size.x - 64.0f);
            const float slotWidth = plotWidth / static_cast<float>(count);
            const float localX = ImGui::GetIO().MousePos.x - (pos.x + plotX);
            const int index = std::clamp(static_cast<int>(localX / slotWidth), 0, count - 1);
            ChartTooltipForValue(labels, index, animated[static_cast<std::size_t>(index)], false);
        }
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
        ImGui::PopID();
    }

    void GuiLineChart(const char* label, const float* values, int count,
                      const char* const* labels, const char* title,
                      GuiLineStyle lineStyle, const GuiChartStyle& style,
                      const GuiChartOptions& options)
    {
        if (values == nullptr || count <= 0)
        {
            return;
        }

        ImGui::PushID(label != nullptr ? label : "##gui_linechart");
        const float width = options.width > 0.0f
                                ? options.width
                                : std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float height = std::max(80.0f, options.height);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, height);
        const ImGuiID chartId = ImGui::GetID("##chart_values");

        const std::vector<float>& animated =
            AnimateChartValues(chartId, values, count, options.animate, options.transition);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        DrawGuiLineChart(drawList, pos, size, animated.data(), count, title, style, lineStyle);

        // X 轴标签（跳过过密项）
        if (labels != nullptr)
        {
            ImFont* font = ImGui::GetFont();
            const float plotX = 32.0f;
            const float plotWidth = std::max(1.0f, size.x - 64.0f);
            const int maxLabels = std::max(1, static_cast<int>(plotWidth / 56.0f));
            const int stride = std::max(1, (count + maxLabels - 1) / maxLabels);
            for (int index = 0; index < count; index += stride)
            {
                if (labels[index] == nullptr)
                {
                    continue;
                }
                const float t = (count > 1)
                                    ? static_cast<float>(index) / static_cast<float>(count - 1)
                                    : 0.5f;
                const ImVec2 extent = font->CalcTextSizeA(style.labelFontSize, FLT_MAX, 0.0f, labels[index]);
                drawList->AddText(font, style.labelFontSize,
                                  ImVec2(pos.x + plotX + plotWidth * t - extent.x * 0.5f,
                                         pos.y + size.y - 30.0f),
                                  style.textColor.ToImU32(), labels[index]);
            }
        }

        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##chart_hit", size);
        if (ImGui::IsItemHovered() && options.showTooltip)
        {
            const float plotX = 32.0f;
            const float plotWidth = std::max(1.0f, size.x - 64.0f);
            const float step = (count > 1) ? plotWidth / static_cast<float>(count - 1) : 0.0f;
            const float localX = ImGui::GetIO().MousePos.x - (pos.x + plotX);
            const int index = (step > 0.0f)
                                  ? std::clamp(static_cast<int>(std::lround(localX / step)), 0, count - 1)
                                  : 0;
            ChartTooltipForValue(labels, index, animated[static_cast<std::size_t>(index)], false);
        }
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
        ImGui::PopID();
    }

    void GuiPieChart(const char* label, const float* values, int count,
                     const char* const* labels,
                     const GuiChartStyle& style, const GuiChartOptions& options)
    {
        if (values == nullptr || count <= 0)
        {
            return;
        }

        ImGui::PushID(label != nullptr ? label : "##gui_piechart");
        const float width = options.width > 0.0f
                                ? options.width
                                : std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float height = std::max(80.0f, options.height);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, height);
        const ImGuiID chartId = ImGui::GetID("##chart_values");

        const std::vector<float>& animated =
            AnimateChartValues(chartId, values, count, options.animate, options.transition);
        DrawGuiPieChart(ImGui::GetWindowDrawList(), pos, size, animated.data(), count, labels, style);

        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##chart_hit", size);
        if (ImGui::IsItemHovered() && options.showTooltip)
        {
            const ImGuiIO& io = ImGui::GetIO();
            const float centerX = pos.x + size.x * 0.5f;
            const float centerY = pos.y + size.y * 0.5f + 10.0f;
            const float radius = std::min(size.x - 48.0f, size.y - 82.0f) * 0.5f;
            const float dx = io.MousePos.x - centerX;
            const float dy = io.MousePos.y - centerY;
            if (radius > 0.0f && dx * dx + dy * dy <= radius * radius)
            {
                // 饼图从 -π/2 开始顺时针；转换为从顶部起算的 [0, 2π)
                float angle = std::atan2(dy, dx) + kPi * 0.5f;
                while (angle < 0.0f)
                {
                    angle += 2.0f * kPi;
                }
                while (angle >= 2.0f * kPi)
                {
                    angle -= 2.0f * kPi;
                }

                float total = 0.0f;
                for (int index = 0; index < count; ++index)
                {
                    total += std::max(0.0f, animated[static_cast<std::size_t>(index)]);
                }
                if (total > 0.0f)
                {
                    float cumulative = 0.0f;
                    for (int index = 0; index < count; ++index)
                    {
                        const float portion = std::max(0.0f, animated[static_cast<std::size_t>(index)]) / total;
                        cumulative += portion * 2.0f * kPi;
                        if (angle <= cumulative)
                        {
                            ChartTooltipForValue(labels, index, portion, true);
                            break;
                        }
                    }
                }
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
        ImGui::PopID();
    }

    // =========================================================================
    // 时钟控件（对应 EUI-NEO apps/clock 的模拟表盘 + 数字时间 + 日期）
    // =========================================================================
    void GuiClock(const char* label, const ImVec2& size, const GuiClockOptions& options)
    {
        GuiThemeTokens tokens = Theme::Dark();
        ImGui::PushID(label != nullptr ? label : "##gui_clock");

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float width = std::max(80.0f, size.x > 0.0f ? size.x : 180.0f);
        const float height = std::max(120.0f, size.y > 0.0f ? size.y : 220.0f);
        float reserved = 12.0f;
        if (options.showDigitalTime)
        {
            reserved += 40.0f;
        }
        if (options.showDate)
        {
            reserved += 22.0f;
        }
        const float diameter = std::max(40.0f, std::min(width, height - reserved));
        const float radius = diameter * 0.5f;
        const ImVec2 center(pos.x + width * 0.5f, pos.y + radius + 6.0f);

        const std::time_t now =
            std::time(nullptr) + static_cast<std::time_t>(options.utcOffsetMinutes) * 60;
        std::tm tm{};
#ifdef _WIN32
        if (options.useLocalTime)
        {
            localtime_s(&tm, &now);
        }
        else
        {
            gmtime_s(&tm, &now);
        }
#else
        if (options.useLocalTime)
        {
            localtime_r(&now, &tm);
        }
        else
        {
            gmtime_r(&now, &tm);
        }
#endif

        constexpr float kTwoPi = 6.28318530717958647692f;
        const float secondAngle = -kPi * 0.5f + (static_cast<float>(tm.tm_sec) / 60.0f) * kTwoPi;
        const float minuteAngle = -kPi * 0.5f +
            ((static_cast<float>(tm.tm_min) + static_cast<float>(tm.tm_sec) / 60.0f) / 60.0f) * kTwoPi;
        const int hour12 = tm.tm_hour % 12;
        const float hourAngle = -kPi * 0.5f +
            ((static_cast<float>(hour12) + static_cast<float>(tm.tm_min) / 60.0f) / 12.0f) * kTwoPi;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const GuiColor face = options.faceColor.a > 0.0f ? options.faceColor : tokens.surface;
        const GuiColor tick = options.tickColor.a > 0.0f ? options.tickColor
                                                         : WithAlpha(tokens.text, 0.32f);
        const GuiColor hand = options.handColor.a > 0.0f ? options.handColor : tokens.text;
        const GuiColor accent = options.accentColor.a > 0.0f ? options.accentColor : tokens.primary;
        const GuiColor text = options.textColor.a > 0.0f ? options.textColor : tokens.text;

        drawList->AddCircleFilled(center, radius, face.ToImU32(), 64);
        drawList->AddCircle(center, radius,
                            WithAlpha(tokens.border, tokens.isDark ? 0.70f : 0.55f).ToImU32(),
                            64, 1.0f);

        // 60 个刻度：每 5 个为主刻度
        for (int index = 0; index < 60; ++index)
        {
            const float angle = -kPi * 0.5f + kTwoPi * static_cast<float>(index) / 60.0f;
            const bool major = (index % 5 == 0);
            const float inner = radius * (major ? 0.84f : 0.90f);
            const float outer = radius * 0.95f;
            drawList->AddLine(
                ImVec2(center.x + std::cos(angle) * inner, center.y + std::sin(angle) * inner),
                ImVec2(center.x + std::cos(angle) * outer, center.y + std::sin(angle) * outer),
                major ? tick.ToImU32() : WithAlpha(tick, tick.a * 0.45f).ToImU32(),
                major ? 2.0f : 1.0f);
        }

        // 12 / 3 / 6 / 9 数字
        ImFont* font = ImGui::GetFont();
        if (font != nullptr)
        {
            const float numeralSize = std::max(10.0f, radius * 0.16f);
            for (int index = 0; index < 12; index += 3)
            {
                const float angle = -kPi * 0.5f + kTwoPi * static_cast<float>(index) / 12.0f;
                char numeral[4];
                std::snprintf(numeral, sizeof(numeral), "%d", index == 0 ? 12 : index);
                const ImVec2 extent = font->CalcTextSizeA(numeralSize, FLT_MAX, 0.0f, numeral);
                drawList->AddText(
                    font, numeralSize,
                    ImVec2(center.x + std::cos(angle) * radius * 0.70f - extent.x * 0.5f,
                           center.y + std::sin(angle) * radius * 0.70f - extent.y * 0.5f),
                    WithAlpha(text, 0.72f).ToImU32(), numeral);
            }
        }

        // 时针 / 分针 / 秒针
        auto drawHand = [&](float angle, float length, float thickness, ImU32 color)
        {
            drawList->AddLine(
                center,
                ImVec2(center.x + std::cos(angle) * length, center.y + std::sin(angle) * length),
                color, thickness);
        };
        drawHand(hourAngle, radius * 0.48f, std::max(3.0f, radius * 0.055f), hand.ToImU32());
        drawHand(minuteAngle, radius * 0.72f, std::max(2.0f, radius * 0.035f), hand.ToImU32());
        if (options.showSeconds)
        {
            drawHand(secondAngle, radius * 0.84f, std::max(1.0f, radius * 0.018f), accent.ToImU32());
            drawList->AddCircleFilled(
                ImVec2(center.x + std::cos(secondAngle) * radius * 0.84f,
                       center.y + std::sin(secondAngle) * radius * 0.84f),
                std::max(2.0f, radius * 0.03f), accent.ToImU32(), 12);
        }
        drawList->AddCircleFilled(center, std::max(3.0f, radius * 0.045f), accent.ToImU32(), 16);

        // 数字时间 + 日期（可分别关闭，TimeDisplay 只复用表盘时全部关闭）
        const float timeFontSize = std::max(14.0f, radius * 0.28f);
        const float timeY = center.y + radius + 10.0f;
        if (options.showDigitalTime)
        {
            int displayHour = tm.tm_hour;
            if (!options.use24Hour)
            {
                displayHour %= 12;
                if (displayHour == 0)
                {
                    displayHour = 12;
                }
            }
            char timeText[16];
            std::snprintf(timeText, sizeof(timeText),
                          options.use24Hour ? "%02d:%02d:%02d" : "%d:%02d:%02d",
                          displayHour, tm.tm_min, tm.tm_sec);
            const ImVec2 timeExtent = font->CalcTextSizeA(timeFontSize, FLT_MAX, 0.0f, timeText);
            drawList->AddText(font, timeFontSize,
                              ImVec2(center.x - timeExtent.x * 0.5f, timeY),
                              text.ToImU32(), timeText);
        }

        if (options.showDate)
        {
            char dateText[32];
            std::snprintf(dateText, sizeof(dateText), "%04d-%02d-%02d",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            const float dateFontSize = std::max(10.0f, radius * 0.15f);
            const ImVec2 dateExtent = font->CalcTextSizeA(dateFontSize, FLT_MAX, 0.0f, dateText);
            const float dateY = options.showDigitalTime ? (timeY + timeFontSize + 2.0f) : timeY;
            drawList->AddText(font, dateFontSize,
                              ImVec2(center.x - dateExtent.x * 0.5f, dateY),
                              WithAlpha(text, 0.62f).ToImU32(), dateText);
        }

        ImGui::Dummy(ImVec2(width, height));
        ImGui::PopID();
    }
}
