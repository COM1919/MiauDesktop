// GuiCard.cpp
// Card 渲染系统的实现（声明见 BaseUI/GUIBase.h）
// 使用 ImDrawList 绘制圆角矩形、高斯软阴影、边框与渐变填充
// 移植自 EUI-NEO components/card.h，配色与视觉层级保持一致

#include "GUIBase.h"

#include <algorithm>
#include <cmath>

namespace GuiBase
{
    // Card 样式默认构造：使用暗色主题令牌
    GuiCardStyle::GuiCardStyle()
        : GuiCardStyle(Theme::Dark())
    {
    }

    // 从主题令牌构造：按视觉规范填充各项默认值
    GuiCardStyle::GuiCardStyle(const GuiThemeTokens& tokens)
    {
        radius = tokens.radius.section;
        padding = tokens.spacing.large;
        backgroundColor = tokens.surface;
        borderColor = tokens.border;
        borderWidth = 1.0f;
        borderAlpha = 0.72f;
        shadow = GetShadowStyle(GuiShadowLevel::Panel, tokens.isDark);
        opacity = 1.0f;
    }

    // 绘制高斯软阴影：通过多层半透明圆角矩形叠加模拟高斯模糊
    // 从最外层到最内层依次绘制，每层向外扩展并按高斯分布衰减 alpha
    void DrawGuiSoftShadow(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                           const GuiShadowStyle& shadow, float rounding)
    {
        if (drawList == nullptr || shadow.blur <= 0.0f)
        {
            return;
        }

        // 层数越多，每层扩展量与 alpha 差异越小，视觉上越接近真实高斯模糊
        // blur=24 时为 16 层
        int layers = (int)std::max(8.0f, shadow.blur / 1.5f);
        float expand = shadow.blur / (float)layers;
        // 高斯分布参数：分布较宽，外层衰减柔和、内层饱满
        float sigma = (float)layers / 4.0f;

        // 阴影偏移：所有层都加上 offsetX/offsetY
        ImVec2 shadowPos = ImVec2(pos.x + shadow.offsetX, pos.y + shadow.offsetY);

        // 从最外层到最内层绘制：外层先画（低 alpha），内层后画（高 alpha）覆盖在上
        for (int i = layers - 1; i >= 0; i--)
        {
            float dist = (float)i;
            float gaussAlpha = std::exp(-(dist * dist) / (2.0f * sigma * sigma));
            float layerAlpha = shadow.color.a * gaussAlpha;

            ImU32 color = IM_COL32(
                (int)(shadow.color.r * 255.0f),
                (int)(shadow.color.g * 255.0f),
                (int)(shadow.color.b * 255.0f),
                (int)(layerAlpha * 255.0f));

            // 该层向外扩展量：(i+1) * expand，外层扩展更多
            float ext = expand * (float)(i + 1);
            ImVec2 pmin = ImVec2(shadowPos.x - ext, shadowPos.y - ext);
            ImVec2 pmax = ImVec2(shadowPos.x + size.x + ext, shadowPos.y + size.y + ext);
            float cornerRounding = rounding + ext;

            drawList->AddRectFilled(pmin, pmax, color, cornerRounding, ImDrawFlags_RoundCornersAll);
        }
    }

    // 绘制 Card 背景：阴影 → 背景圆角矩形 → 边框描边（z-order 从底到顶）
    void DrawGuiCardBackground(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                               const GuiCardStyle& style)
    {
        if (drawList == nullptr)
        {
            return;
        }

        // 先绘制高斯软阴影（位于最底层）
        DrawGuiSoftShadow(drawList, pos, size, style.shadow, style.radius);

        // 绘制背景圆角矩形（alpha 受整体透明度影响）
        GuiColor bg = style.backgroundColor;
        bg.a *= style.opacity;
        drawList->AddRectFilled(pos, pos + size, bg.ToImU32(), style.radius, ImDrawFlags_RoundCornersAll);

        // 绘制边框描边（宽度 > 0 时）
        if (style.borderWidth > 0.0f)
        {
            GuiColor border = style.borderColor;
            border.a = style.borderAlpha * style.opacity;
            drawList->AddRect(pos, pos + size, border.ToImU32(), style.radius, ImDrawFlags_RoundCornersAll, style.borderWidth);
        }
    }

    // 绘制渐变矩形：水平 / 垂直线性渐变
    // 注意：AddRectFilledMultiColor 不支持圆角参数，rounding 当前不生效
    void DrawGuiGradientRect(ImDrawList* drawList, ImVec2 pos, ImVec2 size,
                             GuiColor colorStart, GuiColor colorEnd,
                             bool horizontal, float rounding)
    {
        if (drawList == nullptr)
        {
            return;
        }

        ImVec2 pmax = ImVec2(pos.x + size.x, pos.y + size.y);
        ImU32 colStart = colorStart.ToImU32();
        ImU32 colEnd = colorEnd.ToImU32();

        if (horizontal)
        {
            // 水平渐变：左 = colorStart，右 = colorEnd
            drawList->AddRectFilledMultiColor(pos, pmax,
                colStart,
                colEnd,
                colEnd,
                colStart);
        }
        else
        {
            // 垂直渐变：上 = colorStart，下 = colorEnd
            drawList->AddRectFilledMultiColor(pos, pmax,
                colStart,
                colStart,
                colEnd,
                colEnd);
        }
        (void)rounding;
    }

    // 开始 Card 内容区：设置光标到内容区左上角，并裁剪到内容区内
    void BeginGuiCardContent(ImVec2 pos, ImVec2 size, float padding)
    {
        ImVec2 contentPos = ImVec2(pos.x + padding, pos.y + padding);
        ImVec2 contentSize = ImVec2(size.x - padding * 2.0f, size.y - padding * 2.0f);

        ImGui::SetCursorScreenPos(contentPos);
        ImGui::PushClipRect(contentPos, ImVec2(contentPos.x + contentSize.x, contentPos.y + contentSize.y), true);
    }

    // 结束 Card 内容区：恢复裁剪状态
    void EndGuiCardContent()
    {
        ImGui::PopClipRect();
    }
}
