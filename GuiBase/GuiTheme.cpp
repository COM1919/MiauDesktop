// GuiTheme.cpp
// 主题令牌系统的实现（声明见 BaseUI/GUIBase.h）
// 移植自 EUI-NEO components/theme.h，配色与视觉层级保持一致

#include "GUIBase.h"

namespace GuiBase
{
    // 根据阴影分级与主题模式获取阴影样式
    // 暗色主题使用纯黑阴影，亮色主题使用带蓝调的深色阴影
    GuiShadowStyle GetShadowStyle(GuiShadowLevel level, bool isDark)
    {
        GuiShadowStyle style;
        style.offsetX = 0.0f;

        switch (level)
        {
        case GuiShadowLevel::Button:
            style.blur = 14.0f;
            style.offsetY = 4.0f;
            style.color = isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.22f)
                                 : GuiColor(0.10f, 0.14f, 0.22f, 0.10f);
            break;
        case GuiShadowLevel::Chart:
            style.blur = 18.0f;
            style.offsetY = 4.0f;
            style.color = isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.20f)
                                 : GuiColor(0.10f, 0.14f, 0.22f, 0.10f);
            break;
        case GuiShadowLevel::Panel:
            style.blur = 24.0f;
            style.offsetY = 8.0f;
            style.color = isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.28f)
                                 : GuiColor(0.10f, 0.14f, 0.22f, 0.12f);
            break;
        case GuiShadowLevel::Dialog:
            style.blur = 30.0f;
            style.offsetY = 16.0f;
            style.color = isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.28f)
                                 : GuiColor(0.10f, 0.14f, 0.22f, 0.14f);
            break;
        case GuiShadowLevel::Glow:
            style.blur = 44.0f;
            style.offsetY = 0.0f;
            style.color = isDark ? GuiColor(0.0f, 0.0f, 0.0f, 0.42f)
                                 : GuiColor(0.10f, 0.14f, 0.22f, 0.26f);
            break;
        }
        return style;
    }

    namespace Theme
    {
        // 暗色主题：深色背景 + 白色文字，主色 #3871E0
        GuiThemeTokens Dark()
        {
            GuiThemeTokens t;
            t.background    = GuiColor(0.10f, 0.10f, 0.12f, 1.0f);
            t.primary       = GuiColor(0.22f, 0.44f, 0.88f, 1.0f);
            t.surface       = GuiColor(0.15f, 0.15f, 0.18f, 1.0f);
            t.surfaceHover  = GuiColor(0.25f, 0.25f, 0.28f, 1.0f);
            t.surfaceActive = GuiColor(0.35f, 0.35f, 0.38f, 1.0f);
            t.text          = GuiColor(1.00f, 1.00f, 1.00f, 1.0f);
            t.border        = GuiColor(0.30f, 0.30f, 0.30f, 1.0f);
            t.isDark        = true;
            return t;
        }

        // 亮色主题：浅色背景 + 黑色文字，主色与暗色保持一致
        GuiThemeTokens Light()
        {
            GuiThemeTokens t;
            t.background    = GuiColor(0.95f, 0.95f, 0.97f, 1.0f);
            t.primary       = GuiColor(0.22f, 0.44f, 0.88f, 1.0f);
            t.surface       = GuiColor(1.00f, 1.00f, 1.00f, 1.0f);
            t.surfaceHover  = GuiColor(0.90f, 0.90f, 0.90f, 1.0f);
            t.surfaceActive = GuiColor(0.80f, 0.80f, 0.80f, 1.0f);
            t.text          = GuiColor(0.00f, 0.00f, 0.00f, 1.0f);
            t.border        = GuiColor(0.80f, 0.80f, 0.80f, 1.0f);
            t.isDark        = false;
            return t;
        }
    }
}
