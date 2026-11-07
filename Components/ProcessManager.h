#pragma once
#include "../Framework.h"
#include "../Component.h"
#include "../GuiBase/GuiBase.h"

// 进程信息结构体
static struct ProcessInfo
{
    DWORD pid = 0;
    std::string name;               // UTF-8 进程名称
    std::wstring exePath;           // 进程可执行文件路径（用于获取图标）
    float cpuUsage = 0.0f;          // 百分比
    SIZE_T memUsage = 0;            // 字节
    float gpuUsage = 0.0f;          // 百分比
    float diskMBps = 0.0f;          // 读写总速度 MB/s
    int   networkConns = 0;         // 已建立的 TCP 连接数（网络活动指标）

    // 上次采样的 CPU 时间
    ULONGLONG prevProcKernel = 0;
    ULONGLONG prevProcUser = 0;
    // 上次采样的 IO 字节数
    ULONGLONG prevReadBytes = 0;
    ULONGLONG prevWriteBytes = 0;
};

class ProcessManager : public Component
{
public:
    int m_refreshIntervalMs = 2000;    // 刷新间隔（毫秒）
    bool m_showSystemProcess = false;  // 是否显示系统关键进程
    // 列显示开关
    bool m_showCpu = true;
    bool m_showMemory = true;
    bool m_showGpu = false;
    bool m_showPid = true;
    bool m_showDisk = false;
    bool m_showNetwork = false;

    // 配置加载：基础配置已由基类处理，这里只读自己的特有配置项
    void OnLoadConfig() override
    {
        m_refreshIntervalMs = config.Get<int>("RefreshIntervalMs");
        m_showSystemProcess = config.Get<bool>("ShowSystemProcess");
        m_showCpu = config.Get<bool>("ShowCpu");
        m_showMemory = config.Get<bool>("ShowMemory");
        m_showPid = config.Get<bool>("ShowPid");
        m_showDisk = config.Get<bool>("ShowDisk");
        m_showNetwork = config.Get<bool>("ShowNetwork");
    }
    void OnSaveConfig() override
    {
        config.Set("RefreshIntervalMs", m_refreshIntervalMs);
        config.Set("ShowSystemProcess", m_showSystemProcess);
        config.Set("ShowCpu", m_showCpu);
        config.Set("ShowMemory", m_showMemory);
        config.Set("ShowPid", m_showPid);
        config.Set("ShowDisk", m_showDisk);
        config.Set("ShowNetwork", m_showNetwork);
    }

    ProcessManager() : Component("进程管理")
    {
        SetWindowSize(ImVec2(700, 450));
        windowFlags |= ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoResize;

        // 注册组件特有配置项（自动挂在 "进程管理" 根节点下）
        config.AddItem("RefreshIntervalMs", 2000);
        config.AddItem("ShowSystemProcess", false);
    }

    const char* GetIcon() const override
    {
        return ICON_FA_LIST_CHECK;
    }
    const char* GetDescription() const override
    {
        return "系统进程的性能统计列表";
    }

    ~ProcessManager()
    {
        // 清理图标纹理缓存
        for (auto& pair : m_iconCache)
        {
            if (pair.second)
                GuiBase::UnloadTexture("ProcIcon_" + std::to_string(pair.first));
        }
        // 清理 PDH GPU 查询
        if (m_gpuCounter) PdhRemoveCounter(m_gpuCounter);
        if (m_gpuQuery) PdhCloseQuery(m_gpuQuery);
    }
protected:
    void SettingsRender() override
    {
        SettingsTitle("显示内容");
        SettingsToggleRow("PID", &m_showPid);
        SettingsToggleRow("CPU", &m_showCpu);
        SettingsToggleRow("内存", &m_showMemory);
        SettingsToggleRow("GPU", &m_showGpu);
        SettingsToggleRow("储存", &m_showDisk);
        SettingsToggleRow("网络", &m_showNetwork);
    }

