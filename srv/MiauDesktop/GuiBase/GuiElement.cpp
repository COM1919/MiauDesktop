// GuiElement.cpp
// 基础元素（文本 / 图片 / 鼠标区域）的实现（声明见 GuiBase.h）
// 交互一律由 InvisibleButton / Dummy 承担，外观全部由 ImDrawList 自绘；
// 文本换行按 UTF-8 逐码点累积宽度，空格与 CJK 字符处可断行，绝不切断多字节序列。
// 移植自 EUI-NEO components/text.h / image.h / mousearea.h

#include "GUIBase.h"

#include <cfloat>
#include <cstdio>
#include <cstring>

namespace GuiBase
{
    namespace
    {
        // =========================================================================
        // 文本布局辅助
        // =========================================================================

        constexpr int kMaxTextLines = 64;     // 单次布局的最大行数，防极端输入死循环
        constexpr float kMinWrapWidth = 1.0f; // 软换行的最小宽度，避免除零与零宽死循环

        // 文本行：绘制区间 [begin, end) 与该区间累积出的像素宽度
        struct GuiTextLine
        {
            const char* begin = nullptr;
            const char* end = nullptr;
            float width = 0.0f;
        };

        // 文本块布局：行表 + 块宽（各行宽度的最大值）
        struct GuiTextLayout
        {
            GuiTextLine lines[kMaxTextLines];
            int lineCount = 0;
            float blockWidth = 0.0f;
        };

        // 判断码点是否可在其后直接断行（CJK 汉字 / 假名 / 谚文 / 全角标点）
        bool IsWrapAnyChar(unsigned int codepoint)
        {
            return (codepoint >= 0x1100u && codepoint <= 0x11FFu) || // 谚文字母
                   (codepoint >= 0x2E80u && codepoint <= 0x303Fu) || // CJK 部首补充 + CJK 符号标点
                   (codepoint >= 0x3041u && codepoint <= 0x33FFu) || // 假名 + 注音 + CJK 兼容
                   (codepoint >= 0x3400u && codepoint <= 0x4DBFu) || // CJK 扩展 A
                   (codepoint >= 0x4E00u && codepoint <= 0x9FFFu) || // CJK 统一表意文字
                   (codepoint >= 0xA000u && codepoint <= 0xA4CFu) || // 彝文
                   (codepoint >= 0xAC00u && codepoint <= 0xD7A3u) || // 谚文音节
                   (codepoint >= 0xF900u && codepoint <= 0xFAFFu) || // CJK 兼容表意文字
                   (codepoint >= 0xFE10u && codepoint <= 0xFE19u) || // 竖排标点
                   (codepoint >= 0xFE30u && codepoint <= 0xFE6Fu) || // CJK 兼容形式
                   (codepoint >= 0xFF00u && codepoint <= 0xFF60u) || // 全角形式
                   (codepoint >= 0xFFE0u && codepoint <= 0xFFE6u);   // 全角符号
        }

        // 单个码点的像素推进量：优先取已烘焙字形的 advance（与 ImDrawList::AddText 一致）
        // baked 为空（字体尚未烘焙）时回退到官方测量接口，保证宽度仍可用
        float TextCharAdvance(ImFont* font, float fontSize, ImFontBaked* baked, float bakedScale,
                              unsigned int codepoint, const char* charBegin, const char* charEnd)
        {
            if (baked != nullptr)
            {
                const ImWchar safeCodepoint =
                    (codepoint <= static_cast<unsigned int>(IM_UNICODE_CODEPOINT_MAX))
                        ? static_cast<ImWchar>(codepoint)
                        : static_cast<ImWchar>(IM_UNICODE_CODEPOINT_INVALID);
                return baked->GetCharAdvance(safeCodepoint) * bakedScale;
            }

            if (font == nullptr)
            {
                return 0.0f;
            }
            return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, charBegin, charEnd).x;
        }

