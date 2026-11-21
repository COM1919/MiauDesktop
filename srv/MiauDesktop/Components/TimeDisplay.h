#pragma once
#include "../Framework.h"
#include "../Component.h"
#include "../GuiBase/GuiBase.h"

#include <cfloat>
#include <cstdio>
#include <ctime>

// =============================================================================
// TimeDisplay：时间显示组件
//
// 内容层级（自上而下）：
//   1. 主时间 HH:MM —— 唯一的主信息，最大字号、最高对比度
//   2. 秒 / 上午下午 —— 主字号的派生小字，底部基线对齐，降低对比度
//   3. 日期行 —— 次级文字色，字号随主字号缩放但夹在可读区间
//   4. 当天进度条 —— 低对比度装饰，悬停时才给出百分比
//
// 布局约束：
//   · 辅助字号由主字号派生，不低于 12px（界面文字的可读性下限）
//   · 冒号闪烁只改变透明度、不改变宽度，避免每秒布局跳动
//   · 所有行共用同一内容块宽度，整体可按设置居左 / 居中 / 居右
//   · 窗口尺寸由字号与内容反推（禁止手动缩放），改字号后自动适配
// =============================================================================
class TimeDisplay : public Component
{
public:
    // =========================================================================
    // 配置项对应的成员（默认值与 config.AddItem 保持一致）
    // =========================================================================
    int m_fontSize = 56;            // 主时间字号（px）
    bool m_format24Hour = true;     // 24 小时制；关闭后显示 12 小时制 + 上午/下午
    bool m_showSeconds = false;     // 显示秒（次级字号）
    bool m_blinkColon = true;       // 冒号呼吸闪烁
    bool m_showDate = true;         // 显示日期行
    bool m_showWeekday = true;      // 日期行追加星期
    bool m_showProgress = true;     // 底部当天进度条
    bool m_accentTime = false;      // 时间使用主题主色（默认高对比文字色）
    bool m_useUtc = false;          // 使用 UTC 时间（关闭时为本地时间）
    int m_dateFormat = 0;           // 0: YYYY-MM-DD  1: YYYY/MM/DD  2: MM月DD日
    int m_alignment = 1;            // 0: 居左  1: 居中  2: 居右
    int m_timezoneOffset = 0;       // 在选定时间基础上平移的小时数

    TimeDisplay() : Component("时间显示", 0, true)
    {
        windowFlags |= ImGuiWindowFlags_NoResize;
        windowFlags |= ImGuiWindowFlags_NoScrollbar;
        windowFlags |= ImGuiWindowFlags_NoScrollWithMouse;
        windowFlags |= ImGuiWindowFlags_NoTitleBar;
        windowFlags |= ImGuiWindowFlags_NoMove;

        // 注册组件特有配置项（自动挂在 "时间显示" 根节点下）
        config.AddItem("FontSize", 56);
        config.AddItem("Format24Hour", true);
        config.AddItem("ShowSeconds", false);
        config.AddItem("BlinkColon", true);
        config.AddItem("ShowDate", true);
        config.AddItem("ShowWeekday", true);
        config.AddItem("ShowProgress", true);
        config.AddItem("AccentTime", false);
        config.AddItem("UseUtc", false);
        config.AddItem("DateFormat", 0);
        config.AddItem("Alignment", 1);
        config.AddItem("TimezoneOffset", 0);

        UpdateWindowSize();
    }

    const char* GetIcon() const override { return ICON_FA_CLOCK; }
    const char* GetDescription() const override { return "可调字号、格式与对齐方式的时间显示"; }

    // 配置加载：基础配置已由基类处理，这里只读自己的特有配置项
    void OnLoadConfig() override
    {
        m_fontSize = std::clamp(config.Get<int>("FontSize"), kMinFontSize, kMaxFontSize);
        m_format24Hour = config.Get<bool>("Format24Hour");
        m_showSeconds = config.Get<bool>("ShowSeconds");
        m_blinkColon = config.Get<bool>("BlinkColon");
        m_showDate = config.Get<bool>("ShowDate");
        m_showWeekday = config.Get<bool>("ShowWeekday");
        m_showProgress = config.Get<bool>("ShowProgress");
        m_accentTime = config.Get<bool>("AccentTime");
        m_useUtc = config.Get<bool>("UseUtc");
        m_dateFormat = std::clamp(config.Get<int>("DateFormat"), 0, 2);
        m_alignment = std::clamp(config.Get<int>("Alignment"), 0, 2);
        m_timezoneOffset = std::clamp(config.Get<int>("TimezoneOffset"), kMinTimezoneOffset, kMaxTimezoneOffset);

        // 字号 / 日期行 / 进度条共同决定窗口尺寸，配置变更后立即重新测量
        UpdateWindowSize();
    }