    // 网络列显示已建立的 TCP 连接数（真正的吞吐量需要 ETW，复杂度过高）
    void ComponentRender() override
    {
        if (m_processes.empty() && m_lastRefreshTick == 0)
            RefreshProcesses();

        DWORD now = GetTickCount64();
        // 刷新间隔由组件配置驱动（m_refreshIntervalMs 由配置系统加载）
        const DWORD refreshInterval = (DWORD)m_refreshIntervalMs;
        if (now - m_lastRefreshTick > refreshInterval)
        {
            RefreshProcesses();
            m_lastRefreshTick = now;
        }

        // 使用全局 ImGuiStyle 默认配色（TableHeaderBg/TableRowBg/Text 等已在 SetupImGuiStyle 定义），
        // 组件不再 PushStyleColor，方便后续添加组件

        ImGui::Spacing();

        // === 进程列表表格 ===
        ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
            | ImGuiTableFlags_Sortable
            | ImGuiTableFlags_ScrollY
            | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_BordersInnerV
            | ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("##ProcessTable", GetVisibleColumnCount(), tableFlags, ImVec2(0, ImGui::GetContentRegionAvail().y)))
        {
            int colIdx = 0;

            // 图标列（固定宽度，始终显示）
            ImGui::TableSetupColumn("##Icon", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 28.0f, colIdx++);
            // 进程名称列（可拉伸）
            ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch, 0.0f, colIdx++);
            if (m_showPid)
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 60.0f, colIdx++);
            if (m_showCpu)
                ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 90.0f, colIdx++);
            if (m_showMemory)
                ImGui::TableSetupColumn("内存", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 115.0f, colIdx++);
            if (m_showGpu)
                ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, 80.0f, colIdx++);
            if (m_showDisk)
                ImGui::TableSetupColumn("储存", ImGuiTableColumnFlags_WidthFixed, 80.0f, colIdx++);
            if (m_showNetwork)
                ImGui::TableSetupColumn("网络", ImGuiTableColumnFlags_WidthFixed, 80.0f, colIdx++);

            ImGui::TableHeadersRow();

            // 右键表头弹出列选择菜单
            if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered())
                ImGui::OpenPopup("##ColumnSelector");

            if (ImGui::BeginPopup("##ColumnSelector"))
            {
                ImGui::TextDisabled("列设置");
                ImGui::Separator();
                ImGui::MenuItem("PID", nullptr, &m_showPid);
                ImGui::MenuItem("CPU", nullptr, &m_showCpu);
                ImGui::MenuItem("内存", nullptr, &m_showMemory);
                ImGui::MenuItem("GPU", nullptr, &m_showGpu);
                ImGui::MenuItem("储存", nullptr, &m_showDisk);
                ImGui::MenuItem("网络", nullptr, &m_showNetwork);
                ImGui::EndPopup();
            }

            // 处理排序：如果用户点击了表头，使用 ImGui 的排序规格
            if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
            {
                if (sortSpecs->SpecsDirty && sortSpecs->SpecsCount > 0)
                {
                    const auto& spec = sortSpecs->Specs[0];
                    m_sortColumn = (int)spec.ColumnIndex;
                    m_sortAscending = (spec.SortDirection == ImGuiSortDirection_Ascending);
                    SortProcesses();
                    sortSpecs->SpecsDirty = false;
                }
            }

            // 数据行（使用 ImGuiListClipper 实现虚拟滚动）
            ImGuiListClipper clipper;
            clipper.Begin((int)m_processes.size());
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {
                    const auto& proc = m_processes[i];
                    ImGui::TableNextRow();

                    colIdx = 0;

                    // 系统图标
                    ImGui::TableSetColumnIndex(colIdx++);
                    Texture2D iconTex = GetProcessIconTexture(proc);
                    if (iconTex)
                    {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 9.0f); // 实际是使用截图工具慢慢搞出来的
                        ImGui::Image((ImTextureID)iconTex, ImVec2(24, 24));
                    }
                    else
                    {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 11.0f); // 实际是使用截图工具慢慢搞出来的
                        ImGui::PushStyleColor(ImGuiCol_Text, GetProcessColor(proc.name));
                        {
                            ImGui::SetWindowFontScale(1.2f);
                            ImGui::TextUnformatted(ICON_FA_BOX_ARCHIVE);
                            ImGui::SetWindowFontScale(1.0f);
                        }
                        ImGui::PopStyleColor();
                    }

                    // 进程名称
                    ImGui::TableSetColumnIndex(colIdx++);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(proc.name.c_str());

                    if (m_showPid)
                    {
                        ImGui::TableSetColumnIndex(colIdx++);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%u", proc.pid);
                    }
                    if (m_showCpu)
                    {
                        ImGui::TableSetColumnIndex(colIdx++);
                        ImGui::AlignTextToFramePadding();
                        // CPU 迷你进度条（EUI-NEO progress 风格，Blue 调色板）
                        DrawCellProgressBar(proc.cpuUsage / 100.0f, 44.0f, GuiBase::GuiColor(0.22f, 0.50f, 0.88f, 1.0f));
                        // CPU 使用率高的用高亮色
                        if (proc.cpuUsage > 10.0f)
                            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%.0f%%", proc.cpuUsage);
                        else
                            ImGui::Text("%.0f%%", proc.cpuUsage);
                    }
                    if (m_showMemory)
                    {
                        ImGui::TableSetColumnIndex(colIdx++);
                        ImGui::AlignTextToFramePadding();
                        // 内存迷你进度条（Green 调色板），按占总内存比例显示
                        float memRatio = m_totalMemBytes > 0
                            ? (float)((double)proc.memUsage / (double)m_totalMemBytes) : 0.0f;
                        DrawCellProgressBar(memRatio, 58.0f, GuiBase::GuiColor(0.20f, 0.76f, 0.58f, 1.0f));
                        ImGui::TextUnformatted(FormatMemory(proc.memUsage).c_str());
                    }
                    if (m_showGpu)
                    {
                        ImGui::TableSetColumnIndex(colIdx++);
                        ImGui::AlignTextToFramePadding();
                        // GPU 迷你进度条（Orange 调色板）
                        DrawCellProgressBar(proc.gpuUsage / 100.0f, 34.0f, GuiBase::GuiColor(0.98f, 0.62f, 0.15f, 1.0f));
                        ImGui::Text("%.0f%%", proc.gpuUsage);
                    }
                    if (m_showDisk)
                    {
                        ImGui::TableSetColumnIndex(colIdx++);
                        ImGui::AlignTextToFramePadding();
                        if (proc.diskMBps > 0.01f)
                            ImGui::Text("%.1f MB/s", proc.diskMBps);
                        else
                            ImGui::TextDisabled("0 MB/s");
                    }
                    if (m_showNetwork)
                    {
                        ImGui::TableSetColumnIndex(colIdx++);
                        ImGui::AlignTextToFramePadding();
                        if (proc.networkConns > 0)
                            ImGui::Text("%d 连接", proc.networkConns);
                        else
                            ImGui::TextDisabled("0");
                    }
                }
            }
            clipper.End();

            ImGui::EndTable();
        }
    }

