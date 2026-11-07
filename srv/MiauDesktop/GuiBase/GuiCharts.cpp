// GuiCharts.cpp
// 图表组件的实现（声明见 BaseUI/GUIBase.h）
// 使用 ImDrawList 绘制柱状图 / 折线图 / 饼图 / 进度条四种图表
// 移植自 EUI-NEO components/barchart.h / linechart.h / piechart.h / progress.h

#include "GUIBase.h"

#include <algorithm>
#include <cmath>
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
                                  lineColor, false, lineThickness);
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
                                  lineColor, false, lineThickness);
        }
        else
        {
            // Linear：直接连接各点（Curve 但点数不足时同样回退到直线）
            drawList->AddPolyline(points.data(), count, lineColor, false, lineThickness);
        }

        // 数据点（近似圆，半径 6.5）
        for (int i = 0; i < count; i++)
        {
            drawList->AddCircleFilled(points[static_cast<size_t>(i)], 6.5f, lineColor, 24);
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
}