    void OnSaveConfig() override
    {
        config.Set("FontSize", m_fontSize);
        config.Set("Format24Hour", m_format24Hour);
        config.Set("ShowSeconds", m_showSeconds);
        config.Set("BlinkColon", m_blinkColon);
        config.Set("ShowDate", m_showDate);
        config.Set("ShowWeekday", m_showWeekday);
        config.Set("ShowProgress", m_showProgress);
        config.Set("AccentTime", m_accentTime);
        config.Set("UseUtc", m_useUtc);
        config.Set("DateFormat", m_dateFormat);
        config.Set("Alignment", m_alignment);
        config.Set("TimezoneOffset", m_timezoneOffset);
    }

protected:
    void CollectSettings(std::vector<Miau::Settings::SettingItem>& items) override
    {
        items.push_back(MakeSlider("FontSize", "时间字号", "时间",
                                   static_cast<double>(m_fontSize), kMinFontSize, kMaxFontSize, 1.0, 0, " px",
                                   "主时间文字的字号，秒、上午下午与日期会按比例缩放"));
        items.push_back(MakeToggle("Format24Hour", "24 小时制", "时间", m_format24Hour,
                                   "关闭后使用 12 小时制，并在时间右侧显示上午 / 下午"));
        items.push_back(MakeToggle("ShowSeconds", "显示秒", "时间", m_showSeconds,
                                   "秒以次级字号显示，不抢主时间的层级"));
        items.push_back(MakeToggle("BlinkColon", "冒号呼吸", "时间", m_blinkColon,
                                   "冒号每半秒降低一次对比度，闪烁不改变文字宽度"));
        items.push_back(MakeToggle("UseUtc", "使用 UTC", "时间", m_useUtc,
                                   "开启后显示 UTC 时间，关闭时显示本地时间"));
        items.push_back(MakeSlider("TimezoneOffset", "时区偏移", "时间",
                                   static_cast<double>(m_timezoneOffset),
                                   kMinTimezoneOffset, kMaxTimezoneOffset, 1.0, 0, " 小时",
                                   "在选定时间基础上平移的小时数，用于查看其他城市的时间"));

        items.push_back(MakeToggle("ShowDate", "显示日期", "日期", m_showDate,
                                   "在主时间下方显示日期行"));
        items.push_back(MakeChoice("DateFormat", "日期格式", "日期",
                                   { "YYYY-MM-DD", "YYYY/MM/DD", "MM月DD日" }, m_dateFormat,
                                   "日期的显示格式"));
        items.push_back(MakeToggle("ShowWeekday", "显示星期", "日期", m_showWeekday,
                                   "在日期后追加星期，例如“11月21日 · 星期五”"));

        items.push_back(MakeChoice("Alignment", "对齐方式", "外观",
                                   { "居左", "居中", "居右" }, m_alignment,
                                   "时间、日期与进度条在组件内的水平对齐"));
        items.push_back(MakeToggle("AccentTime", "时间使用主色", "外观", m_accentTime,
                                   "开启后时间改用主题主色，默认使用高对比文字色"));
        items.push_back(MakeToggle("ShowProgress", "当天进度条", "外观", m_showProgress,
                                   "底部细条表示今天已过去的比例，鼠标悬停显示百分比"));
    }

