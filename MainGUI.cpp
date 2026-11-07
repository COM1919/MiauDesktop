#include "MainGUI.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef MD_DEBUG_CMD
    if (!AllocConsole())
    {
        OutputDebugString(L"[Init] 无法分配控制台窗口");
        return -2;
    }
    // 设置标准输入输出错误句柄到新控制台
    FILE* stream;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);

    HWND ConsoleHwnd = GetConsoleWindow();
    RECT rect;
    GetWindowRect(ConsoleHwnd, &rect);
    SetWindowPos(ConsoleHwnd, NULL, 0, 0, rect.right - rect.left + 100, rect.bottom - rect.top + 20, SWP_NOMOVE);
#endif // MD_DEBUG_CMD

    HANDLE hMutex = CreateMutex(NULL, TRUE, L"Global\\MiauDesktop\\Mutex");
    // 检查互斥量是否已存在
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // 互斥量已存在，说明程序已在运行
        MessageBox(NULL, L"你已经打开了一个 MiauDesktop 了", L"提示", MB_OK);
        CloseHandle(hMutex);
        return 0;
    }

    // 加载配置
    Config::Instance().SetConfigFile(FilePath::Config);
    // 注册全局设置项（软件自身设置，挂在配置根节点下，非组件名）
    Config::Instance().AddItem("Settings.Animations.Enabled", true);
    Config::Instance().AddItem("Settings.Animations.Speed", 1.0f);
    Config::Instance().AddItem("Settings.Animations.Ease", 5);   // 5 = OutCubic
    Config::Instance().LoadConfig();
    // 把配置值分发到各组件变量（注册机制：变量在加载/保存时由配置系统读写）
    g_compManager.LoadAllConfigs();
    // 加载全局设置（动画开关 / 速度 / 优先公式）到 AppSettings 缓存
    AppSettings::LoadGlobalSettings();
    // 将全局动画设置注册给 GuiBase::GuiAnimatedProgress（组件动画统一走全局设置）
    GuiBase::GuiAnimatedProgress::SetAnimationSettingsProvider(
        AppSettings::IsAnimationEnabled,
        AppSettings::GetAnimationSpeed,
        AppSettings::IsLerpPreferred,
        AppSettings::GetPreferredEase);

    // 创建窗口，然后渲染GUI，这就是入口函数了
    RenderGui(hInstance);

    // 释放互斥量句柄
    CloseHandle(hMutex);
}

