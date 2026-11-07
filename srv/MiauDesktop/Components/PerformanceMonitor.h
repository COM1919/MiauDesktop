#pragma once
#include "../Framework.h"
#include "../Component.h"
#include "../GuiBase/GuiBase.h"
#include "../AppSettings/AppSettings.h"

class PerformanceMonitor : public Component
{
public:
    bool m_showCpu = true;
    bool m_showMemory = true;
    bool m_showGpu = true;

    void UpdateWindowSize()
    {
        int visibleCount = (int)m_showCpu + (int)m_showMemory + (int)m_showGpu;
        if (visibleCount == 0)
            visibleCount = 1;
        SetWindowSize(ImVec2((150.0f + style.ItemSpacing.x) * visibleCount + 8.0f, 175.0f));
    }

    PerformanceMonitor() : Component("性能监控", 0, true)
    {
        UpdateWindowSize();
        windowFlags |= ImGuiWindowFlags_NoResize;
        windowFlags |= ImGuiWindowFlags_NoScrollbar;
        windowFlags |= ImGuiWindowFlags_NoScrollWithMouse;
        windowFlags |= ImGuiWindowFlags_NoTitleBar;
        windowFlags |= ImGuiWindowFlags_NoMove;

        // 注册组件特有配置项（自动挂在 "性能监控" 根节点下）
        config.AddItem("ShowCpu", true);
        config.AddItem("ShowMemory", true);
        config.AddItem("ShowGpu", true);
    }

    // 配置加载：基础配置已由基类处理，这里只读自己的特有配置项
    void OnLoadConfig() override
    {
        m_showCpu = config.Get<bool>("ShowCpu");
        m_showMemory = config.Get<bool>("ShowMemory");
        m_showGpu = config.Get<bool>("ShowGpu");
    }
    // 配置保存：基础配置已由基类处理，这里只保存自己的特有配置项
    void OnSaveConfig() override
    {
        config.Set("ShowCpu", m_showCpu);
        config.Set("ShowMemory", m_showMemory);
        config.Set("ShowGpu", m_showGpu);
    }

    const char* GetIcon() const override { return ICON_FA_GAUGE_HIGH; }
    const char* GetDescription() const override { return "CPU、内存、GPU 实时监控"; }

protected:
    void CollectSettings(std::vector<Miau::Settings::SettingItem>& items) override
    {
        items.push_back(MakeToggle("ShowCpu", "CPU 仪表盘", "显示面板", m_showCpu, "显示 CPU 使用率环形仪表"));
        items.push_back(MakeToggle("ShowMemory", "内存仪表盘", "显示面板", m_showMemory, "显示物理内存使用率环形仪表"));
        items.push_back(MakeToggle("ShowGpu", "GPU 仪表盘", "显示面板", m_showGpu, "显示 GPU 3D 引擎使用率环形仪表"));
    }

    void OnSettingsApplied() override
    {
        UpdateWindowSize();
    }

    void AfterBegin() override
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    }

    void ComponentRender() override
    {
        ImGui::PopStyleVar();

        int visibleCount = (int)m_showCpu + (int)m_showMemory + (int)m_showGpu;
        if (visibleCount == 0)
        {
            ImGui::TextDisabled("请在设置中启用至少一个面板");
            return;
        }

        const ImVec2 gaugeSize(150, 160);

        if (m_showCpu)
        {
            DrawGuiGauge("CPU", m_cpuAnim.Advance(GetCpuUsage() / 100.0f), gaugeSize, GuiBase::GuiColor(0.22f, 0.50f, 0.88f, 1.0f));
            if (m_showMemory || m_showGpu) ImGui::SameLine();
        }
        if (m_showMemory)
        {
            DrawGuiGauge("内存", m_memAnim.Advance(GetMemoryUsage() / 100.0f), gaugeSize, GuiBase::GuiColor(0.20f, 0.76f, 0.58f, 1.0f));
            if (m_showGpu) ImGui::SameLine();
        }
        if (m_showGpu)
        {
            DrawGuiGauge("GPU", m_gpuAnim.Advance(GetGpuUsage() / 100.0f), gaugeSize, GuiBase::GuiColor(0.98f, 0.62f, 0.15f, 1.0f));
        }
    }