    void AfterBegin() override
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPaddingX, kPaddingY));
    }

    void ComponentRender() override
    {
        ImGui::PopStyleVar();   // AfterBegin 的 WindowPadding

        ImFont* font = ImGui::GetFont();
        if (font == nullptr)
            return;

        // 窗口尺寸完全由字号推算（见 UpdateWindowSize）：这里不再重设，
        // 否则 Component::Draw 末尾用当前帧实际尺寸回写 m_windowSize，
        // 会把刚算好的尺寸冲掉，窗口被永久卡在 ImGui 最小尺寸。

        const GuiBase::GuiThemeTokens tokens = GuiBase::Theme::Dark();
        const GuiBase::GuiColor timeColor = TimeColor(tokens);
        const ImU32 timeU32 = timeColor.ToImU32();
        const ImU32 subColor = GuiBase::WithAlpha(tokens.text, 0.62f).ToImU32();

        const TimeParts now = CurrentTime();
        const float smallFontSize = SmallFontSize();
        const float periodFontSize = PeriodFontSize();
        const float dateFontSize = DateFontSize();

        // 主时间拆成 时 / 冒号 / 分 三段，便于只让冒号参与闪烁
        const std::string hourText = TwoDigits(m_format24Hour ? now.hour : To12Hour(now.hour));
        const std::string minuteText = TwoDigits(now.minute);
        const std::string secondText = m_showSeconds ? ":" + TwoDigits(now.second) : std::string();
        const std::string periodText = m_format24Hour ? std::string() : PeriodLabel(now.hour);

        const float hourWidth = TextWidth(font, static_cast<float>(m_fontSize), hourText);
        const float colonWidth = TextWidth(font, static_cast<float>(m_fontSize), ":");
        const float minuteWidth = TextWidth(font, static_cast<float>(m_fontSize), minuteText);
        const float secondWidth = TextWidth(font, smallFontSize, secondText);
        const float periodWidth = TextWidth(font, periodFontSize, periodText);

        const float timeLineWidth = hourWidth + colonWidth + minuteWidth +
                                    (secondText.empty() ? 0.0f : kSecondGap + secondWidth) +
                                    (periodText.empty() ? 0.0f : kPeriodGap + periodWidth);

        const std::string dateText = BuildDateText(now);
        const float dateWidth = TextWidth(font, dateFontSize, dateText);

        // 内容块宽度：所有行共用，保证居左 / 居中 / 居右时边缘一致
        const float blockWidth = std::max({ timeLineWidth,
                                            m_showDate ? dateWidth : 0.0f,
                                            m_showProgress ? kMinBarWidth : 0.0f,
                                            kMinBlockWidth });

        const ImVec2 contentPos = ImGui::GetCursorScreenPos();
        const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const float totalHeight = ContentHeight();
        const float top = contentPos.y + std::max(0.0f, (contentAvail.y - totalHeight) * 0.5f);
        const float left = AlignedX(contentPos.x, contentAvail.x, blockWidth);

        // 冒号呼吸：闪烁帧只降低对比度，不隐藏、不改宽度
        const bool colonDim = m_blinkColon && (static_cast<int>(ImGui::GetTime() * 2.0) % 2 == 0);
        const ImU32 colonColor = GuiBase::WithAlpha(timeColor, colonDim ? 0.22f : 1.0f).ToImU32();

        // ---- 主时间行：时 / 冒号 / 分，小字按基线对齐 ----
        const float mainAscent = AscentFor(font, static_cast<float>(m_fontSize));
        const float smallAscent = AscentFor(font, smallFontSize);
        const float periodAscent = AscentFor(font, periodFontSize);

        float cursorX = left;
        drawList->AddText(font, static_cast<float>(m_fontSize), ImVec2(cursorX, top), timeU32, hourText.c_str());
        cursorX += hourWidth;
        drawList->AddText(font, static_cast<float>(m_fontSize), ImVec2(cursorX, top), colonColor, ":");
        cursorX += colonWidth;
        drawList->AddText(font, static_cast<float>(m_fontSize), ImVec2(cursorX, top), timeU32, minuteText.c_str());
        cursorX += minuteWidth;

        if (!secondText.empty())
        {
            cursorX += kSecondGap;
            drawList->AddText(font, smallFontSize, ImVec2(cursorX, top + mainAscent - smallAscent),
                              subColor, secondText.c_str());
            cursorX += secondWidth;
        }
        if (!periodText.empty())
        {
            cursorX += kPeriodGap;
            drawList->AddText(font, periodFontSize, ImVec2(cursorX, top + mainAscent - periodAscent),
                              subColor, periodText.c_str());
        }

        // ---- 日期行：次级文字，字号随主字号缩放 ----
        float nextRowY = top + MainLineHeight();
        if (m_showDate && !dateText.empty())
        {
            const float dateTop = nextRowY + kDateGap;
            drawList->AddText(font, dateFontSize,
                              ImVec2(AlignedX(contentPos.x, contentAvail.x, dateWidth), dateTop),
                              subColor, dateText.c_str());
            nextRowY = dateTop + DateLineHeight();
        }

        // ---- 当天进度条：低对比度装饰 + 悬停百分比 ----
        if (m_showProgress)
        {
            const float barY = nextRowY + kBarGap;
            const float progress = DayProgress(now);

            ImGui::SetCursorScreenPos(ImVec2(left, barY));
            ImGui::InvisibleButton("##time_day_progress", ImVec2(blockWidth, kBarHeight));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("今天已过去 %d%%", static_cast<int>(progress * 100.0f + 0.5f));

            GuiBase::DrawGuiProgress(drawList, ImVec2(left, barY), ImVec2(blockWidth, kBarHeight),
                                     progress, tokens.primary, GuiBase::WithAlpha(tokens.text, 0.12f));
        }
    }