int RenderGui(HINSTANCE hInstance)
{
    // 使进程 DPI 意识并获取主显示器缩放比例
    ImGui_ImplWin32_EnableDpiAwareness();
    MONITORINFO mi = { sizeof(mi) };
    // 使用 GetMonitorInfo 获取更准确的信息，包括 DPI 缩放影响
    GetMonitorInfo(MonitorFromPoint(POINT{ 0,0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    const int ScreenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    const int ScreenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    const float MainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
     
    // 创建窗口
    WNDCLASSEX wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"MiauDesktop", nullptr };
    RegisterClassEx(&wc);
    // WS_EX_NOREDIRECTIONBITMAP：不分配 GDI 重定向表面，改由 DirectComposition 接管渲染
    // 这样配合 premultiplied alpha swap chain 即可实现 per-pixel 半透明（替代旧的 LWA_COLORKEY 色键方案）
    g_hWnd = CreateWindowEx(WS_EX_TRANSPARENT | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW, // WS_EX_TOOLWINDOW：不在任务栏显示
        wc.lpszClassName, L"MiauDesktop", WS_POPUP, 0, 0,
        ScreenWidth, ScreenHeight, nullptr, nullptr, wc.hInstance, nullptr);
	if (!g_hWnd)
	{
		std::cerr << "[Render] 无法创建窗口" << std::endl;
		return 1;
	}
    // 注意：不再调用 SetLayeredWindowAttributes
    // 旧方案用 LWA_COLORKEY 色键（纯黑=透明），无法实现真正的半透明
    // 新方案用 DirectComposition + premultiplied alpha，支持任意 alpha 值的半透明渲染

#ifndef MD_TOPSHOW
    HWND hProgman = FindWindowW(L"Progman", NULL);
    if (hProgman) // 将窗口的 Owner 设置为桌面，注意这里用的是 GWLP_HWNDPARENT 而不是 SetParent
        SetWindowLongPtr(g_hWnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
#endif // MD_TOPSHOW

    // 初始化 Direct3D
    if (!CreateDeviceD3D(g_hWnd))
    {
		std::cerr << "[Render] 无法创建 Direct3D 设备" << std::endl;
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 2;
    }

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    // 初始化系统托盘图标
    AppSettings::InitTrayIcon(g_hWnd);

    // 设置 ImGui 上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 设置 ImGui 样式
    ImGui::StyleColorsDark();
    SetupImGuiStyle();

    // 设置 DPI 缩放
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(MainScale); // 一个固定的风格比例。（在我们找到动态样式缩放的解决方案之前，更改此项需要重置style+并再次调用此项）
    style.FontScaleDpi = MainScale; // 设置初始字体比例。（在停靠分支中：使用io.ConfigDpiScaleFonts=true会根据当前监视器自动覆盖每个窗口）

    // 设置平台/渲染器后端
    if (!ImGui_ImplWin32_Init(g_hWnd))
    {
        std::cerr << "[Render] 无法初始化 Win32 平台后端" << std::endl;
        return 3;
    }
    ImGui_ImplDX11_Init(RenderData::g_pd3dDevice, RenderData::g_pd3dDeviceContext);

    // 加载字体
    ImFontConfig FontConfig;
    FontConfig.SizePixels = 18.0f * MainScale;
    FontConfig.PixelSnapH = true;
    FontConfig.OversampleH = 1;
    FontConfig.OversampleV = 1;
    ImFont* MainFont = io.Fonts->AddFontFromFileTTF(
        FilePath::Font.c_str(), 18.0f, &FontConfig, io.Fonts->GetGlyphRangesChineseFull()  // 中文范围
    );
    if (!MainFont)
    {
        std::cerr << "[Render] 无法加载字体" << std::endl;
        return 4; // 在入口函数里面return，退出程序
    }

    // 合并 Font Awesome 图标字体
    ImFontConfig IconConfig;
    IconConfig.MergeMode = true;
    IconConfig.PixelSnapH = true;
    IconConfig.OversampleH = 1;
    IconConfig.OversampleV = 1;
    //IconConfig.GlyphOffset.y = 1.0f;  // 正数向下移动，1.0f 是向下
    //IconConfig.GlyphOffset.x = -1.0f; // 正数向右偏移，-1.0f 是向左

    static const ImWchar IconsRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFont* IconFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        (void*)FA_compressed_data, FA_compressed_size, 16.0f * MainScale, &IconConfig, IconsRanges
    );

	if (!IconFont) // 非致命错误，可不退出程序，但会影响图标显示
        std::cout << "[Render] 无法加载图标字体" << std::endl;
    io.Fonts->Build();

    // 在代码层面，添加和注册组件
    // 注意：一定要在ImGui初始化后才注册，不然Components类使用ImGui::GetIO时，如果ImGui没初始化就会崩溃
    g_compManager.AddComponent(std::make_unique<Components::PerformanceMonitor>());
    g_compManager.AddComponent(std::make_unique<Components::ProcessManager>());
    g_compManager.AddComponent(std::make_unique<Components::DebugDemo>());
    g_compManager.AddComponent(std::make_unique<Components::MusicDisplay>());

	bool isQuit = false;
    while (!isQuit)
    {
		// 处理 Windows 消息循环
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                isQuit = true;
        }
        // 处理程序退出
        if (isQuit)
            break;

        // 处理窗口最小化或屏幕锁定
        if (RenderData::g_SwapChainOccluded && RenderData::g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            Sleep(20);
            continue;
        }
        RenderData::g_SwapChainOccluded = false;

        // 滑动条动画处理
        ScrollAnim::HandleScrolling();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        {
            // 编辑模式前置处理：拖拽检测和对齐计算
            AppSettings::PreDraw();

            // 清空上一帧的组件 ID 列表
            g_compManager.ResetComponentWindowList();
            // 绘画所有可见组件
            g_compManager.DrawAll();
            // 用 SetWindowRgn 裁剪窗口区域为组件矩形并集，区域外系统层面自动穿透
            UpdateWindowRgn();

            // 后置处理：设置窗口 + 编辑模式 UI（辅助线、边框、按钮）
            AppSettings::PostDraw();
        }
        ImGui::Render();
        RenderData::g_pd3dDeviceContext->OMSetRenderTargets(1, &RenderData::g_mainRenderTargetView, nullptr);
        const float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        RenderData::g_pd3dDeviceContext->ClearRenderTargetView(RenderData::g_mainRenderTargetView, color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		HRESULT hr = RenderData::g_pSwapChain->Present(1, 0);   // 使用垂直同步（VSync）
		//HRESULT hr = RenderData::g_pSwapChain->Present(0, 0); // 不使用垂直同步
        RenderData::g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // 我的任务完成啦，开始清理....
    // 保存配置：先把各组件变量写回配置树，再写入文件
    g_compManager.SaveAllConfigs();
    Config::Instance().WriteConfig();

    // 先显式析构所有组件（在静态析构阶段之前执行）：
    // 组件析构会释放纹理缓存中的资源，此时纹理缓存仍然有效，
    // 若等到 main 返回后的全局静态析构阶段才析构组件，
    // 可能因全局对象销毁顺序未定义而访问已销毁的容器导致崩溃
    g_compManager.Clear();

    AppSettings::RemoveTrayIcon();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

void ScrollAnim::HandleScrolling()
{
    ImGuiIO& io = ImGui::GetIO();
    // 处理垂直滚动
    float scroll_now_y = 0.0f;
    if (fabs(ScrollAnim::g_EnergyY) > 0.001f)
    {
        scroll_now_y = ScrollAnim::g_EnergyY * io.DeltaTime * ScrollAnim::SMOOTHING;
        ScrollAnim::g_EnergyY -= scroll_now_y;
    }
    else
        ScrollAnim::g_EnergyY = 0.0f;

    // 处理水平滚动（如果有）
    float scroll_now_x = 0.0f;
    if (fabs(ScrollAnim::g_EnergyX) > 0.001f)
    {
        scroll_now_x = ScrollAnim::g_EnergyX * io.DeltaTime * ScrollAnim::SMOOTHING;
        ScrollAnim::g_EnergyX -= scroll_now_x;
    }
    else
        ScrollAnim::g_EnergyX = 0.0f;

    // 将平滑值赋给 IO（注意符号：ImGui 的正方向与 Win32 可能相反）
    io.MouseWheel = scroll_now_y / 2.0f;      // 垂直
    io.MouseWheelH = -scroll_now_x;    // 水平（可能需要负号）
}

void UpdateWindowRgn()
{
    if (!g_hWnd)
        return;

    // 编辑模式/设置窗口：使用整个窗口区域（NULL）
    if (EditMode::g_active || AppSettings::g_showSettings)
    {
        SetWindowRgn(g_hWnd, NULL, FALSE);
        return;
    }

    // 构建所有可见组件窗口矩形的并集区域（含阴影扩展范围）
    // 阴影参数：blur=24, offsetY=8，向四周扩展 blur，向底部额外扩展 offsetY
    const float shadowExpand = 26.0f;   // 略大于 blur=24，确保阴影完全包含
    const float shadowBottomExtra = 10.0f; // 略大于 offsetY=8

    HRGN hrgnCombined = CreateRectRgn(0, 0, 0, 0);  // 起始空区域
    bool hasAny = false;
    const auto& comps = g_compManager.GetComponents();
    for (const auto& comp : comps)
    {
        if (!comp->IsVisible())
            continue;
        // 跳过 DebugDemo 组件（不参与 EUI 渲染，但仍需包含在裁剪区域中）
        ImGuiWindow* win = ImGui::FindWindowByName(comp->GetName().c_str());
        if (win)
        {
            // 扩展区域以包含阴影：上/左/右扩展 shadowExpand，底部扩展 shadowExpand + shadowBottomExtra
            HRGN hrgnChild = CreateRectRgn(
                (int)(win->Pos.x - shadowExpand),
                (int)(win->Pos.y - shadowExpand),
                (int)(win->Pos.x + win->Size.x + shadowExpand) + 1,
                (int)(win->Pos.y + win->Size.y + shadowExpand + shadowBottomExtra) + 1);
            CombineRgn(hrgnCombined, hrgnCombined, hrgnChild, RGN_OR);
            DeleteObject(hrgnChild);
            hasAny = true;
        }
    }

    // 设置窗口区域：
    // - 有组件：裁剪为并集区域，区域外系统层面自动穿透
    // - 无组件：空区域（完全穿透，窗口不可点击但 DComp 仍渲染清屏）
    // 注意：SetWindowRgn 后 hrgnCombined 所有权交给系统，不要 DeleteObject
    // FALSE = 不立即触发重绘（我们每帧都在渲染，不需要系统重绘）
    SetWindowRgn(g_hWnd, hrgnCombined, FALSE);
}

void SetupImGuiStyle()
{
    // Moonlight style by Madam-Herta from ImThemes
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.WindowRounding = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowMinSize = ImVec2(20.0f, 20.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ChildRounding = 12.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 12.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(20.0f, 3.4f);
    style.FrameRounding = 12.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(4.3f, 5.5f);
    style.ItemInnerSpacing = ImVec2(7.1f, 1.8f);
    style.CellPadding = ImVec2(12.1f, 9.2f);
    style.IndentSpacing = 0.0f;
    style.ColumnsMinSpacing = 4.9f;
    style.ScrollbarSize = 11.6f;
    style.ScrollbarRounding = 15.9f;
    style.GrabMinSize = 3.7f;
    style.GrabRounding = 20.0f;
    style.TabRounding = 12.0f;
    style.TabBorderSize = 0.0f;
    style.TabCloseButtonMinWidthSelected = 0.0f;
    style.TableAngledHeadersAngle = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(0.776f, 0.816f, 0.961f, 1.0f);         // Catppuccin text #c6d0f5
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.647f, 0.678f, 0.808f, 1.0f); // subtext0 #a5adce
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 0.88f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09411765f, 0.101960786f, 0.11764706f, 0.88f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11372549f, 0.1254902f, 0.15294118f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.972549f, 1.0f, 0.49803922f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.972549f, 1.0f, 0.49803922f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.79607844f, 0.49803922f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);              // 默认透明（与背景一致）
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.149f, 0.153f, 0.169f, 1.0f); // (38,39,43) 比背景淡
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.165f, 0.173f, 0.188f, 1.0f);  // (42,44,48) 按下略亮
    style.Colors[ImGuiCol_Header] = ImVec4(0.14117648f, 0.16470589f, 0.20784314f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.105882354f, 0.105882354f, 0.105882354f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.12941177f, 0.14901961f, 0.19215687f, 1.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.14509805f, 0.14509805f, 0.14509805f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.972549f, 1.0f, 0.49803922f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1254902f, 0.27450982f, 0.57254905f, 1.0f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.52156866f, 0.6f, 0.7019608f, 1.0f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.039215688f, 0.98039216f, 0.98039216f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.88235295f, 0.79607844f, 0.56078434f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.95686275f, 0.95686275f, 0.95686275f, 1.0f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.102f, 0.110f, 0.125f, 1.0f);  // (26,28,32) 深灰细线
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9372549f, 0.9372549f, 0.9372549f, 1.0f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.26666668f, 0.2901961f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    case WM_SYSCOMMAND: // 拦截系统命令（阻止 "显示桌面" 发送的最小化指令）
    {
        if ((wParam & 0xFFF0) == SC_MINIMIZE)
            return 0;    // 直接丢弃最小化命令，不处理
        break;           // 其他系统命令正常放行
    }
#ifndef MD_TOPSHOW
    case WM_WINDOWPOSCHANGING: // 拦截窗口位置/状态改变消息
    {
        WINDOWPOS* pwp = (WINDOWPOS*)lParam;
        // 强制将其置于最底层
        pwp->hwndInsertAfter = HWND_BOTTOM;
        // 如果系统试图隐藏窗口，则取消隐藏标志
        if (pwp->flags & SWP_HIDEWINDOW)
            pwp->flags &= ~SWP_HIDEWINDOW;
        // 防止窗口在操作中被意外激活
        pwp->flags |= SWP_NOACTIVATE;
        break;
    }
    case WM_SIZE: // 兜底保护：如果通过其他底层 API 强行最小化了，立刻无痕迹恢复
    {
        if (wParam == SIZE_MINIMIZED)
        {
            // 立即恢复窗口显示，但不抢夺焦点
            ShowWindow(hWnd, SW_SHOWNOACTIVATE);
            return 0;
        }
        break;
    }
#endif // MD_TOPSHOW
    case WM_TRAYICON:
    {
        if (lParam == WM_RBUTTONUP)
        {
            // 右键点击托盘图标：显示菜单
            AppSettings::ShowTrayMenu();
        }
        else if (lParam == WM_LBUTTONUP)
        {
            // 左键点击托盘图标：直接打开设置窗口
            AppSettings::g_showSettings = true;
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        // 获取滚轮增量（通常是120的倍数）
        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        // 转换为浮点数，并累加（注意方向：正值表示向上滚动）
        float delta = (float)zDelta / 120.0f; // 归一化到“格”
        delta *= ScrollAnim::SCROLL_MULTIPLIER;

        // 如果滚动方向改变，清零能量，防止卡顿
        if (ScrollAnim::g_EnergyY * delta < 0.0f) ScrollAnim::g_EnergyY = 0.0f;
        ScrollAnim::g_EnergyY += delta;
        // 可选：横向滚动（需要WM_MOUSEHWHEEL）
        return 0; // 表示已处理
    }
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}