private:
    // EUI 风格圆形仪表盘：外圈轨道 + 进度弧 + 中心百分比 + 底部标签 + 胶囊进度条
    void DrawGuiGauge(const char* label, float progress, ImVec2 size, GuiBase::GuiColor fillColor)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* dl = window->DrawList;

        float value = std::clamp(progress, 0.0f, 1.0f);
        ImVec2 cursor = ImGui::GetCursorScreenPos();

        // 外圈轨道颜色（EUI surface 暗色）
        GuiBase::GuiColor trackColor = GuiBase::GuiColor(0.20f, 0.20f, 0.24f, 1.0f);
        // 文字颜色（EUI text）
        GuiBase::GuiColor textColor = GuiBase::GuiColor(1.0f, 1.0f, 1.0f, 1.0f);
        GuiBase::GuiColor subColor = GuiBase::GuiColor(0.55f, 0.55f, 0.60f, 1.0f);

        float centerX = cursor.x + size.x * 0.5f;
        float centerY = cursor.y + 58.0f;
        float outerRadius = 48.0f;
        float thickness = 8.0f;

        // 轨道圆环
        dl->AddCircle(ImVec2(centerX, centerY), outerRadius, trackColor.ToImU32(), 64, thickness);

        // 进度弧（从顶部顺时针）
        if (value > 0.001f)
        {
            const float startAngle = -IM_PI * 0.5f;  // 顶部
            const float sweep = value * IM_PI * 2.0f;
            int segments = std::max(16, (int)(value * 64.0f));

            std::vector<ImVec2> arcPoints;
            arcPoints.reserve(segments + 1);
            for (int i = 0; i <= segments; i++)
            {
                float a = startAngle + sweep * (float)i / (float)segments;
                arcPoints.push_back(ImVec2(centerX + cosf(a) * outerRadius,
                                           centerY + sinf(a) * outerRadius));
            }
            dl->AddPolyline(arcPoints.data(), (int)arcPoints.size(), fillColor.ToImU32(), 0, thickness);
        }

        // 中心百分比文字
        char pctText[16];
        snprintf(pctText, sizeof(pctText), "%d%%", (int)(value * 100.0f));
        float pctFontSize = 22.0f;
        ImVec2 pctSize = ImGui::CalcTextSize(pctText);
        dl->AddText(ImGui::GetFont(), pctFontSize,
                    ImVec2(centerX - pctSize.x * 0.5f, centerY - pctSize.y * 0.5f),
                    textColor.ToImU32(), pctText);

        // 底部标签
        float labelFontSize = 14.0f;
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        dl->AddText(ImGui::GetFont(), labelFontSize,
                    ImVec2(centerX - labelSize.x * 0.5f, centerY + outerRadius + 12.0f),
                    subColor.ToImU32(), label);

        // 底部 EUI 胶囊进度条
        float barY = cursor.y + size.y - 16.0f;
        float barH = 6.0f;
        float barMargin = 12.0f;
        ImVec2 barPos(cursor.x + barMargin, barY);
        ImVec2 barSize(size.x - barMargin * 2.0f, barH);
        GuiBase::DrawGuiProgress(dl, barPos, barSize, value, fillColor, trackColor);

        ImGui::Dummy(size);
    }

    float GetCpuUsage() const
    {
        DWORD currentTick = GetTickCount64();

        if (currentTick - m_cpuLastUpdateTick < 1000 && !m_cpuIsFirstCall)
            return m_cpuCachedUsage;

        FILETIME idleTime, kernelTime, userTime;
        GetSystemTimes(&idleTime, &kernelTime, &userTime);

        ULARGE_INTEGER curIdle, curKernel, curUser;
        curIdle.LowPart = idleTime.dwLowDateTime;
        curIdle.HighPart = idleTime.dwHighDateTime;
        curKernel.LowPart = kernelTime.dwLowDateTime;
        curKernel.HighPart = kernelTime.dwHighDateTime;
        curUser.LowPart = userTime.dwLowDateTime;
        curUser.HighPart = userTime.dwHighDateTime;

        if (m_cpuIsFirstCall)
        {
            m_cpuPrevIdle = curIdle.QuadPart;
            m_cpuPrevKernel = curKernel.QuadPart;
            m_cpuPrevUser = curUser.QuadPart;
            m_cpuIsFirstCall = false;
            m_cpuLastUpdateTick = currentTick;
            m_cpuCachedUsage = 0.0f;
            return 0.0f;
        }

        ULONGLONG idleDiff = curIdle.QuadPart - m_cpuPrevIdle;
        ULONGLONG kernelDiff = curKernel.QuadPart - m_cpuPrevKernel;
        ULONGLONG userDiff = curUser.QuadPart - m_cpuPrevUser;
        ULONGLONG totalDiff = kernelDiff + userDiff;

        m_cpuPrevIdle = curIdle.QuadPart;
        m_cpuPrevKernel = curKernel.QuadPart;
        m_cpuPrevUser = curUser.QuadPart;

        float usage = 0.0f;
        if (totalDiff > 0)
        {
            usage = static_cast<float>((totalDiff - idleDiff) * 100.0 / totalDiff);
            if (usage < 0.0f) usage = 0.0f;
            if (usage > 100.0f) usage = 100.0f;
        }

        m_cpuCachedUsage = usage;
        m_cpuLastUpdateTick = currentTick;
        return usage;
    }

    float GetMemoryUsage() const
    {
        DWORD currentTick = GetTickCount64();
        if (currentTick - m_memLastUpdateTick < 1000)
            return m_memCachedUsage;

        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (!GlobalMemoryStatusEx(&memStatus))
            return m_memCachedUsage;

        float usage = 0.0f;
        if (memStatus.ullTotalPhys > 0)
        {
            usage = 100.0f * (1.0f - (float)memStatus.ullAvailPhys / (float)memStatus.ullTotalPhys);
            if (usage < 0.0f) usage = 0.0f;
            if (usage > 100.0f) usage = 100.0f;
        }

        m_memCachedUsage = usage;
        m_memLastUpdateTick = currentTick;
        return usage;
    }

    float GetGpuUsage() const
    {
        DWORD currentTick = GetTickCount64();
        if (currentTick - m_gpuLastUpdateTick < 1000 && m_gpuInitialized)
            return m_gpuCachedUsage;

        if (!m_gpuInitialized)
        {
            if (PdhOpenQuery(NULL, 0, &m_gpuQuery) != ERROR_SUCCESS)
            {
                m_gpuCachedUsage = 0.0f;
                m_gpuLastUpdateTick = currentTick;
                m_gpuInitialized = true;
                return 0.0f;
            }

            if (PdhAddEnglishCounterW(m_gpuQuery, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &m_gpuCounter) != ERROR_SUCCESS)
            {
                PdhCloseQuery(m_gpuQuery);
                m_gpuQuery = nullptr;
                m_gpuCounter = nullptr;
            }

            m_gpuInitialized = true;
            m_gpuLastUpdateTick = currentTick;
            m_gpuCachedUsage = 0.0f;
            return 0.0f;
        }

        if (!m_gpuQuery || !m_gpuCounter)
        {
            m_gpuLastUpdateTick = currentTick;
            return m_gpuCachedUsage;
        }

        PdhCollectQueryData(m_gpuQuery);

        DWORD dwBufferSize = 0;
        DWORD dwItemCount = 0;
        PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);

        if (dwBufferSize > 0)
        {
            std::vector<BYTE> buffer(dwBufferSize);
            PDH_FMT_COUNTERVALUE_ITEM_W* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
            if (PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, items) == ERROR_SUCCESS)
            {
                double maxUsage = 0.0;
                for (DWORD i = 0; i < dwItemCount; i++)
                {
                    if (items[i].FmtValue.doubleValue > maxUsage)
                        maxUsage = items[i].FmtValue.doubleValue;
                }
                m_gpuCachedUsage = (float)maxUsage;
                if (m_gpuCachedUsage < 0.0f) m_gpuCachedUsage = 0.0f;
                if (m_gpuCachedUsage > 100.0f) m_gpuCachedUsage = 100.0f;
            }
        }

        m_gpuLastUpdateTick = currentTick;
        return m_gpuCachedUsage;
    }

private:
    mutable float  m_cpuCachedUsage = 0.0f;
    mutable DWORD  m_cpuLastUpdateTick = 0;
    mutable bool   m_cpuIsFirstCall = true;
    mutable ULONGLONG m_cpuPrevIdle = 0;
    mutable ULONGLONG m_cpuPrevKernel = 0;
    mutable ULONGLONG m_cpuPrevUser = 0;

    mutable float  m_memCachedUsage = 0.0f;
    mutable DWORD  m_memLastUpdateTick = 0;

    mutable float  m_gpuCachedUsage = 0.0f;
    mutable DWORD  m_gpuLastUpdateTick = 0;
    mutable bool   m_gpuInitialized = false;
    mutable PDH_HQUERY m_gpuQuery = nullptr;
    mutable PDH_HCOUNTER m_gpuCounter = nullptr;

    // 仪表盘进度动画值（由全局动画设置驱动，GuiBase::GuiAnimatedProgress 封装，组件零配置）
    GuiBase::GuiAnimatedProgress m_cpuAnim;
    GuiBase::GuiAnimatedProgress m_memAnim;
    GuiBase::GuiAnimatedProgress m_gpuAnim;
};