        // 单遍扫描生成换行布局：'\n' 直接断行；软换行只在空格后或 CJK 字符后断开，
        // 长英文单词无可断点时按字符强制断行。累积宽度即行宽，不需要二次测量。
        void BuildTextLayout(ImFont* font, const char* text, float fontSize, float wrapWidth,
                             GuiTextLayout& layout)
        {
            layout.lineCount = 0;
            layout.blockWidth = 0.0f;
            if (font == nullptr || text == nullptr || text[0] == '\0')
            {
                return;
            }

            // 字形的烘焙尺寸可能被取整，需按比例换算到调用方要求的像素字号
            ImFontBaked* baked = font->GetFontBaked(fontSize);
            const float bakedScale = (baked != nullptr && baked->Size > 0.0f)
                                         ? (fontSize / baked->Size)
                                         : 1.0f;

            // 记录一行：行数达到上限后丢弃后续行（上限仅用于防极端输入）
            auto emitLine = [&layout](const char* begin, const char* end, float width)
            {
                if (layout.lineCount >= kMaxTextLines)
                {
                    return;
                }
                GuiTextLine& line = layout.lines[layout.lineCount];
                line.begin = begin;
                line.end = end;
                line.width = width;
                layout.blockWidth = std::max(layout.blockWidth, width);
                ++layout.lineCount;
            };

            const bool wrapEnabled = wrapWidth > 0.0f;
            const float wrapLimit = wrapEnabled ? std::max(wrapWidth, kMinWrapWidth) : 0.0f;

            const char* scan = text;
            const char* lineBegin = text;
            const char* lastBreak = nullptr; // 上一个可断行位置（字符之后的指针）
            float lineWidth = 0.0f;
            float lastBreakWidth = 0.0f;
            bool wrapActive = wrapEnabled;

            while (*scan != '\0' && layout.lineCount < kMaxTextLines)
            {
                // 只剩最后一行容量时关闭软换行，让末行吞掉剩余文本，避免丢字
                if (layout.lineCount >= kMaxTextLines - 1)
                {
                    wrapActive = false;
                }

                if (*scan == '\n')
                {
                    emitLine(lineBegin, scan, lineWidth);
                    ++scan;
                    lineBegin = scan;
                    lastBreak = nullptr;
                    lineWidth = 0.0f;
                    lastBreakWidth = 0.0f;
                    continue;
                }
                if (*scan == '\r')
                {
                    ++scan;
                    continue;
                }

                // 解码一个完整码点（多字节序列整体前进，绝不从中间切断）
                const char* charBegin = scan;
                unsigned int codepoint = 0;
                const int charBytes = ImTextCharFromUtf8(&codepoint, scan, nullptr);
                if (charBytes <= 0)
                {
                    break; // 遇到 '\0' 或不可解码的终止情形
                }
                const char* charEnd = charBegin + charBytes;

                if (codepoint == static_cast<unsigned int>(IM_UNICODE_CODEPOINT_INVALID))
                {
                    scan = charEnd; // 非法码点跳过，不参与测量也不绘制
                    continue;
                }

                const float advance = TextCharAdvance(font, fontSize, baked, bakedScale,
                                                      codepoint, charBegin, charEnd);
                const float nextWidth = lineWidth + advance;

                // 软换行：仅当当前行已有内容时才断开，保证循环必定前进
                if (wrapActive && nextWidth > wrapLimit && charBegin > lineBegin)
                {
                    if (lastBreak != nullptr && lastBreak > lineBegin)
                    {
                        emitLine(lineBegin, lastBreak, lastBreakWidth);
                        scan = lastBreak;
                    }
                    else
                    {
                        emitLine(lineBegin, charBegin, lineWidth);
                        scan = charBegin;
                    }
                    lineBegin = scan;
                    lineWidth = 0.0f;
                    lastBreak = nullptr;
                    lastBreakWidth = 0.0f;
                    continue;
                }

                scan = charEnd;
                lineWidth = nextWidth;

                // 更新可断行点：空格 / 制表符之后，或 CJK 类字符之后
                if (codepoint == static_cast<unsigned int>(' ') ||
                    codepoint == static_cast<unsigned int>('\t') ||
                    IsWrapAnyChar(codepoint))
                {
                    lastBreak = scan;
                    lastBreakWidth = lineWidth;
                }
            }

            // 末尾剩余内容（含尾部硬换行后的残余）作为最后一行输出
            if (lineBegin < scan)
            {
                emitLine(lineBegin, scan, lineWidth);
            }
        }

