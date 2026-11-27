#pragma once
#include "../Framework.h"
#include "../Component.h"
#include "../GuiBase/GuiBase.h"

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <thread>

// =============================================================================
// WeatherDisplay：天气组件
//   数据源：https://wttr.in/?format=j1，请求失败时回退到 https://wttr.in/?format=j2（精简版本的JSON）
//   位置：默认由 wttr.in 按公网 IP 自动识别；也可在配置文件里填写 Location 覆盖自动识别结果
//   网络请求在后台线程执行，渲染只读取互斥量保护的数据快照
// =============================================================================
class WeatherDisplay : public Component
{
public:
    bool m_showDetails = true;      // 显示体感/湿度/风速/能见度
    bool m_showForecast = true;     // 显示未来三天预报
    bool m_fahrenheit = false;      // 温度单位（false = 摄氏度）

    WeatherDisplay() : Component("天气")
    {
        windowFlags |= ImGuiWindowFlags_NoResize;
        windowFlags |= ImGuiWindowFlags_NoScrollbar;
        windowFlags |= ImGuiWindowFlags_NoScrollWithMouse;
        windowFlags |= ImGuiWindowFlags_NoTitleBar;
        windowFlags |= ImGuiWindowFlags_NoMove;

        // 注册组件特有配置项（自动挂在 "天气" 根节点下）
        config.AddItem("ShowDetails", true);
        config.AddItem("ShowForecast", true);
        config.AddItem("Fahrenheit", false);
        config.AddItem("RefreshMinutes", 10);
        config.AddItem("Location", std::string(""));

        UpdateWindowSize();

        // 后台线程：按刷新间隔请求天气，不阻塞主渲染线程
        m_worker = std::thread([this]() { WorkerLoop(); });
    }

    ~WeatherDisplay() override
    {
        m_stop = true;
        if (m_worker.joinable())
            m_worker.join();
    }

    // 配置加载：基础配置已由基类处理，这里只读自己的特有配置项
    void OnLoadConfig() override
    {
        m_showDetails = config.Get<bool>("ShowDetails");
        m_showForecast = config.Get<bool>("ShowForecast");
        m_fahrenheit = config.Get<bool>("Fahrenheit");

        int minutes = config.Get<int>("RefreshMinutes");
        if (minutes < 2) minutes = 2;
        if (minutes > 60) minutes = 60;
        m_refreshMinutes = minutes;

        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_location = config.Get<std::string>("Location");
        }
    }
    void OnSaveConfig() override
    {
        config.Set("ShowDetails", m_showDetails);
        config.Set("ShowForecast", m_showForecast);
        config.Set("Fahrenheit", m_fahrenheit);
        config.Set("RefreshMinutes", m_refreshMinutes.load());
        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            config.Set("Location", m_location);
        }
    }

    const char* GetIcon() const override { return ICON_FA_CLOUD_SUN; }
    const char* GetDescription() const override { return "实时天气与未来三天预报（wttr.in）"; }