private:
    std::vector<ProcessInfo> m_processes;
    DWORD m_lastRefreshTick = 0;
    SIZE_T m_totalMemBytes = 0;  // 物理内存总量（用于内存列迷你进度条比例）

    // 排序状态（-1 表示未点击表头，使用默认排序：内存降序）
    int m_sortColumn = -1;
    bool m_sortAscending = false;

    // 在单元格内绘制 EUI 迷你胶囊进度条（参照 EUI-NEO progress 组件：
    // 胶囊形圆角 = height*0.5，track 使用 surfaceHover 色）
    void DrawCellProgressBar(float value, float barWidth, const GuiBase::GuiColor& fillColor)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float barH = 6.0f;
        // 垂直居中于文本行
        float y = p.y + (ImGui::GetTextLineHeight() - barH) * 0.5f;
        GuiBase::GuiColor track(0.25f, 0.25f, 0.28f, 1.0f);  // EUI surfaceHover
        GuiBase::DrawGuiProgress(dl, ImVec2(p.x, y), ImVec2(barWidth, barH),
                        std::clamp(value, 0.0f, 1.0f), fillColor, track);
        // 光标向右推进，供后续百分比文字同行显示（不改变 Y，避免错行）
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + barWidth + ImGui::GetStyle().ItemSpacing.x);
    }

    ULONGLONG m_prevSysKernel = 0;
    ULONGLONG m_prevSysUser = 0;
    bool m_firstSysSample = true;

    std::unordered_map<DWORD, ProcessInfo> m_prevProcessData;

    // 图标纹理缓存：pid → 纹理
    std::unordered_map<DWORD, Texture2D> m_iconCache;
    // exe 路径 → 纹理（避免重复加载同一 exe 的图标）
    std::unordered_map<std::wstring, Texture2D> m_exeIconCache;

    // ===== GPU 进程级使用率（PDH）=====
    // PDH 计数器 \GPU Engine(*)\Utilization Percentage 的实例名格式为：
    //   pid_XXXXX_luid_0x..._phys_N_engtype_3D
    // 解析其中的 pid 即可得到各进程的 GPU 使用率
    mutable PDH_HQUERY m_gpuQuery = nullptr;
    mutable PDH_HCOUNTER m_gpuCounter = nullptr;
    mutable bool m_gpuInitialized = false;
    mutable std::unordered_map<DWORD, float> m_procGpuCache;  // pid → 最大引擎使用率
    mutable DWORD m_procGpuLastUpdateTick = 0;

    // 刷新所有进程的 GPU 使用率缓存（每秒更新一次）
    void RefreshProcessGpuUsage() const
    {
        DWORD currentTick = GetTickCount64();
        if (currentTick - m_procGpuLastUpdateTick < 1000 && m_gpuInitialized)
            return;
        m_procGpuLastUpdateTick = currentTick;

        if (!m_gpuInitialized)
        {
            if (PdhOpenQuery(NULL, 0, &m_gpuQuery) == ERROR_SUCCESS)
                PdhAddEnglishCounterW(m_gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_gpuCounter);
            m_gpuInitialized = true;
            return;  // 首帧仅初始化，无数据可读
        }

        m_procGpuCache.clear();
        if (!m_gpuQuery || !m_gpuCounter)
            return;

        PdhCollectQueryData(m_gpuQuery);

        DWORD bufSize = 0, itemCount = 0;
        PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, NULL);
        if (bufSize == 0)
            return;

        std::vector<BYTE> buffer(bufSize);
        PDH_FMT_COUNTERVALUE_ITEM_W* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, items) != ERROR_SUCCESS)
            return;

        for (DWORD i = 0; i < itemCount; i++)
        {
            std::wstring instance = items[i].szName;
            // 实例名以 "pid_" 开头，其后跟随进程 ID
            if (instance.size() <= 4 || instance.compare(0, 4, L"pid_") != 0)
                continue;

            size_t start = 4;
            size_t end = instance.find(L'_', start);
            if (end == std::wstring::npos)
                continue;

            DWORD pid = (DWORD)_wtol(instance.substr(start, end - start).c_str());
            float usage = (float)items[i].FmtValue.doubleValue;
            if (usage < 0.0f) usage = 0.0f;
            if (usage > 100.0f) usage = 100.0f;

            // 同一进程可能有多个 GPU 引擎，取最大值（与 Windows 任务管理器一致）
            auto it = m_procGpuCache.find(pid);
            if (it == m_procGpuCache.end() || usage > it->second)
                m_procGpuCache[pid] = usage;
        }
    }

    // ===== 网络连接数统计（iphlpapi）=====
    // 注意：真正的进程级网络吞吐量需要 ETW (Event Tracing for Windows)，
    // 复杂度过高且需要管理员权限。这里统计每进程的已建立 TCP 连接数作为网络活动指标。
    mutable std::unordered_map<DWORD, int> m_procConnCache;  // pid → 连接数
    mutable DWORD m_procConnLastUpdateTick = 0;

    void RefreshProcessConnections() const
    {
        DWORD currentTick = GetTickCount64();
        if (currentTick - m_procConnLastUpdateTick < 2000)
            return;
        m_procConnLastUpdateTick = currentTick;

        m_procConnCache.clear();

        // IPv4 TCP 表（按 PID 分组，统计已建立连接数）
        DWORD tcpSize = 0;
        GetExtendedTcpTable(NULL, &tcpSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        if (tcpSize > 0)
        {
            std::vector<BYTE> tcpBuf(tcpSize);
            if (GetExtendedTcpTable(tcpBuf.data(), &tcpSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR)
            {
                MIB_TCPTABLE_OWNER_PID* tcpTable = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(tcpBuf.data());
                for (DWORD i = 0; i < tcpTable->dwNumEntries; i++)
                {
                    if (tcpTable->table[i].dwState == MIB_TCP_STATE_ESTAB)
                        m_procConnCache[tcpTable->table[i].dwOwningPid]++;
                }
            }
        }
    }

    int GetVisibleColumnCount() const
    {
        int count = 2;
        if (m_showPid) count++;
        if (m_showCpu) count++;
        if (m_showMemory) count++;
        if (m_showGpu) count++;
        if (m_showDisk) count++;
        if (m_showNetwork) count++;
        return count;
    }

    // 排序：根据当前排序列和方向对 m_processes 排序
    void SortProcesses()
    {
        // 将 ImGui 列索引映射为逻辑列（0=Icon, 1=Name, 之后按可见性递增）
        int col = m_sortColumn;
        enum SortField { Field_Name, Field_Pid, Field_Cpu, Field_Mem, Field_Gpu, Field_Disk, Field_Net } field = Field_Name;
        if (col == 1) field = Field_Name;
        else
        {
            int c = 2;
            if (m_showPid)    { if (col == c) field = Field_Pid; c++; }
            if (m_showCpu)    { if (col == c) field = Field_Cpu; c++; }
            if (m_showMemory) { if (col == c) field = Field_Mem; c++; }
            if (m_showGpu)    { if (col == c) field = Field_Gpu; c++; }
            if (m_showDisk)   { if (col == c) field = Field_Disk; c++; }
            if (m_showNetwork){ if (col == c) field = Field_Net; c++; }
        }

        // 三态比较器：返回 -1/0/1，保证 std::sort 满足严格弱序（自反性 comp(a,a)==false）
        auto CompareField = [](const ProcessInfo& a, const ProcessInfo& b, SortField field) -> int
        {
            switch (field)
            {
            case Field_Name:
            {
                std::string na = a.name, nb = b.name;
                for (auto& ch : na) ch = (char)tolower((unsigned char)ch);
                for (auto& ch : nb) ch = (char)tolower((unsigned char)ch);
                if (na < nb) return -1;
                if (na > nb) return 1;
                return 0;
            }
            case Field_Pid:  return (a.pid < b.pid) ? -1 : (a.pid > b.pid ? 1 : 0);
            case Field_Cpu:  return (a.cpuUsage < b.cpuUsage) ? -1 : (a.cpuUsage > b.cpuUsage ? 1 : 0);
            case Field_Mem:  return (a.memUsage < b.memUsage) ? -1 : (a.memUsage > b.memUsage ? 1 : 0);
            case Field_Gpu:  return (a.gpuUsage < b.gpuUsage) ? -1 : (a.gpuUsage > b.gpuUsage ? 1 : 0);
            case Field_Disk: return (a.diskMBps < b.diskMBps) ? -1 : (a.diskMBps > b.diskMBps ? 1 : 0);
            case Field_Net:  return (a.networkConns < b.networkConns) ? -1 : (a.networkConns > b.networkConns ? 1 : 0);
            }
            return 0;
        };

        bool asc = m_sortAscending;
        // 升序用 c < 0，降序用 c > 0（等价于比较参数交换，严格弱序）
        std::sort(m_processes.begin(), m_processes.end(),
            [field, asc, CompareField](const ProcessInfo& a, const ProcessInfo& b) {
                int c = CompareField(a, b, field);
                return asc ? c < 0 : c > 0;
            });
    }

    // 获取进程的系统图标纹理（类似 MusicDisplay 的方式）
    Texture2D GetProcessIconTexture(const ProcessInfo& proc)
    {
        // 先查 pid 缓存
        auto it = m_iconCache.find(proc.pid);
        if (it != m_iconCache.end())
            return it->second;

        // 再查 exe 路径缓存
        if (!proc.exePath.empty())
        {
            auto exeIt = m_exeIconCache.find(proc.exePath);
            if (exeIt != m_exeIconCache.end())
            {
                m_iconCache[proc.pid] = exeIt->second;
                return exeIt->second;
            }

            // 从 exe 提取图标
            Texture2D tex = ExtractIconFromExe(proc.exePath);
            if (tex)
            {
                m_exeIconCache[proc.exePath] = tex;
                m_iconCache[proc.pid] = tex;
                return tex;
            }
        }

        m_iconCache[proc.pid] = nullptr;
        return nullptr;
    }

    // 从 exe 文件提取图标并转换为 ImGui 纹理
    Texture2D ExtractIconFromExe(const std::wstring& exePath)
    {
        HICON hIcon = ExtractIconW(NULL, exePath.c_str(), 0);
        if (!hIcon || (INT_PTR)hIcon == 1)
            return nullptr;

        Texture2D tex = IconToTexture(hIcon);
        DestroyIcon(hIcon);
        return tex;
    }

    // 将 HICON 转换为 RGBA 纹理
    Texture2D IconToTexture(HICON hIcon)
    {
        ICONINFO iconInfo;
        if (!GetIconInfo(hIcon, &iconInfo))
            return nullptr;

        BITMAP bm;
        GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bm);

        int width = bm.bmWidth;
        int height = bm.bmHeight;
        if (width <= 0 || height <= 0)
        {
            DeleteObject(iconInfo.hbmColor);
            DeleteObject(iconInfo.hbmMask);
            return nullptr;
        }

        HDC hdc = GetDC(NULL);
        HDC memDC = CreateCompatibleDC(hdc);

        // 准备 BITMAPINFO 用于 GetDIBits
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = width;
        bi.bmiHeader.biHeight = -height;  // 自上而下
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        std::vector<unsigned char> pixels(width * height * 4);
        HBITMAP hbmColor = CreateCompatibleBitmap(hdc, width, height);
        HGDIOBJ oldObj = SelectObject(memDC, hbmColor);

        // 绘制图标到 DC
        DrawIconEx(memDC, 0, 0, hIcon, width, height, 0, NULL, DI_NORMAL);

        // 读取像素
        GetDIBits(memDC, hbmColor, 0, height, pixels.data(), &bi, DIB_RGB_COLORS);

        // 转换 BGRA → RGBA
        for (int i = 0; i < width * height; i++)
        {
            std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);  // B ↔ R
        }

        SelectObject(memDC, oldObj);
        DeleteObject(hbmColor);
        DeleteDC(memDC);
        ReleaseDC(NULL, hdc);
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);

        // 生成唯一 ID
        static int iconIdCounter = 0;
        std::string texId = "ProcIcon_" + std::to_string(iconIdCounter++);

        int w = width, h = height;
        return GuiBase::LoadTexture(pixels.data(), w, h, nullptr, nullptr, texId.c_str());
    }

    static ImVec4 GetProcessColor(const std::string& name)
    {
        std::string lower = name;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);

        if (lower.find("explorer") != std::string::npos)  return ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
        if (lower.find("chrome") != std::string::npos)    return ImVec4(0.26f, 0.73f, 0.96f, 1.0f);
        if (lower.find("code") != std::string::npos)      return ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
        return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }

    // 系统关键进程黑名单：不可交互、与桌面用户无关的内核/会话进程
    // 这些进程即使枚举到也不在列表中显示（对应 Windows 任务管理器默认视图）
    static bool IsSystemProcessName(const std::string& name)
    {
        std::string lower = name;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);

        return lower == "system" || lower == "registry" || lower == "memory compression"
            || lower == "smss.exe" || lower == "csrss.exe" || lower == "wininit.exe"
            || lower == "winlogon.exe" || lower == "services.exe" || lower == "lsass.exe"
            || lower == "fontdrvhost.exe";
    }

    static std::string FormatMemory(SIZE_T bytes)
    {
        char buf[32];
        if (bytes < 1024ULL * 1024)
            sprintf_s(buf, "%.0f KB", (double)bytes / 1024.0);
        else if (bytes < 1024ULL * 1024 * 1024)
            sprintf_s(buf, "%.1f MB", (double)bytes / (1024.0 * 1024));
        else
            sprintf_s(buf, "%.2f GB", (double)bytes / (1024.0 * 1024 * 1024));
        return buf;
    }

    static std::string WideToUtf8(const std::wstring& wstr)
    {
        if (wstr.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string result(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], len, nullptr, nullptr);
        return result;
    }

    void RefreshProcesses()
    {
        // 记录物理内存总量（供内存列迷你进度条计算比例）
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus))
            m_totalMemBytes = memStatus.ullTotalPhys;

        // 系统级 CPU 时间
        FILETIME sysIdle, sysKernel, sysUser;
        GetSystemTimes(&sysIdle, &sysKernel, &sysUser);
        ULARGE_INTEGER curSysKernel, curSysUser;
        curSysKernel.LowPart = sysKernel.dwLowDateTime;
        curSysKernel.HighPart = sysKernel.dwHighDateTime;
        curSysUser.LowPart = sysUser.dwLowDateTime;
        curSysUser.HighPart = sysUser.dwHighDateTime;

        ULONGLONG sysKernelDiff = 0, sysUserDiff = 0;
        if (!m_firstSysSample)
        {
            sysKernelDiff = curSysKernel.QuadPart - m_prevSysKernel;
            sysUserDiff = curSysUser.QuadPart - m_prevSysUser;
        }
        m_prevSysKernel = curSysKernel.QuadPart;
        m_prevSysUser = curSysUser.QuadPart;
        m_firstSysSample = false;

        ULONGLONG sysTotalDiff = sysKernelDiff + sysUserDiff;

        // 获取当前会话 ID，用于过滤系统服务进程（session 0）
        DWORD currentSessionId = 0;
        ProcessIdToSessionId(GetCurrentProcessId(), &currentSessionId);

        DWORD processes[1024], bytesReturned;
        if (!EnumProcesses(processes, sizeof(processes), &bytesReturned))
            return;

        DWORD processCount = bytesReturned / sizeof(DWORD);
        std::vector<ProcessInfo> newProcesses;

        // 注意：不清空 m_iconCache，避免图标频繁重载导致闪烁
        // 仅移除已不存在的 pid 缓存项
        std::unordered_set<DWORD> activePids;

        for (DWORD i = 0; i < processCount; i++)
        {
            DWORD pid = processes[i];
            if (pid == 0) continue;
            activePids.insert(pid);

            // 过滤系统服务进程：只显示当前用户会话中的进程
            DWORD procSessionId = 0;
            if (ProcessIdToSessionId(pid, &procSessionId) && procSessionId != currentSessionId)
                continue;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (!hProcess) continue;

            ProcessInfo info;
            info.pid = pid;

            // 进程名称和 exe 路径
            HMODULE hMod;
            DWORD cbNeeded;
            if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
            {
                wchar_t szProcessName[MAX_PATH] = L"<unknown>";
                GetModuleBaseNameW(hProcess, hMod, szProcessName, MAX_PATH);
                info.name = WideToUtf8(szProcessName);

                // 忽略系统关键进程（System/会话管理等，见 IsSystemProcessName 黑名单）
                // 忽略系统关键进程（System/会话管理等，见 IsSystemProcessName 黑名单）
                // 可通过配置项 m_showSystemProcess 关闭过滤
                if (!m_showSystemProcess && IsSystemProcessName(info.name))
                {
                    CloseHandle(hProcess);
                    continue;
                }

                // 获取完整 exe 路径（用于提取图标）
                wchar_t szExePath[MAX_PATH] = {};
                if (GetModuleFileNameExW(hProcess, hMod, szExePath, MAX_PATH))
                    info.exePath = szExePath;
            }
            else
            {
                // 无法枚举模块的进程通常是系统服务或权限不足，跳过
                CloseHandle(hProcess);
                continue;
            }

            // 内存
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
                info.memUsage = pmc.WorkingSetSize;

            // CPU
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser))
            {
                ULARGE_INTEGER curKernel, curUser;
                curKernel.LowPart = ftKernel.dwLowDateTime;
                curKernel.HighPart = ftKernel.dwHighDateTime;
                curUser.LowPart = ftUser.dwLowDateTime;
                curUser.HighPart = ftUser.dwHighDateTime;

                auto it = m_prevProcessData.find(pid);
                if (it != m_prevProcessData.end() && sysTotalDiff > 0)
                {
                    ULONGLONG procKernelDiff = curKernel.QuadPart - it->second.prevProcKernel;
                    ULONGLONG procUserDiff = curUser.QuadPart - it->second.prevProcUser;
                    ULONGLONG procTotalDiff = procKernelDiff + procUserDiff;

                    info.cpuUsage = (float)((double)procTotalDiff / (double)sysTotalDiff * 100.0);
                    if (info.cpuUsage < 0.0f) info.cpuUsage = 0.0f;
                    if (info.cpuUsage > 100.0f) info.cpuUsage = 100.0f;
                }

                info.prevProcKernel = curKernel.QuadPart;
                info.prevProcUser = curUser.QuadPart;
            }

            // 磁盘 I/O
            IO_COUNTERS ioCounters;
            if (GetProcessIoCounters(hProcess, &ioCounters))
            {
                auto it = m_prevProcessData.find(pid);
                if (it != m_prevProcessData.end())
                {
                    ULONGLONG readDiff = ioCounters.ReadTransferCount - it->second.prevReadBytes;
                    ULONGLONG writeDiff = ioCounters.WriteTransferCount - it->second.prevWriteBytes;
                    info.diskMBps = (float)((double)(readDiff + writeDiff) / (1024.0 * 1024.0) / 2.0);
                    if (info.diskMBps < 0.0f) info.diskMBps = 0.0f;
                }

                info.prevReadBytes = ioCounters.ReadTransferCount;
                info.prevWriteBytes = ioCounters.WriteTransferCount;
            }

            // GPU 使用率 — 通过 PDH GPU Engine 计数器获取进程级 GPU 使用率
            info.gpuUsage = GetProcessGpuUsage(pid);

            // 网络数据 — 统计已建立的 TCP 连接数
            info.networkConns = GetProcessConnectionCount(pid);

            CloseHandle(hProcess);
            newProcesses.push_back(std::move(info));
        }

        // 清理已不存在的 pid 的图标缓存
        for (auto it = m_iconCache.begin(); it != m_iconCache.end(); )
        {
            if (activePids.find(it->first) == activePids.end())
                it = m_iconCache.erase(it);
            else
                ++it;
        }

        // 默认按内存占用降序排列（未点击表头时的默认视图，与内存列的 DefaultSort 标志一致）
        if (m_sortColumn == -1)
        {
            std::sort(newProcesses.begin(), newProcesses.end(),
                [](const ProcessInfo& a, const ProcessInfo& b) {
                    return a.memUsage > b.memUsage;
                });
        }
        else
        {
            // 用户指定了排序列，使用 SortProcesses
            m_processes = std::move(newProcesses);
            SortProcesses();
            // 保存上一帧的进程数据用于下次 CPU 计算
            m_prevProcessData.clear();
            for (auto& proc : m_processes)
                m_prevProcessData[proc.pid] = proc;
            return;
        }

        m_prevProcessData.clear();
        for (auto& proc : newProcesses)
            m_prevProcessData[proc.pid] = proc;

        m_processes = std::move(newProcesses);
    }

    // 获取进程 GPU 使用率（通过 PDH GPU Engine 计数器）
    float GetProcessGpuUsage(DWORD pid)
    {
        RefreshProcessGpuUsage();
        auto it = m_procGpuCache.find(pid);
        return it != m_procGpuCache.end() ? it->second : 0.0f;
    }

    // 获取进程网络连接数（通过 GetExtendedTcpTable 统计已建立的 TCP 连接）
    // 返回连接数（非吞吐量），真正的吞吐量需要 ETW
    int GetProcessConnectionCount(DWORD pid)
    {
        RefreshProcessConnections();
        auto it = m_procConnCache.find(pid);
        return it != m_procConnCache.end() ? it->second : 0;
    }
};