private:
    // =========================================================================
    // 尺寸令牌：与项目其它组件的窗口内边距 / 间距风格保持一致
    // =========================================================================
    static constexpr int kMinFontSize = 24;
    static constexpr int kMaxFontSize = 120;
    static constexpr int kMinTimezoneOffset = -12;
    static constexpr int kMaxTimezoneOffset = 14;

    static constexpr float kPaddingX = 18.0f;
    static constexpr float kPaddingY = 16.0f;
    static constexpr float kDateGap = 8.0f;
    static constexpr float kBarGap = 14.0f;
    static constexpr float kBarHeight = 4.0f;
    static constexpr float kSecondGap = 4.0f;
    static constexpr float kPeriodGap = 8.0f;
    static constexpr float kMinBarWidth = 96.0f;
    static constexpr float kMinBlockWidth = 48.0f;

    // =========================================================================
    // 时间模型
    // =========================================================================
    struct TimeParts
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        int weekday = 0;   // 0 = 星期日
    };

    TimeParts CurrentTime() const
    {
        // 先按偏移平移时间戳，再按本地 / UTC 解释，偏移即“相对选定时间的平移”
        std::time_t stamp = std::time(nullptr) + static_cast<std::time_t>(m_timezoneOffset) * 3600;
        std::tm local{};
        if (m_useUtc)
            gmtime_s(&local, &stamp);
        else
            localtime_s(&local, &stamp);

        TimeParts parts;
        parts.year = local.tm_year + 1900;
        parts.month = local.tm_mon + 1;
        parts.day = local.tm_mday;
        parts.hour = local.tm_hour;
        parts.minute = local.tm_min;
        parts.second = local.tm_sec;
        parts.weekday = local.tm_wday;
        return parts;
    }

    static int To12Hour(int hour24)
    {
        const int hour = hour24 % 12;
        return hour == 0 ? 12 : hour;
    }

    static const char* PeriodLabel(int hour24)
    {
        return hour24 < 12 ? "上午" : "下午";
    }

    static std::string TwoDigits(int value)
    {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%02d", value < 0 ? 0 : value);
        return std::string(buffer);
    }

    static std::string WeekdayLabel(int weekday)
    {
        static const char* const kLabels[] = { "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六" };
        if (weekday < 0 || weekday > 6)
            return std::string();
        return kLabels[weekday];
    }

    std::string BuildDateText(const TimeParts& now) const
    {
        char buffer[64];
        switch (m_dateFormat)
        {
        case 1:
            std::snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d", now.year, now.month, now.day);
            break;
        case 2:
            std::snprintf(buffer, sizeof(buffer), "%d月%d日", now.month, now.day);
            break;
        default:
            std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", now.year, now.month, now.day);
            break;
        }

        std::string text(buffer);
        if (m_showWeekday)
        {
            const std::string weekday = WeekdayLabel(now.weekday);
            if (!weekday.empty())
                text += " · " + weekday;
        }
        return text;
    }

    // 当天进度：0 表示刚过零点，1 表示接近午夜
    static float DayProgress(const TimeParts& now)
    {
        const float seconds = static_cast<float>(now.hour * 3600 + now.minute * 60 + now.second);
        return std::clamp(seconds / 86400.0f, 0.0f, 1.0f);
    }

    // =========================================================================
    // 字体度量
    // =========================================================================
    static float AscentFor(ImFont* font, float fontSize)
    {
        if (font == nullptr || fontSize <= 0.0f)
            return 0.0f;
        ImFontBaked* baked = font->GetFontBaked(fontSize);
        if (baked == nullptr || baked->Size <= 0.0f)
            return fontSize * 0.86f;
        // Ascent 是 baked 尺寸下的值；按请求字号等比换算，保证基线对齐不失真
        return baked->Ascent * (fontSize / baked->Size);
    }

    static float TextWidth(ImFont* font, float fontSize, const std::string& text)
    {
        if (font == nullptr || text.empty() || fontSize <= 0.0f)
            return 0.0f;
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()).x;
    }

    // 辅助字号：随主字号缩放，但保留可读下限
    float SmallFontSize() const
    {
        return std::clamp(static_cast<float>(m_fontSize) * 0.5f, 14.0f, static_cast<float>(m_fontSize) * 0.62f);
    }

    float PeriodFontSize() const
    {
        return std::clamp(static_cast<float>(m_fontSize) * 0.26f, 12.0f, 28.0f);
    }

    float DateFontSize() const
    {
        return std::clamp(static_cast<float>(m_fontSize) * 0.32f, 12.0f, 20.0f);
    }

    float MainLineHeight() const
    {
        return static_cast<float>(m_fontSize) * 1.12f;
    }

    float DateLineHeight() const
    {
        return DateFontSize() * 1.5f;
    }

    float ContentHeight() const
    {
        float height = MainLineHeight();
        if (m_showDate)
            height += kDateGap + DateLineHeight();
        if (m_showProgress)
            height += kBarGap + kBarHeight;
        return height;
    }

    GuiBase::GuiColor TimeColor(const GuiBase::GuiThemeTokens& tokens) const
    {
        return m_accentTime ? tokens.primary : GuiBase::WithAlpha(tokens.text, 0.98f);
    }

    // 按对齐方式计算某一行的起始 X（居中 / 居右时按行自身宽度摆放）
    float AlignedX(float contentLeft, float contentWidth, float lineWidth) const
    {
        const float slack = std::max(0.0f, contentWidth - lineWidth);
        switch (m_alignment)
        {
        case 0:
            return contentLeft;
        case 2:
            return contentLeft + slack;
        default:
            return contentLeft + slack * 0.5f;
        }
    }

    // =========================================================================
    // 窗口尺寸：由字号与当前显示项推算（禁止手动缩放，避免留白与裁切）
    // 注意：这里刻意不调用 ImGui 字体度量 —— 构造与配置加载发生在 NewFrame
    //       之前，此时取字体 baked 数据不可靠，会得出无效尺寸。
    //       绘制时仍按真实字体宽度排版，窗口宽度只是留足余量。
    // =========================================================================
    void UpdateWindowSize()
    {
        const float contentWidth = std::max({ EstimateTimeWidth(),
                                              m_showDate ? EstimateDateWidth() : 0.0f,
                                              m_showProgress ? kMinBarWidth : 0.0f,
                                              kMinBlockWidth });

        // 估算值放大 10%：不同字体字宽有偏差，宁可窗口略宽也不要裁掉字形
        SetWindowSize(ImVec2(contentWidth * 1.10f + kPaddingX * 2.0f + 2.0f,
                             ContentHeight() + kPaddingY * 2.0f + 2.0f));
    }

    // "00:00" 约 2.5em（数字 0.55em × 4 + 冒号 0.3em）
    float EstimateTimeWidth() const
    {
        const float main = static_cast<float>(m_fontSize);
        float width = main * 2.5f;
        if (m_showSeconds)
            width += kSecondGap + SmallFontSize() * 1.6f;    // ":00"
        if (!m_format24Hour)
            width += kPeriodGap + PeriodFontSize() * 2.1f;   // "上午"
        return width;
    }

    // 日期：半角数字/符号约 0.55em，中文约 1em
    float EstimateDateWidth() const
    {
        const float size = DateFontSize();
        float width = size * 6.0f;                            // 日期本体
        if (m_showWeekday)
            width += size * 4.3f;                             // " · 星期三"
        return width;
    }
};