protected:
    void CollectSettings(std::vector<Miau::Settings::SettingItem>& items) override
    {
        items.push_back(MakeToggle("ShowDetails", "详细指标", "显示内容", m_showDetails,
                                   "体感温度、湿度、风速与能见度"));
        items.push_back(MakeToggle("ShowForecast", "未来三天预报", "显示内容", m_showForecast,
                                   "显示 wttr.in 返回的三天天气预报"));
        items.push_back(MakeToggle("Fahrenheit", "使用华氏度", "数据", m_fahrenheit,
                                   "关闭时使用摄氏度"));
        items.push_back(MakeSlider("RefreshMinutes", "刷新间隔", "数据",
                                   static_cast<double>(m_refreshMinutes.load()), 20.0, 720.0, 1.0, 0, " 分钟",
                                   "天气数据的自动刷新间隔（过短会被服务端限流）"));

        Miau::Settings::SettingItem refresh;
        refresh.key = "RefreshNow";
        refresh.label = "立即刷新";
        refresh.group = "数据";
        refresh.description = "跳过等待，立刻重新请求天气数据";
        refresh.kind = Miau::Settings::ItemKind::Action;
        refresh.valueKind = Miau::Settings::ValueKind::Bool;
        items.push_back(refresh);
    }

    void OnSettingsApplied() override
    {
        UpdateWindowSize();
        m_refreshRequested = true;   // 设置变更后立即拉取一次，保证显示与配置一致
    }

    void AfterBegin() override
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    }

    void ComponentRender() override
    {
        ImGui::PopStyleVar();   // AfterBegin 的 WindowPadding

        WeatherData data;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            data = m_data;
        }

        GuiBase::GuiThemeTokens tokens = GuiBase::Theme::Dark();
        const ImVec4 textColor = tokens.text.ToImVec4();
        const ImVec4 subColor(0.62f, 0.64f, 0.70f, 1.0f);
        const ImVec4 accentColor = tokens.primary.ToImVec4();
        const ImVec4 dangerColor(0.96f, 0.52f, 0.42f, 1.0f);

        if (!data.valid)
        {
            ImGui::Spacing();
            if (data.error.empty())
                ImGui::TextColored(subColor, "正在获取天气信息…");
            else
            {
                ImGui::TextColored(dangerColor, "无法获取天气信息");
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(subColor, "%s", data.error.c_str());
                ImGui::PopTextWrapPos();
            }
            if (ImGui::Button("立即重试"))
                m_refreshRequested = true;
            return;
        }

        const WeatherVisual visual = ResolveVisual(data.code);
        const char* description = visual.desc[0] != '\0' ? visual.desc : data.desc.c_str();
        const int shownTemp = m_fahrenheit ? ToFahrenheit(data.tempC) : data.tempC;
        const int shownFeels = m_fahrenheit ? ToFahrenheit(data.feelsC) : data.feelsC;

        // ---- 主信息行：天气图标 + 温度 + 描述/位置 ----
        ImGui::PushFont(NULL, ImGui::GetFontSize() * 2.4f);
        ImGui::TextColored(accentColor, "%s", visual.icon);
        ImGui::PopFont();

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::PushFont(NULL, ImGui::GetFontSize() * 2.4f);
        ImGui::TextColored(textColor, "%d°", shownTemp);
        ImGui::PopFont();

        ImGui::SameLine(0.0f, 14.0f);
        ImGui::BeginGroup();
        ImGui::TextColored(textColor, "%s", description);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(subColor, "%s", data.locationText.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();

        // ---- 详细指标：体感 / 湿度 / 风速 / 能见度（2×2 网格）----
        if (m_showDetails)
        {
            DrawDivider(tokens.border);

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 4));
            if (ImGui::BeginTable("##weather_metrics", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                DrawMetric("体感", FormatTemperature(shownFeels), subColor, textColor);
                ImGui::TableSetColumnIndex(1);
                DrawMetric("湿度", std::to_string(data.humidity) + "%", subColor, textColor);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                DrawMetric("风速", std::to_string(data.windKmph) + " km/h " + data.windDir, subColor, textColor);
                ImGui::TableSetColumnIndex(1);
                DrawMetric("能见度", std::to_string(data.visibility) + " km", subColor, textColor);

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }

        // ---- 未来三天预报 ----
        if (m_showForecast && !data.forecast.empty())
        {
            DrawDivider(tokens.border);

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 4));
            const int dayCount = static_cast<int>(data.forecast.size());
            if (ImGui::BeginTable("##weather_forecast", dayCount, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                for (int i = 0; i < dayCount; ++i)
                {
                    const ForecastDay& day = data.forecast[i];
                    const WeatherVisual dayVisual = ResolveVisual(day.code);
                    const int maxTemp = m_fahrenheit ? ToFahrenheit(day.maxC) : day.maxC;
                    const int minTemp = m_fahrenheit ? ToFahrenheit(day.minC) : day.minC;

                    ImGui::TableSetColumnIndex(i);
                    ImGui::TextColored(subColor, "%s", day.label.c_str());
                    ImGui::TextColored(accentColor, "%s", dayVisual.icon);
                    ImGui::TextColored(textColor, "%d° / %d°", maxTemp, minTemp);
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
    }

private:
    // 数据模型
    struct ForecastDay
    {
        std::string date;    // 原始日期（2026-11-20）
        std::string label;   // 显示用日期（11-20）
        std::string code;    // WW 天气代码
        int maxC = 0;
        int minC = 0;
    };

    struct WeatherData
    {
        bool valid = false;
        std::string error;
        std::string locationText;   // 自动识别到的位置
        std::string desc;           // 接口返回的天气描述（映射失败时兜底）
        std::string code;           // 当前天气代码
        int tempC = 0;
        int feelsC = 0;
        int humidity = 0;
        int windKmph = 0;
        int visibility = 0;
        int pressure = 0;
        std::string windDir;
        std::vector<ForecastDay> forecast;
    };

    struct WeatherVisual
    {
        const char* desc;
        const char* icon;
    };

    // 线程与状态
    std::thread m_worker;
    std::atomic<bool> m_stop{ false };
    std::atomic<bool> m_refreshRequested{ false };
    std::atomic<int> m_refreshMinutes{ 10 };

    std::mutex m_dataMutex;      // 保护 m_data
    WeatherData m_data;

    std::mutex m_requestMutex;   // 保护 m_location
    std::string m_location;      // 空 = 由 wttr.in 按公网 IP 自动识别

    void UpdateWindowSize()
    {
        float height = 88.0f;                          // 内边距 + 主信息行
        if (m_showDetails) height += 118.0f;           // 分割线 + 2×2 指标
        if (m_showForecast) height += 90.0f;           // 分割线 + 三天预报
        SetWindowSize(ImVec2(400.0f, height));
    }

    // 后台刷新
    void WorkerLoop()
    {
        ULONGLONG nextAttempt = 0;
        while (!m_stop.load())
        {
            const ULONGLONG now = GetTickCount64();
            if (m_refreshRequested.exchange(false) || now >= nextAttempt)
            {
                const bool ok = FetchOnce();
                // 成功：按配置的分钟数刷新、失败：1 分钟后重试
                const ULONGLONG delayMinutes = static_cast<ULONGLONG>(m_refreshMinutes.load());
                const ULONGLONG delay = ok ? delayMinutes * 60ULL * 1000ULL : 60ULL * 1000ULL;
                nextAttempt = GetTickCount64() + delay;
            }

            // 分片休眠，保证停止/立即刷新能及时响应
            for (int i = 0; i < 10 && !m_stop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool FetchOnce()
    {
        std::string location;
        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            location = m_location;
        }

        WeatherData parsed;
        std::string error;

        // 主接口 j1（完整 JSON）；失败时回退 j2（j2是精简JSON）
        bool ok = false;
        std::string body;
        if (HttpGet(BuildPath(location, true), body, error))
            ok = ParseWeather(body, parsed, error);

        if (!ok)
        {
            std::string fallbackBody;
            std::string fallbackError;
            if (HttpGet(BuildPath(location, false), fallbackBody, fallbackError))
            {
                parsed = WeatherData{};
                if (ParseWeather(fallbackBody, parsed, fallbackError))
                    ok = true;
                else if (error.empty())
                    error = fallbackError;
            }
            else if (error.empty())
                error = fallbackError;
        }

        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            if (ok)
                m_data = std::move(parsed);
            else
                m_data.error = error.empty() ? "未知错误" : error;
            // 已有有效数据时保留旧数据，仅记录错误
        }
        return ok;
    }

    // 请求路径：location 为空时由 wttr.in 自动定位
    static std::wstring BuildPath(const std::string& location, bool fullJson)
    {
        std::string path = "/";
        if (!location.empty())
            path += PercentEncode(location);
        path += fullJson ? "?format=j1&lang=zh" : "?format=j2";

        std::wstring wide;
        wide.reserve(path.size());
        for (char ch : path)
            wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
        return wide;
    }

	// 百分号编码（URL Encode）,实际上可以说是数字的URL编码
    static std::string PercentEncode(const std::string& text)
    {
        static const char* hex = "0123456789ABCDEF";
        std::string out;
        out.reserve(text.size() * 3);
        for (unsigned char ch : text)
        {
            const bool unreserved = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                    (ch >= '0' && ch <= '9') ||
                                    ch == '-' || ch == '_' || ch == '.' || ch == '~';
            if (unreserved)
                out.push_back(static_cast<char>(ch));
            else
            {
                out.push_back('%');
                out.push_back(hex[ch >> 4]);
                out.push_back(hex[ch & 0x0F]);
            }
        }
        return out;
    }

    static bool HttpGet(const std::wstring& path, std::string& body, std::string& error)
    {
        body.clear();

        HINTERNET session = WinHttpOpen(L"curl/8.5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
        {
            error = "无法初始化 WinHTTP";
            return false;
        }
        WinHttpSetTimeouts(session, 5000, 5000, 8000, 12000);

        bool ok = false;
        HINTERNET connect = WinHttpConnect(session, L"wttr.in", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect)
            error = "无法连接 wttr.in（网络不可用或被拦截）";
        else
        {
            HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!request)
                error = "无法创建 HTTP 请求";
            else
            {
                if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
                    error = "HTTP 请求发送失败";
                else if (!WinHttpReceiveResponse(request, nullptr))
                    error = "等待服务器响应超时";
                else
                {
                    DWORD statusCode = 0;
                    DWORD size = sizeof(statusCode);
                    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
                    if (statusCode != 200)
                        error = "服务器返回状态 " + std::to_string(statusCode);
                    else
                    {
                        ok = true;
                        DWORD available = 0;
                        while (WinHttpQueryDataAvailable(request, &available) && available > 0)
                        {
                            std::string chunk(available, '\0');
                            DWORD read = 0;
                            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0)
                            {
                                ok = false;
                                error = "读取响应失败";
                                break;
                            }
                            chunk.resize(read);
                            body += chunk;
                            if (body.size() > 4u * 1024u * 1024u)   // 防御性上限
                                break;
                        }
                    }
                }
                WinHttpCloseHandle(request);
            }
            WinHttpCloseHandle(connect);
        }

        WinHttpCloseHandle(session);
        return ok;
    }

    // 取字符串：兼容 "文本" 与 [{"value": "文本"}] 两种形式
    static std::string GetText(const picojson::value& object, const char* key)
    {
        if (!object.is<picojson::object>() || !object.contains(key))
            return {};

        const picojson::value& value = object.get(key);
        if (value.is<std::string>())
            return value.get<std::string>();
        if (value.is<picojson::array>())
        {
            const picojson::array& array = value.get<picojson::array>();
            if (!array.empty())
                return GetText(array[0], "value");
        }
        return {};
    }

    // 取整数：兼容 12 与 "12" 两种形式；无法解析时返回 0
    static int GetInt(const picojson::value& object, const char* key)
    {
        if (!object.is<picojson::object>() || !object.contains(key))
            return 0;

        const picojson::value& value = object.get(key);
        if (value.is<double>())
            return static_cast<int>(value.get<double>());
        if (value.is<std::string>())
        {
            const std::string& text = value.get<std::string>();
            char* end = nullptr;
            const long parsed = std::strtol(text.c_str(), &end, 10);
            return (end != text.c_str()) ? static_cast<int>(parsed) : 0;
        }
        return 0;
    }

    // 取数组：字段不存在或不是数组时返回 nullptr
    static const picojson::array* GetArray(const picojson::value& object, const char* key)
    {
        if (!object.is<picojson::object>() || !object.contains(key))
            return nullptr;

        const picojson::value& value = object.get(key);
        return value.is<picojson::array>() ? &value.get<picojson::array>() : nullptr;
    }

    // 解析 wttr.in 响应：j1 为完整 JSON，j2 为精简 JSON，字段结构相同
    static bool ParseWeather(const std::string& body, WeatherData& out, std::string& error)
    {
        picojson::value root;
        const std::string parseError = picojson::parse(root, body);
        if (!parseError.empty())
        {
            error = "JSON 解析失败：" + parseError;
            return false;
        }
        if (!root.is<picojson::object>())
        {
            error = "响应格式不正确";
            return false;
        }

        // ---- 当前天气：current_condition[0] ----
        const picojson::array* conditions = GetArray(root, "current_condition");
        if (conditions == nullptr || conditions->empty() || !(*conditions)[0].is<picojson::object>())
        {
            error = "响应中没有当前天气数据";
            return false;
        }

        const picojson::value& current = (*conditions)[0];
        out = WeatherData{};
        out.tempC = GetInt(current, "temp_C");
        out.feelsC = GetInt(current, "FeelsLikeC");
        out.humidity = GetInt(current, "humidity");
        out.windKmph = GetInt(current, "windspeedKmph");
        out.visibility = GetInt(current, "visibility");
        out.pressure = GetInt(current, "pressure");
        out.windDir = GetText(current, "winddir16Point");
        out.code = GetText(current, "weatherCode");
        out.desc = GetText(current, "weatherDesc");

        // ---- 位置：nearest_area[0].areaName / region / country ----
        std::string area;
        std::string region;
        std::string country;
        if (const picojson::array* areas = GetArray(root, "nearest_area"); areas != nullptr && !areas->empty())
        {
            const picojson::value& nearest = (*areas)[0];
            area = GetText(nearest, "areaName");
            region = GetText(nearest, "region");
            country = GetText(nearest, "country");
        }

        std::string location = area;
        if (!region.empty() && region != area)
            location += " · " + region;
        else if (!country.empty())
            location += " · " + country;
        out.locationText = location.empty() ? "自动定位" : location;

        // ---- 未来三天：weather[].hourly[4] 为当天正午 ----
        if (const picojson::array* days = GetArray(root, "weather"))
        {
            for (const picojson::value& dayJson : *days)
            {
                if (out.forecast.size() >= 3)
                    break;
                if (!dayJson.is<picojson::object>())
                    continue;

                ForecastDay day;
                day.date = GetText(dayJson, "date");
                day.label = FormatDayLabel(day.date);
                day.maxC = GetInt(dayJson, "maxtempC");
                day.minC = GetInt(dayJson, "mintempC");

                if (const picojson::array* hours = GetArray(dayJson, "hourly"); hours != nullptr && !hours->empty())
                {
                    const picojson::value& noon = hours->size() > 4 ? (*hours)[4] : (*hours)[0];
                    day.code = GetText(noon, "weatherCode");
                }
                if (day.code.empty())
                    day.code = out.code;

                out.forecast.push_back(std::move(day));
            }
        }

        out.valid = true;
        return true;
    }

    // "2026-11-20" -> "11-20"，去除年份
    static std::string FormatDayLabel(const std::string& date)
    {
        return date.size() >= 10 ? date.substr(5, 5) : date;
    }

    // ---------------------------------------------------------------------
    // 显示辅助
    // ---------------------------------------------------------------------
    static int ToFahrenheit(int celsius)
    {
        const double value = celsius * 9.0 / 5.0 + 32.0;
        return static_cast<int>(value >= 0.0 ? value + 0.5 : value - 0.5);
    }

    static std::string FormatTemperature(int value)
    {
        return std::to_string(value) + "°";
    }

    static void DrawDivider(const GuiBase::GuiColor& color)
    {
        ImGui::Spacing();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        drawList->AddLine(start, ImVec2(start.x + width, start.y), color.ToImU32(), 1.0f);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }

    static void DrawMetric(const char* label, const std::string& value,
                           const ImVec4& labelColor, const ImVec4& valueColor)
    {
        ImGui::TextColored(labelColor, "%s", label);
        ImGui::PushFont(NULL, ImGui::GetFontSize() * 1.05f);
        ImGui::TextColored(valueColor, "%s", value.c_str());
        ImGui::PopFont();
    }

    // WW 天气代码 -> 中文描述 + Font Awesome 图标（未知代码返回空描述，由调用方兜底）
    static WeatherVisual ResolveVisual(const std::string& code)
    {
        switch (atoi(code.c_str()))
        {
        case 113: return { "晴", ICON_FA_SUN };
        case 116: return { "多云", ICON_FA_CLOUD_SUN };
        case 119: return { "阴", ICON_FA_CLOUD };
        case 122: return { "阴天", ICON_FA_CLOUD };
        case 143: return { "薄雾", ICON_FA_SMOG };
        case 248: return { "雾", ICON_FA_SMOG };
        case 260: return { "冻雾", ICON_FA_SMOG };

        case 200: case 386: case 389: case 392: case 395:
            return { "雷雨", ICON_FA_CLOUD_BOLT };

        case 227: case 230: case 323: case 326: case 329:
        case 332: case 335: case 338: case 368: case 371:
            return { "降雪", ICON_FA_SNOWFLAKE };

        case 179: case 182: case 185: case 281: case 284: case 311:
        case 314: case 317: case 320: case 350: case 362: case 365:
        case 374: case 377:
            return { "雨夹雪", ICON_FA_CLOUD_RAIN };

        case 299: case 302: case 305: case 308: case 356: case 359:
            return { "中到大雨", ICON_FA_CLOUD_SHOWERS_HEAVY };

        case 176: case 263: case 266: case 293: case 296: case 353:
            return { "小雨", ICON_FA_CLOUD_RAIN };

        default:
            return { "", ICON_FA_CLOUD };
        }
    }
};