        // 解析字号与行高：<=0 的入参按令牌补默认值
        void ResolveTextMetrics(const GuiThemeTokens& tokens, float fontSize, float lineHeight,
                                float& outFontSize, float& outLineHeight)
        {
            outFontSize = (fontSize > 0.0f) ? fontSize : ImGui::GetFontSize();
            if (outFontSize <= 0.0f)
            {
                outFontSize = tokens.typography.body;
            }

            outLineHeight = (lineHeight > 0.0f) ? lineHeight
                                                : (outFontSize + tokens.typography.lineGap);
            if (outLineHeight <= 0.0f)
            {
                outLineHeight = outFontSize;
            }
        }
    }

    // =========================================================================
    // 文本测量
    // 只测量不绘制：按 UTF-8 逐码点累积宽度并完成换行，返回换行后的实际占用尺寸
    // =========================================================================
    ImVec2 GuiTextMeasure(const char* text, float fontSize, float maxWidth, float lineHeight)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return ImVec2(0.0f, 0.0f);
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        ImFont* font = ImGui::GetFont();
        if (font == nullptr)
        {
            return ImVec2(0.0f, 0.0f);
        }

        float useFontSize = 0.0f;
        float useLineHeight = 0.0f;
        ResolveTextMetrics(tokens, fontSize, lineHeight, useFontSize, useLineHeight);

        GuiTextLayout layout;
        BuildTextLayout(font, text, useFontSize, maxWidth, layout);
        if (layout.lineCount <= 0)
        {
            return ImVec2(0.0f, 0.0f);
        }

        return ImVec2(layout.blockWidth, useLineHeight * static_cast<float>(layout.lineCount));
    }

    // =========================================================================
    // 文本绘制
    // 复用同一份换行布局：水平对齐按可用宽度计算，逐行按 lineHeight 递进，
    // 绘制结束后把光标推进到文本块下方
    // =========================================================================
    ImVec2 GuiText(const char* label, const char* text, float fontSize, GuiColor color,
                   GuiTextAlign align, float maxWidth, float lineHeight)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return ImVec2(0.0f, 0.0f);
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        ImFont* font = ImGui::GetFont();
        if (font == nullptr)
        {
            return ImVec2(0.0f, 0.0f);
        }

        float useFontSize = 0.0f;
        float useLineHeight = 0.0f;
        ResolveTextMetrics(tokens, fontSize, lineHeight, useFontSize, useLineHeight);

        // 单遍布局，绘制与测量复用同一结果（同一文本每帧只测一次）
        GuiTextLayout layout;
        BuildTextLayout(font, text, useFontSize, maxWidth, layout);
        const ImVec2 blockSize(layout.blockWidth,
                               useLineHeight * static_cast<float>(layout.lineCount));
        if (layout.lineCount <= 0)
        {
            return blockSize;
        }

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        // 可用宽度：显式 maxWidth 优先，否则取当前内容区剩余宽度
        const float availWidth = (maxWidth > 0.0f) ? maxWidth : ImGui::GetContentRegionAvail().x;
        const ImU32 textColor = (color.a > 0.0f) ? color.ToImU32() : tokens.text.ToImU32();

        ImGui::PushID(label != nullptr ? label : "##gui_text");

        // 占位推进光标：左对齐按实际块宽，居中/右对齐按可用宽度占满，保证对齐基准稳定
        const float advanceWidth = (align == GuiTextAlign::Left || maxWidth <= 0.0f)
                                       ? layout.blockWidth
                                       : maxWidth;
        ImGui::Dummy(ImVec2(advanceWidth, blockSize.y));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (int index = 0; index < layout.lineCount; ++index)
        {
            const GuiTextLine& line = layout.lines[index];

            float x = origin.x;
            if (align == GuiTextAlign::Center)
            {
                x = origin.x + (availWidth - line.width) * 0.5f;
            }
            else if (align == GuiTextAlign::Right)
            {
                x = origin.x + availWidth - line.width;
            }
            x = std::max(origin.x, x); // 行宽超出可用宽度时不反向溢出到左侧

            const float y = origin.y + static_cast<float>(index) * useLineHeight;
            drawList->AddText(font, useFontSize, ImVec2(x, y), textColor, line.begin, line.end);
        }

        ImGui::PopID();
        return blockSize;
    }

    // =========================================================================
    // 图片
    // 有纹理时按圆角裁剪绘制，并把 tint 与 opacity 相乘得到最终 alpha；
    // 纹理为空时绘制 surface 底色 + border 描边的圆角占位块与居中图标
    // =========================================================================
    void GuiImage(const char* label, Texture2D texture, const ImVec2& size,
                  GuiColor tint, float rounding, float opacity,
                  const ImVec2& uv0, const ImVec2& uv1)
    {
        float width = size.x;
        float height = size.y;
        if (width <= 0.0f || height <= 0.0f)
        {
            const float availWidth = ImGui::GetContentRegionAvail().x;
            if (width <= 0.0f)
            {
                width = (availWidth > 0.0f) ? availWidth : 0.0f;
            }
            if (height <= 0.0f)
            {
                height = width; // 未给高度时按正方形处理
            }
        }
        if (width <= 0.0f || height <= 0.0f)
        {
            return;
        }

        const GuiThemeTokens tokens = Theme::Dark(); // 每帧仅取一次令牌
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + width, pos.y + height);
        const float useRounding = std::max(0.0f, rounding);
        const float useOpacity = std::clamp(opacity, 0.0f, 1.0f);

        // tint 与 opacity 相乘得到最终 alpha
        GuiColor useTint = tint;
        useTint.a = std::clamp(useTint.a * useOpacity, 0.0f, 1.0f);
        const ImU32 tintU32 = useTint.ToImU32();

        // UV 非法（空区间）时回退到整张纹理
        ImVec2 useUv0 = uv0;
        ImVec2 useUv1 = uv1;
        if (useUv1.x <= useUv0.x || useUv1.y <= useUv0.y)
        {
            useUv0 = ImVec2(0.0f, 0.0f);
            useUv1 = ImVec2(1.0f, 1.0f);
        }

        ImGui::PushID(label != nullptr ? label : "##gui_image");
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        if (texture != nullptr)
        {
            if ((tintU32 & IM_COL32_A_MASK) != 0u)
            {
                drawList->AddImageRounded((ImTextureID)texture, pos, maxPos, useUv0, useUv1,
                                          tintU32, useRounding);
            }
        }
        else
        {
            // 占位：surface 底色 + border 描边 + 居中图片图标
            drawList->AddRectFilled(pos, maxPos, tokens.surface.ToImU32(), useRounding);
            drawList->AddRect(pos, maxPos,
                              WithAlpha(tokens.border, 0.72f * useOpacity).ToImU32(),
                              useRounding, ImDrawFlags_RoundCornersAll, 1.0f);

            ImFont* font = ImGui::GetFont();
            if (font != nullptr && useOpacity > 0.0f)
            {
                const float iconSize = std::max(12.0f, std::min(width, height) * 0.34f);
                const ImVec2 iconTextSize =
                    font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, ICON_FA_IMAGE);
                const ImVec2 iconPos(pos.x + (width - iconTextSize.x) * 0.5f,
                                     pos.y + (height - iconTextSize.y) * 0.5f);
                drawList->AddText(font, iconSize, iconPos,
                                  WithAlpha(tokens.text, 0.55f * useOpacity).ToImU32(),
                                  ICON_FA_IMAGE);
            }
        }

        // 占位推进光标（图片本身无交互）
        ImGui::Dummy(ImVec2(width, height));
        ImGui::PopID();
    }

    // =========================================================================
    // 鼠标区域
    // InvisibleButton 承担命中检测，可选背景在 hover 时向白色混合 0.10；
    // hover 且未禁用时设置鼠标指针与 tooltip，点击且未禁用时触发回调
    // =========================================================================
    bool GuiMouseArea(const char* label, const ImVec2& size,
                      const GuiMouseAreaOptions& options,
                      const std::function<void()>& onClick)
    {
        if (size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
        const float rounding = std::max(0.0f, options.rounding);

        ImGui::PushID(label != nullptr ? label : "##gui_mouse_area");

        const bool pressed = ImGui::InvisibleButton("##gui_mouse_area_hit", size);
        // 禁用态不响应 hover / 指针 / 点击（InvisibleButton 仍占位，避免布局跳动）
        const bool hovered = (!options.disabled) && ImGui::IsItemHovered();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (options.color.a > 0.0f)
        {
            GuiColor background = options.color;
            if (hovered)
            {
                background = MixColor(background, GuiColor(1.0f, 1.0f, 1.0f, background.a), 0.10f);
            }
            drawList->AddRectFilled(pos, maxPos, background.ToImU32(), rounding);
        }

        if (hovered)
        {
            ImGui::SetMouseCursor(options.cursor);
            if (options.tooltip != nullptr && options.tooltip[0] != '\0')
            {
                GuiTooltip(options.tooltip);
            }
        }

        bool clicked = false;
        if (pressed && !options.disabled)
        {
            if (onClick)
            {
                onClick();
            }
            clicked = true;
        }

        // === 事件族（mousearea.h:58-81）===
        // 按下 / 抬起 / 每帧悬浮 / 拖拽累计位移 / 滚轮
        const bool activated = (!options.disabled) && ImGui::IsItemActivated();
        const bool deactivated = (!options.disabled) && ImGui::IsItemDeactivated();
        const bool itemActive = (!options.disabled) && ImGui::IsItemActive();

        if (activated && options.onPress)
        {
            options.onPress();
        }
        if (deactivated && options.onRelease)
        {
            options.onRelease();
        }
        if (hovered && options.onHover)
        {
            options.onHover();
        }

        // 拖拽：累计位移存 ImGuiStorage，抬起时清零
        if (options.onDrag)
        {
            const ImGuiID dragKey = ImGui::GetID("##mouse_area_drag");
            if (activated)
            {
                ImGui::GetStateStorage()->SetFloat(dragKey, 0.0f);
            }
            if (itemActive)
            {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                const float total = ImGui::GetStateStorage()->GetFloat(dragKey, 0.0f) +
                                    delta.x + delta.y;
                ImGui::GetStateStorage()->SetFloat(dragKey, total);
                if (delta.x != 0.0f || delta.y != 0.0f)
                {
                    options.onDrag(delta.x, delta.y);
                }
            }
        }

        // 滚轮：仅在悬浮时上报
        if (hovered && options.onScroll)
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                options.onScroll(wheel);
            }
        }

        // 右键（acceptedButtons）：InvisibleButton 只处理左键，右键单独判定
        if (options.acceptRightButton && hovered && !options.disabled &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            if (onClick)
            {
                onClick();
            }
            clicked = true;
        }

        ImGui::PopID();
        return clicked;
    }

    // =========================================================================
    // 文本层级（对应 text.h:13/21/29 的 body / title / subtitle 三档样式）
    // 复用 GuiText，只是把字号与颜色固定为 EUI pageVisuals 的层级值
    // =========================================================================
    void GuiTextBody(const char* label, const char* text, float maxWidth)
    {
        const GuiThemeTokens tokens = Theme::Dark();
        GuiText(label, text, tokens.typography.label, WithAlpha(tokens.text, 0.68f),
                GuiTextAlign::Left, maxWidth);
    }

    void GuiTextTitle(const char* label, const char* text, float maxWidth)
    {
        const GuiThemeTokens tokens = Theme::Dark();
        GuiText(label, text, tokens.typography.display, WithAlpha(tokens.text, 0.98f),
                GuiTextAlign::Left, maxWidth);
    }

    void GuiTextSubtitle(const char* label, const char* text, float maxWidth)
    {
        const GuiThemeTokens tokens = Theme::Dark();
        GuiText(label, text, tokens.typography.subtitle, WithAlpha(tokens.text, 0.72f),
                GuiTextAlign::Left, maxWidth);
    }

    // =========================================================================
    // Markdown 文档控件
    // Card 背景 + 滚动视口 + imgui_md 渲染：标题层级由 imgui_md::get_font_size 提供，
    // 走 ImGui 1.92 的动态字号烘焙（单一字体文件），不加载多份字号字体
    // =========================================================================
    bool GuiMarkdown(const char* label, const char* markdownText, float* offset,
                     const ImVec2& size, const GuiCardStyle& style)
    {
        if (markdownText == nullptr || size.x <= 0.0f || size.y <= 0.0f)
        {
            return false;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Card 背景：阴影 → 填充 → 边框
        DrawGuiCardBackground(drawList, pos, size, style);

        const float padding = std::max(0.0f, style.padding);
        const ImVec2 contentPos(pos.x + padding, pos.y + padding);
        const ImVec2 contentSize(std::max(0.0f, size.x - padding * 2.0f),
                                 std::max(0.0f, size.y - padding * 2.0f));

        ImGui::PushID(label != nullptr ? label : "##gui_markdown");
        if (contentSize.x > 8.0f && contentSize.y > 8.0f)
        {
            ImGui::SetCursorScreenPos(contentPos);
            float localOffset = (offset != nullptr) ? *offset : 0.0f;
            if (BeginGuiScrollView("##markdown_scroll", contentSize, &localOffset))
            {
                // 单个 imgui_md 实例复用：解析状态在 print 内部重置，不产生跨帧累积分配
                static imgui_md s_markdown;
                ImGui::PushTextWrapPos(contentSize.x - 8.0f);
                s_markdown.print(markdownText, markdownText + std::strlen(markdownText));
                ImGui::PopTextWrapPos();
            }
            EndGuiScrollView();
            if (offset != nullptr)
            {
                *offset = localOffset;
            }
        }

        // 推进光标到卡片下方
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
        ImGui::PopID();
        return true;
    }
}
