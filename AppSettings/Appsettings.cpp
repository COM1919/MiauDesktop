#include "AppSettings.h"
#include "../Components/PerformanceMonitor.h"
#include "../Components/ProcessManager.h"

// 初始化托盘图标（使用系统默认图标作为占位）
void AppSettings::InitTrayIcon(HWND hWnd)
{
    g_hWnd = hWnd;
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"MiauDesktop");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

// 移除托盘图标
void AppSettings::RemoveTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

// 显示托盘右键菜单
void AppSettings::ShowTrayMenu()
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, 1, L"设置");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, 2, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hWnd);

    int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, 0, g_hWnd, NULL);

    switch (cmd)
    {
    case 1:
        g_showSettings = true;
        break;
    case 2:
        PostQuitMessage(0);
        break;
    }
    DestroyMenu(hMenu);
}

// 初始化设置界面
void AppSettings::InitSettingsUI()
{
    if (g_navPanel) return;
    g_navPanel = new NavPanel();
    g_filterBar = new FilterBar();
    EnterSettingsUI();
}

// 销毁设置界面
void AppSettings::CleanupSettingsUI()
{
    for (auto* card : g_moduleCards) delete card;
    g_moduleCards.clear();
    delete g_navPanel; g_navPanel = nullptr;
    delete g_filterBar; g_filterBar = nullptr;
}

// 进入设置界面时重建卡片列表
void AppSettings::EnterSettingsUI()
{
    for (auto* card : g_moduleCards) delete card;
    g_moduleCards.clear();
    for (auto& comp : g_compManager.GetComponents())
        g_moduleCards.push_back(new ComponentCard(comp.get()));
    g_currentPage = SettingsPage::ComponentList;
    g_isInModuleSettings = false;
    g_selectedComponent = nullptr;
    g_lastSelectedIndex = 0;
    if (g_navPanel)
        g_navPanel->SetSelectedIndex(0);
}

// 主设置界面
// 左栏导航 + 主面板，组件以动画卡片形式展示
void AppSettings::RenderSettingsWindow()
{
    if (!g_showSettings)
        return;

    // 初始化
    if (!g_navPanel)
        InitSettingsUI();

    // 确保 popup 处于打开状态（从编辑模式返回时需要重新打开）
    if (!ImGui::IsPopupOpen("设置"))
        ImGui::OpenPopup("设置");

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 displaySize = io.DisplaySize;

    // 设置窗口大小
    ImGui::SetNextWindowSize(settingMenuSize, ImGuiCond_Appearing);
    // 设置窗口始终置顶
    ImGui::SetNextWindowFocus();

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    bool open = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    // 背景淡入动画（alpha 从 0 渐变到目标值）
    const float targetAlpha = 0.5f;
    const float speed = 0.2f * (1.0f - ImGui::GetIO().DeltaTime);
    g_animAlpha = ImLerp(g_animAlpha, targetAlpha, speed);
    // 设置界面专属主题（集中 Push 颜色；组件渲染用全局默认，互不影响）
    PushSettingsTheme(g_animAlpha);

    if (ImGui::BeginPopupModal("设置", &open, flags))
    {
        ImGui::PopStyleVar(); // ImGuiStyleVar_WindowPadding
        PopSettingsTheme();   // 设置界面主题

        // 整个设置窗口统一渐变背景（覆盖全窗口，不分区）
        // 深色主题：所有背景色 rgb ≤ 40（带冷蓝调，不发灰）
        // 圆角：渐变内缩 12px（PopupRounding），四角圆角区域由 ImGui 绘制的圆角 WindowBg 显示
        {
            ImDrawList* winDraw = ImGui::GetWindowDrawList();
            ImVec2 wMin = ImGui::GetWindowPos();
            ImVec2 wMax = ImVec2(wMin.x + settingMenuSize.x, wMin.y + settingMenuSize.y);
            const float radius = 12.0f;  // 与 PopupRounding 一致
            ImVec2 gMin = wMin + ImVec2(radius, radius);
            ImVec2 gMax = wMax - ImVec2(radius, radius);
            winDraw->AddRectFilledMultiColor(gMin, gMax,
                IM_COL32(30, 32, 38, 255),   // 左上：略亮
                IM_COL32(27, 29, 35, 255),   // 右上
                IM_COL32(18, 20, 26, 255),   // 右下：最深
                IM_COL32(21, 23, 29, 255));  // 左下
        }

        // 左栏Logo + 导航（透明背景，透出统一渐变）
        ImVec2 screenPos = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::BeginChild("##LeftPanel", ImVec2(settingsPadding * 2 + 150.0f, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        {
            // 标题（全局 Text 色已是 Catppuccin text #c6d0f5）
            ImGui::SetCursorPos(ImVec2(settingsPadding, 20.0f));
            ImGui::PushFont(NULL, ImGui::GetFontSize() * 1.3f);
            ImGui::TextUnformatted("MiauDesktop");
            ImGui::PopFont();

            // 导航按钮
            ImGui::SetCursorPos(ImVec2(settingsPadding, 65.0f));
            ImGui::PushFont(NULL, ImGui::GetFontSize() * 1.2f);

            // 重建卡片列表（首次进入时）
            if (g_moduleCards.empty() && !g_compManager.GetComponents().empty())
                EnterSettingsUI();

            if (g_navPanel->Draw(settingMenuSize.y))
            {
                int index = g_navPanel->GetSelectedIndex();
                switch (index)
                {
                case 0: // 组件列表
                    g_currentPage = SettingsPage::ComponentList;
                    g_isInModuleSettings = false;
                    g_selectedComponent = nullptr;
                    break;
                case 1: // 位置调整
                    g_showSettings = false;
                    g_lastSelectedIndex = g_navPanel ? g_navPanel->GetSelectedIndex() : 0;  // 记录进入前的导航页
                    SaveConfigNow();  // 进入位置调整前保存一次设置
                    EditMode::g_active = true;
                    EditMode::g_draggedWindow.clear();
                    EditMode::g_resizingWindow.clear();
                    EditMode::g_resizeDir = EditMode::ResizeDir_None;
                    EditMode::g_hoverResizeDir = EditMode::ResizeDir_None;
                    EditMode::g_hoverResizeWindow.clear();
                    break;
                case 2: // 全局设置（软件自身设置）
                    g_currentPage = SettingsPage::GlobalSettings;
                    g_isInModuleSettings = false;
                    g_selectedComponent = nullptr;
                    break;
                case 3: // 退出程序
                    PostQuitMessage(0);
                    break;
                }
            }
            ImGui::PopFont();

            // 分隔线（低对比淡白，前端层次：弱化分割线）
            ImVec2 sepPos = ImGui::GetCursorScreenPos();
            sepPos.x -= settingsPadding * 0.5f;
            sepPos.y += settingsPadding;
            draw->AddLine(sepPos, ImVec2(sepPos.x + settingsPadding + 150.0f, sepPos.y),
                IM_COL32(255, 255, 255, 26), 1.0f);
        }
        ImGui::EndChild();

        // 左栏右侧分隔线（低对比淡白）
        ImVec2 linePos = ImVec2(settingsPadding * 2 + 150.0f + screenPos.x, screenPos.y);
        draw->AddLine(linePos, ImVec2(linePos.x, settingMenuSize.y + screenPos.y),
            IM_COL32(255, 255, 255, 26), 1.0f);

        // 主面板（透明背景，透出统一渐变）
        ImGui::SetCursorPos(ImVec2(settingsPadding * 2 + 150.0f, 0));
        ImGui::BeginChild("##MainPanel", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        {
            ImVec2 mainScreenPos = ImGui::GetCursorScreenPos();

            // 标题行分隔线（低对比淡白）
            ImVec2 titlePos1 = ImVec2(mainScreenPos.x + settingsPadding * 0.5f, mainScreenPos.y + 60);
            ImVec2 titlePos2 = ImVec2(mainScreenPos.x + mainPanelSize.x - settingsPadding * 0.5f, mainScreenPos.y + 60);
            draw->AddLine(titlePos1, titlePos2, IM_COL32(255, 255, 255, 26), 1.0f);

            ImGui::PushFont(NULL, ImGui::GetFontSize() * 1.5f);
            const char* pageTitle = (g_currentPage == SettingsPage::GlobalSettings) ? "设置" : "组件管理";
            ImVec2 titleSize = ImGui::CalcTextSize(pageTitle);
            ImGui::SetCursorScreenPos(ImVec2(mainScreenPos.x + settingsPadding, mainScreenPos.y + 60 - titleSize.y - settingsPadding * (1.0f - 0.618f)));
            ImGui::TextUnformatted(pageTitle);
            ImGui::PopFont();

            // 主面板内容
            if (g_isInModuleSettings && g_selectedComponent)
                RenderComponentSettings(g_selectedComponent);
            else if (g_currentPage == SettingsPage::GlobalSettings)
                RenderGlobalSettings();
            else
            {
                // 搜索框
                ImGui::SetCursorScreenPos(ImVec2(mainScreenPos.x + settingsPadding, mainScreenPos.y + 60 + settingsPadding * 0.625f));
                g_filterBar->Draw();

                // 组件卡片列表（可滚动）
                // 列表起点 = 搜索框顶 + 搜索框高（GetButtonHeight）+ 小间距，显式计算避免依赖 FilterBar 内部光标
                ImVec2 listPos = ImGui::GetCursorPos();
                listPos.y = (60 + settingsPadding * 0.625f) + g_filterBar->GetButtonHeight() + 13;
                ImGui::SetCursorPos(listPos);

                ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoScrollbar;
                ImGui::BeginChild("ModuleList",
                    ImVec2(
                        -settingsPadding + ImGui::GetStyle().WindowPadding.x,
                        -settingsPadding + ImGui::GetStyle().WindowPadding.y
                    ),
                    false, childFlags
                );
                {
                    ImGui::SetCursorPos(ImVec2(settingsPadding, settingsPadding));

                    for (auto* card : g_moduleCards)
                    {
                        Component* comp = card->GetComponent();
                        // 搜索过滤：按组件名称 / 描述匹配（ImGuiTextFilter，不区分大小写、空格分隔多关键字）
                        if (g_filterBar)
                        {
                            ImGuiTextFilter* f = g_filterBar->GetFilter();
                            if (!f->PassFilter(comp->GetName().c_str()) &&
                                !f->PassFilter(comp->GetDescription()))
                                continue;
                        }
                        if (card->Draw())
                        {
                            g_isInModuleSettings = true;
                            g_selectedComponent = comp;
                        }
                    }
                    ImGui::Dummy(ImVec2(0, 10));
                }
                ImGui::EndChild();
            }
        }
        ImGui::EndChild();

        // 退出按钮（右上角）— 放在独立子窗口中最后绘制
        // 原因：MainPanel 子窗口 (0,0) 填满窗口右缘，若按钮直接挂在 popup 根窗口，
        //       鼠标在其上时 g.HoveredWindow 命中 MainPanel 子窗口（z 序更高），
        //       ImGui 会因 "g.HoveredWindow != window" 判定按钮不可 hover，导致无法点击。
        ImGui::SetCursorPos(ImVec2(settingMenuSize.x - 30.0f - 15.0f, 15.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("##CloseButton", ImVec2(30.0f, 30.0f), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleVar();
        {
            if (GuiBase::CircularButton(ICON_FA_XMARK, ImVec2(30.0f, 30.0f)))
                g_showSettings = false;
        }
        ImGui::EndChild();

        ImGui::EndPopup();
    }
    else
    {
        // BeginPopupModal 返回 false（popup 未打开）时，必须配对 Pop，否则样式栈失衡
        ImGui::PopStyleVar();
        PopSettingsTheme();
    }

    if (!open || (g_navPanel && g_navPanel->GetSelectedIndex() == 3))  // 3 = 退出程序（buttons 数量）
    {
        g_showSettings = false;
        g_animAlpha = 0.0f;  // 重置动画，下次打开时从 0 开始淡入
        SaveConfigNow();     // 设置改动实时落盘（关闭设置窗口时保存一次）
    }
}

// 组件子设置页面
void AppSettings::RenderComponentSettings(Component* comp)
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

    // 从标题行（y=60 分隔线）下方开始，避免与主面板标题"组件管理"重叠，并保证高度充足
    ImGui::SetCursorPos(ImVec2(0.0f, 70.0f));
    ImGui::BeginChild("Settings", ImVec2(-settingsPadding + style.WindowPadding.x, -settingsPadding + style.WindowPadding.y), true, flags);
    {
        // 返回按钮
        if (GuiBase::CircularButton(ICON_FA_ARROW_LEFT, ImVec2(35.0f, 35.0f)))
        {
            g_isInModuleSettings = false;
            g_selectedComponent = nullptr;
            SaveConfigNow();  // 组件设置改动实时落盘
            ImGui::EndChild();
            return;
        }

        // 标题文字
        const float buttonHeight = ImGui::GetCursorPosY();
        const ImVec2 separatorPos = ImGui::GetCursorPos();
        {
            ImGui::SameLine();
            const float titleHeight = ImGui::GetFontSize() * 1.2f;
            ImGui::PushFont(NULL, titleHeight);
            ImGui::SetCursorPosY((separatorPos.y - titleHeight) * 0.5f);
            ImGui::TextUnformatted(comp->GetName().c_str());
            ImGui::PopFont();
        }

        ImGui::SameLine();

        // 重置按钮
        {
            const float right = ImGui::GetWindowWidth();
            const float btnWidth = ImGui::GetStyle().ItemSpacing.x * 2.0f + 35.0f;
            ImGui::SetCursorPos(ImVec2(right - btnWidth, buttonHeight));
            if (GuiBase::CircularButton(ICON_FA_ROTATE_LEFT, ImVec2(35.0f, 35.0f)))
                comp->SetVisible(true);
        }

        ImGui::SetCursorPos(separatorPos);
        ImGui::Separator();

        // 设置内容区域
        ImVec2 childPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(0.0f, childPos.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
        // 可滚动：组件设置内容超高时（如很多设置项）可滚动完整显示
        // 尺寸用 -1 填满剩余区域（由 ImGui 计算），避免链式高度计算偏差导致底部内容被裁剪
        ImGui::BeginChild("InnerSettings", ImVec2(-1, -1), true, ImGuiWindowFlags_NoBackground);
        {
            // 根据组件名称分发专属设置
            std::string compName = comp->GetName();
            // 内容区留白：上下用 style.padding 的倍数（行控件的左右内边距由行控件自身统一处理）
            const float padY = style.WindowPadding.y * 2.0f;
            ImGui::SetCursorPos(ImVec2(style.WindowPadding.x * 3.0f, padY));                          // 顶部留白 padY
            const float renderW = ImGui::GetContentRegionAvail().x - (style.WindowPadding.x * 3.0f);  // 铺满宽度
            // ##settings_render 自身 padding 置 0：行控件的左右 padX（style.padding×2）即为对称留白
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            // AutoResizeY：高度随内容自适应，超高时由 InnerSettings 滚动显示（保证完整显示）
            ImGui::BeginChild("##settings_render", ImVec2(renderW, 0.0f), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground);
            {
                ImGui::SetCursorPos(ImVec2(0.0f, padY));   // 顶部留白 padY（文字上方）
                comp->SettingsRender();
                ImGui::Dummy(ImVec2(0.0f, padY));          // 底部留白 padY（文字下方）
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(); // ImGuiStyleVar_WindowPadding
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(); // ImGuiStyleVar_WindowPadding
    }
    ImGui::EndChild();
}

// 全局"设置"页（软件自身设置）：动画设置
void AppSettings::RenderGlobalSettings()
{
    // 内容起始位置：主面板标题行下方
    ImGui::SetCursorPos(ImVec2(0.0f, 60.0f + settingsPadding * 0.625f + 10.0f));

    // 动画设置（行式控件自带左右内边距，无需分组容器）
    if (GuiBase::SettingsToggleRow("启用组件动画", &g_animEnabled))
        Config::Instance().Set("Settings.Animations.Enabled", g_animEnabled);
    if (GuiBase::SettingsSliderRow("动画速度", &g_animSpeed, 0.2f, 3.0f, "%.1fx"))
        Config::Instance().Set("Settings.Animations.Speed", g_animSpeed);

    // 优先动画公式：Lerp（指数平滑）+ 完整 GuiEase 8 种
    static const char* easeNames[] = {
        "指数平滑 (Lerp)",
        "线性 (Linear)",
        "缓入 (InQuad)",
        "缓出 (OutQuad)",
        "缓入缓出 (InOutQuad)",
        "缓出三次 (OutCubic)",
        "缓入缓出三次 (InOutCubic)",
        "缓出指数 (OutExpo)",
        "回弹 (OutBack)"
    };
    if (GuiBase::SettingsDropdownRow("优先动画公式", easeNames, 9, &g_animEaseIndex))
        Config::Instance().Set("Settings.Animations.Ease", g_animEaseIndex);

    // 提示（与行文字左对齐）
    ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x * 2.0f);
    ImGui::TextDisabled("以上为\"优先\"设置：组件按需使用，可自行覆盖。");
}

// 动画设置读取 API（供组件按需取用："优先"而非强制）
bool AppSettings::IsAnimationEnabled()
{
    return g_animEnabled;
}

float AppSettings::GetAnimationSpeed()
{
    return g_animSpeed;
}

bool AppSettings::IsLerpPreferred()
{
    return g_animEaseIndex <= 0;
}

GuiBase::GuiEase AppSettings::GetPreferredEase()
{
    int idx = g_animEaseIndex - 1;
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;
    return static_cast<GuiBase::GuiEase>(idx);
}

// 从配置加载全局设置（启动时调用）
void AppSettings::LoadGlobalSettings()
{
    auto& cfg = Config::Instance();
    if (cfg.HasItem("Settings.Animations.Enabled"))
        g_animEnabled = cfg.Get<bool>("Settings.Animations.Enabled");
    if (cfg.HasItem("Settings.Animations.Speed"))
        g_animSpeed = cfg.Get<float>("Settings.Animations.Speed");
    if (cfg.HasItem("Settings.Animations.Ease"))
    {
        g_animEaseIndex = cfg.Get<int>("Settings.Animations.Ease");
        if (g_animEaseIndex < 0) g_animEaseIndex = 0;
        if (g_animEaseIndex > 8) g_animEaseIndex = 8;
    }
}

// 实时保存配置：各组件配置写回配置树 + 全局配置写盘（picojson）
void AppSettings::SaveConfigNow()
{
    g_compManager.SaveAllConfigs();
    Config::Instance().WriteConfig();
}

// 设置界面专属主题：集中 Push 设置窗口的 ColorStyle
// - 组件渲染使用全局默认 ImGuiStyle（不 push），两者互不影响
// - 后续给设置界面调色只需改这里
void AppSettings::PushSettingsTheme(float dimAlpha)
{
    // 遮罩（淡入动画透明度）
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, dimAlpha));
    // 设置窗口底色（rgb ≤ 40 深色主题；四角圆角由 PopupRounding 绘制，渐变内缩后显示此色）
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.098f, 0.106f, 0.137f, 1.0f));  // (25,27,35)
    // 子窗口（左栏/主面板/列表）透明背景，透出统一渐变
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
}

// 与 PushSettingsTheme 配对
void AppSettings::PopSettingsTheme()
{
    ImGui::PopStyleColor(3); // ModalWindowDimBg / WindowBg / ChildBg
}

// 鼠标位置命中的窗口调整方向（Windows 风格边缘调整）
// winMin/winMax 为窗口矩形，edge 为边缘热区宽度（向内 + 向外）
// 返回 EditMode::ResizeDir_* 位标志组合；不在任何边缘返回 ResizeDir_None
static int HitTestResizeEdge(const ImVec2& mouse, const ImVec2& winMin, const ImVec2& winMax, float edge)
{
    // 扩展矩形（窗口 + 边缘热区）之外：不在任何边缘
    if (mouse.x < winMin.x - edge || mouse.x > winMax.x + edge ||
        mouse.y < winMin.y - edge || mouse.y > winMax.y + edge)
        return EditMode::ResizeDir_None;

    int dir = EditMode::ResizeDir_None;
    if (mouse.x - winMin.x <= edge) dir |= EditMode::ResizeDir_Left;
    if (winMax.x - mouse.x <= edge) dir |= EditMode::ResizeDir_Right;
    if (mouse.y - winMin.y <= edge) dir |= EditMode::ResizeDir_Up;
    if (winMax.y - mouse.y <= edge) dir |= EditMode::ResizeDir_Down;
    return dir;
}

// 按方向设置调整大小光标（与 Windows 一致：四角斜箭头、四边双向箭头）
static void SetResizeCursor(int dir)
{
    if (dir & EditMode::ResizeDir_Left && dir & EditMode::ResizeDir_Up)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);   // 左上角
    else if (dir & EditMode::ResizeDir_Right && dir & EditMode::ResizeDir_Down)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);   // 右下角
    else if (dir & EditMode::ResizeDir_Right && dir & EditMode::ResizeDir_Up)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);   // 右上角
    else if (dir & EditMode::ResizeDir_Left && dir & EditMode::ResizeDir_Down)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);   // 左下角
    else if (dir & (EditMode::ResizeDir_Left | EditMode::ResizeDir_Right))
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);     // 左右边
    else if (dir & (EditMode::ResizeDir_Up | EditMode::ResizeDir_Down))
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);     // 上下边
}

// 编辑模式前置处理（在 DrawAll 之前调用）
void AppSettings::PreDraw()
{
    if (!EditMode::g_active)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const float threshold = 12.0f;
    const float thresholdTight = 8.0f;
    const float thresholdPadded = 14.0f;
    const float defaultPadding = 12.0f;
    const float resizeEdge = 8.0f; // 边缘热区宽度（向内 + 向外，Windows 风格）
    const float edgeSnap = 8.0f;   // 屏幕边缘吸附距离（移动拖拽时）

    EditMode::g_showAlignLineV = false;
    EditMode::g_showAlignLineH = false;

    ImVec2 displaySize = io.DisplaySize;
    float ButtonW = 120.0f, ButtonH = 40.0f, PadX = 15.0f, PadY = 10.0f, Spacing = 20.0f;
    float btnW = ButtonW * 2 + Spacing + PadX * 2;
    float btnH = ButtonH + PadY * 2;
    float btnLeft = (displaySize.x - btnW) / 2;
    float btnTop = (displaySize.y - btnH) / 2;
    bool mouseOverButtons = 
        (io.MousePos.x >= btnLeft && io.MousePos.x <= btnLeft + btnW && io.MousePos.y >= btnTop && io.MousePos.y <= btnTop + btnH);

    // === 边缘调整大小（Windows 风格：鼠标在窗口边缘即可调整）===
    // 1) 每帧更新鼠标悬停的调整方向（用于光标提示 + 边缘高亮）
    EditMode::g_hoverResizeDir = EditMode::ResizeDir_None;
    EditMode::g_hoverResizeWindow.clear();
    if (EditMode::g_resizingWindow.empty())
    {
        auto& comps = g_compManager.GetComponents();
        for (int i = (int)comps.size() - 1; i >= 0; i--)
        {
            if (!comps[i]->IsVisible() || !comps[i]->AllowResizeInEdit())
                continue;
            ImGuiWindow* win = ImGui::FindWindowByName(comps[i]->GetName().c_str());
            if (!win)
                continue;
            int dir = HitTestResizeEdge(io.MousePos, win->Pos,
                ImVec2(win->Pos.x + win->Size.x, win->Pos.y + win->Size.y), resizeEdge);
            if (dir != EditMode::ResizeDir_None)
            {
                EditMode::g_hoverResizeDir = dir;
                EditMode::g_hoverResizeWindow = comps[i]->GetName();
                break;
            }
        }
    }

    // 2) 调整大小进行中：按鼠标位移计算新位置/新大小（含最小尺寸限制）
    if (!EditMode::g_resizingWindow.empty())
    {
        if (io.MouseDown[0])
        {
            ImGuiWindow* win = ImGui::FindWindowByName(EditMode::g_resizingWindow.c_str());
            if (win)
            {
                const float minW = 80.0f, minH = 60.0f; // 最小尺寸（避免调整得太小）
                ImVec2 delta(io.MousePos.x - EditMode::g_resizeStartMouse.x,
                             io.MousePos.y - EditMode::g_resizeStartMouse.y);
                ImVec2 newPos = EditMode::g_resizeStartPos;
                ImVec2 newSize = EditMode::g_resizeStartSize;
                int dir = EditMode::g_resizeDir;

                // 右/下边缘：只改大小；左/上边缘：同时改位置（保证对侧边缘不动）
                if (dir & EditMode::ResizeDir_Right) newSize.x = EditMode::g_resizeStartSize.x + delta.x;
                if (dir & EditMode::ResizeDir_Down)  newSize.y = EditMode::g_resizeStartSize.y + delta.y;
                if (dir & EditMode::ResizeDir_Left)  { newSize.x = EditMode::g_resizeStartSize.x - delta.x; newPos.x = EditMode::g_resizeStartPos.x + delta.x; }
                if (dir & EditMode::ResizeDir_Up)    { newSize.y = EditMode::g_resizeStartSize.y - delta.y; newPos.y = EditMode::g_resizeStartPos.y + delta.y; }

                // 最小尺寸限制：达到最小值后左/上边缘不再移动（对侧边缘固定）
                if (newSize.x < minW)
                {
                    if (dir & EditMode::ResizeDir_Left)
                        newPos.x = EditMode::g_resizeStartPos.x + EditMode::g_resizeStartSize.x - minW;
                    newSize.x = minW;
                }
                if (newSize.y < minH)
                {
                    if (dir & EditMode::ResizeDir_Up)
                        newPos.y = EditMode::g_resizeStartPos.y + EditMode::g_resizeStartSize.y - minH;
                    newSize.y = minH;
                }

                // 不能在这里直接 SetNextWindowPos/Size：它们只作用于"下一个 Begin 的窗口"
                //（即注册顺序第一个组件），会错乱。必须存入状态，由目标窗口自己的 Draw 内紧贴 Begin 应用
                EditMode::g_resizeNewPos = newPos;
                EditMode::g_resizeNewSize = newSize;
            }
        }
        else
            EditMode::g_resizingWindow.clear(); // 松开鼠标结束调整
    }

    // 3) 光标反馈：悬停边缘或调整中显示 resize 光标，否则恢复默认箭头
    if (!EditMode::g_resizingWindow.empty())
        SetResizeCursor(EditMode::g_resizeDir);
    else if (EditMode::g_hoverResizeDir != EditMode::ResizeDir_None)
        SetResizeCursor(EditMode::g_hoverResizeDir);
    else
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

    // 4) 点击检测：边缘热区 → 开始调整大小；窗口内部 → 开始拖拽移动
    if (ImGui::IsMouseClicked(0) && EditMode::g_draggedWindow.empty() && EditMode::g_resizingWindow.empty() && !mouseOverButtons)
    {
        auto& components = g_compManager.GetComponents();
        for (int i = (int)components.size() - 1; i >= 0; i--)
        {
            if (!components[i]->IsVisible())
                continue;
            ImGuiWindow* win = ImGui::FindWindowByName(components[i]->GetName().c_str());
            if (!win)
                continue;
            ImVec2 min = win->Pos;
            ImVec2 max = ImVec2(win->Pos.x + win->Size.x, win->Pos.y + win->Size.y);

            // 不在扩展矩形（窗口 + 边缘热区）内则跳过
            if (io.MousePos.x < min.x - resizeEdge || io.MousePos.x > max.x + resizeEdge ||
                io.MousePos.y < min.y - resizeEdge || io.MousePos.y > max.y + resizeEdge)
                continue;

            // 边缘热区优先：进入调整大小（与 Windows 一致，四边四角均可）
            if (components[i]->AllowResizeInEdit())
            {
                int dir = HitTestResizeEdge(io.MousePos, min, max, resizeEdge);
                if (dir != EditMode::ResizeDir_None)
                {
                    EditMode::g_resizingWindow = components[i]->GetName();
                    EditMode::g_resizeDir = dir;
                    EditMode::g_resizeStartMouse = io.MousePos;
                    EditMode::g_resizeStartPos = min;
                    EditMode::g_resizeStartSize = win->Size;
                    // 初始化当帧应用值 = 当前状态，避免点击瞬间窗口跳到残留位置/大小
                    EditMode::g_resizeNewPos = min;
                    EditMode::g_resizeNewSize = win->Size;
                    break;
                }
            }
            // 窗口内部：拖拽移动
            if (io.MousePos.x >= min.x && io.MousePos.x <= max.x && io.MousePos.y >= min.y && io.MousePos.y <= max.y)
            {
                EditMode::g_draggedWindow = components[i]->GetName();
                EditMode::g_dragOffset = ImVec2(io.MousePos.x - min.x, io.MousePos.y - min.y);
                break;
            }
        }
    }

    if (!EditMode::g_draggedWindow.empty())
    {
        if (io.MouseDown[0])
        {
            ImVec2 newPos(io.MousePos.x - EditMode::g_dragOffset.x, io.MousePos.y - EditMode::g_dragOffset.y);
            ImGuiWindow* dragWin = ImGui::FindWindowByName(EditMode::g_draggedWindow.c_str());
            if (dragWin)
            {
                ImVec2 winSize = dragWin->Size;
                ImVec2 screenCenter(displaySize.x / 2, displaySize.y / 2);

                // 屏幕边缘吸附：组件窗口边缘靠近屏幕边缘时吸附（与 Windows 拖动窗口一致）
                // 同轴用 else if：左/右、上/下只会吸附其中一边，避免互相抢线
                if (fabs(newPos.x) < edgeSnap)
                {
                    newPos.x = 0.0f;
                    EditMode::g_showAlignLineV = true;
                    EditMode::g_alignLineX = 0.0f;
                }
                else if (fabs(newPos.x + winSize.x - displaySize.x) < edgeSnap)
                {
                    newPos.x = displaySize.x - winSize.x;
                    EditMode::g_showAlignLineV = true;
                    EditMode::g_alignLineX = displaySize.x;
                }
                if (fabs(newPos.y) < edgeSnap)
                {
                    newPos.y = 0.0f;
                    EditMode::g_showAlignLineH = true;
                    EditMode::g_alignLineY = 0.0f;
                }
                else if (fabs(newPos.y + winSize.y - displaySize.y) < edgeSnap)
                {
                    newPos.y = displaySize.y - winSize.y;
                    EditMode::g_showAlignLineH = true;
                    EditMode::g_alignLineY = displaySize.y;
                }

                if (fabs(newPos.x + winSize.x / 2 - screenCenter.x) < threshold)
                {
                    newPos.x = screenCenter.x - winSize.x / 2;
                    EditMode::g_showAlignLineV = true;
                    EditMode::g_alignLineX = screenCenter.x;
                }
                if (fabs(newPos.y + winSize.y / 2 - screenCenter.y) < threshold)
                {
                    newPos.y = screenCenter.y - winSize.y / 2;
                    EditMode::g_showAlignLineH = true;
                    EditMode::g_alignLineY = screenCenter.y;
                }

                for (auto& other : g_compManager.GetComponents())
                {
                    if (!other->IsVisible() || other->GetName() == EditMode::g_draggedWindow)
                        continue;
                    ImGuiWindow* otherWin = ImGui::FindWindowByName(other->GetName().c_str());
                    if (!otherWin)
                        continue;

                    ImVec2 oPos = otherWin->Pos;
                    ImVec2 oSize = otherWin->Size;
                    bool hAligned = false;

                    if (fabs(newPos.x - oPos.x) < threshold)
                    {
                        newPos.x = oPos.x;
                        EditMode::g_showAlignLineV = true;
                        EditMode::g_alignLineX = oPos.x;
                        hAligned = true;
                    }
                    else if (fabs(newPos.x + winSize.x - oPos.x - oSize.x) < threshold)
                    {
                        newPos.x = oPos.x + oSize.x - winSize.x;
                        EditMode::g_showAlignLineV = true;
                        EditMode::g_alignLineX = oPos.x + oSize.x;
                        hAligned = true;
                    }
                    else if (fabs(newPos.x + winSize.x / 2 - oPos.x - oSize.x / 2) < threshold)
                    {
                        newPos.x = oPos.x + oSize.x / 2 - winSize.x / 2;
                        EditMode::g_showAlignLineV = true;
                        EditMode::g_alignLineX = oPos.x + oSize.x / 2;
                        hAligned = true;
                    }

                    if (!hAligned)
                    {
                        float gapRight = newPos.x - (oPos.x + oSize.x);
                        if (gapRight > 0 && gapRight < thresholdPadded)
                        {
                            if (gapRight < thresholdTight)
                                newPos.x = oPos.x + oSize.x;
                            else
                                newPos.x = oPos.x + oSize.x + defaultPadding;
                            EditMode::g_showAlignLineV = true;
                            EditMode::g_alignLineX = newPos.x;
                        }
                        float gapLeft = oPos.x - (newPos.x + winSize.x);
                        if (gapLeft > 0 && gapLeft < thresholdPadded)
                        {
                            if (gapLeft < thresholdTight)
                                newPos.x = oPos.x - winSize.x;
                            else
                                newPos.x = oPos.x - winSize.x - defaultPadding;
                            EditMode::g_showAlignLineV = true;
                            EditMode::g_alignLineX = newPos.x + winSize.x;
                        }
                    }

                    bool vAligned = false;
                    if (fabs(newPos.y - oPos.y) < threshold)
                    {
                        newPos.y = oPos.y;
                        EditMode::g_showAlignLineH = true;
                        EditMode::g_alignLineY = oPos.y;
                        vAligned = true;
                    }
                    else if (fabs(newPos.y + winSize.y - oPos.y - oSize.y) < threshold)
                    {
                        newPos.y = oPos.y + oSize.y - winSize.y;
                        EditMode::g_showAlignLineH = true;
                        EditMode::g_alignLineY = oPos.y + oSize.y;
                        vAligned = true;
                    }
                    else if (fabs(newPos.y + winSize.y / 2 - oPos.y - oSize.y / 2) < threshold)
                    {
                        newPos.y = oPos.y + oSize.y / 2 - winSize.y / 2;
                        EditMode::g_showAlignLineH = true;
                        EditMode::g_alignLineY = oPos.y + oSize.y / 2;
                        vAligned = true;
                    }

                    if (!vAligned)
                    {
                        float gapBottom = newPos.y - (oPos.y + oSize.y);
                        if (gapBottom > 0 && gapBottom < thresholdPadded)
                        {
                            if (gapBottom < thresholdTight)
                                newPos.y = oPos.y + oSize.y;
                            else
                                newPos.y = oPos.y + oSize.y + defaultPadding;
                            EditMode::g_showAlignLineH = true;
                            EditMode::g_alignLineY = newPos.y;
                        }
                        float gapTop = oPos.y - (newPos.y + winSize.y);
                        if (gapTop > 0 && gapTop < thresholdPadded)
                        {
                            if (gapTop < thresholdTight)
                                newPos.y = oPos.y - winSize.y;
                            else
                                newPos.y = oPos.y - winSize.y - defaultPadding;
                            EditMode::g_showAlignLineH = true;
                            EditMode::g_alignLineY = newPos.y + winSize.y;
                        }
                    }
                }
            }
            EditMode::g_dragNewPos = newPos;
        }
        else
            EditMode::g_draggedWindow.clear();
    }
}

// 重置布局
void AppSettings::ResetToDefaultLayout()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float originX = displaySize.x / 2;
    const float margin = 16.0f;
    const float gap = 12.0f;
    const float taskbarHeight = 48.0f;

    float startX = displaySize.x - margin;
    float startY = displaySize.y - taskbarHeight - margin;
    float currentX = startX;
    float currentY = startY;
    float rowMaxHeight = 0.0f;

    for (auto& comp : g_compManager.GetComponents())
    {
        if (!comp->IsVisible())
            continue;
        ImVec2 size = comp->GetWindowSize();
        if (size.x <= 0 || size.y <= 0)
            size = ImVec2(200, 200);

        if (currentX - size.x < originX)
        {
            currentY -= rowMaxHeight + gap;
            currentX = startX;
            rowMaxHeight = 0.0f;
        }
        comp->SetWindowPos(ImVec2(currentX - size.x, currentY - size.y));
        currentX -= size.x + gap;
        if (size.y > rowMaxHeight)
            rowMaxHeight = size.y;
    }
}

// 编辑模式后置处理
void AppSettings::PostDraw()
{
    RenderSettingsWindow();

    if (!EditMode::g_active)
        return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImDrawList* bgDrawList = ImGui::GetBackgroundDrawList();

    if (EditMode::g_showAlignLineV)
        bgDrawList->AddLine(ImVec2(EditMode::g_alignLineX, 0), ImVec2(EditMode::g_alignLineX, displaySize.y), IM_COL32(100, 200, 255, 150), 2.0f);
    if (EditMode::g_showAlignLineH)
        bgDrawList->AddLine(ImVec2(0, EditMode::g_alignLineY), ImVec2(displaySize.x, EditMode::g_alignLineY), IM_COL32(100, 200, 255, 150), 2.0f);

    for (auto& comp : g_compManager.GetComponents())
    {
        if (!comp->IsVisible())
            continue;
        ImGuiWindow* win = ImGui::FindWindowByName(comp->GetName().c_str());
        if (!win)
            continue;
        bgDrawList->AddRect(win->Pos, ImVec2(win->Pos.x + win->Size.x, win->Pos.y + win->Size.y), IM_COL32(100, 200, 255, 80), 0, 0, 1.5f);

        // 边缘调整高亮：hover 或调整中时高亮对应边缘（Windows 风格反馈）
        int hlDir = EditMode::ResizeDir_None;
        if (EditMode::g_resizingWindow == comp->GetName())
            hlDir = EditMode::g_resizeDir;
        else if (EditMode::g_resizingWindow.empty() && EditMode::g_hoverResizeWindow == comp->GetName())
            hlDir = EditMode::g_hoverResizeDir;

        if (hlDir != EditMode::ResizeDir_None)
        {
            const bool active = (EditMode::g_resizingWindow == comp->GetName());
            ImU32 hlCol = active ? IM_COL32(100, 200, 255, 255) : IM_COL32(100, 200, 255, 170);
            const float thick = active ? 2.5f : 2.0f;
            ImVec2 p0 = win->Pos;
            ImVec2 p1 = ImVec2(win->Pos.x + win->Size.x, win->Pos.y + win->Size.y);
            if (hlDir & EditMode::ResizeDir_Left)   bgDrawList->AddLine(p0, ImVec2(p0.x, p1.y), hlCol, thick);
            if (hlDir & EditMode::ResizeDir_Right)  bgDrawList->AddLine(ImVec2(p1.x, p0.y), p1, hlCol, thick);
            if (hlDir & EditMode::ResizeDir_Up)     bgDrawList->AddLine(p0, ImVec2(p1.x, p0.y), hlCol, thick);
            if (hlDir & EditMode::ResizeDir_Down)   bgDrawList->AddLine(ImVec2(p0.x, p1.y), p1, hlCol, thick);
        }
    }

    // 编辑模式按钮 — 使用 ForegroundDrawList 确保始终置顶
    float ButtonW = 120.0f, ButtonH = 40.0f, PadX = 15.0f, PadY = 10.0f, Spacing = 20.0f;
    const ImVec2 WindowSize(ButtonW * 2 + Spacing + PadX * 2, ButtonH + PadY * 2);
    const ImVec2 WindowPos((displaySize.x - WindowSize.x) / 2, (displaySize.y - WindowSize.y) / 2);

    // 使用 ForegroundDrawList 绘制按钮背景，确保不被组件窗口遮挡
    ImDrawList* fgDraw = ImGui::GetForegroundDrawList();
    ImU32 bgCol = ImGui::GetColorU32(ImGuiCol_WindowBg, 0.9f);
    fgDraw->AddRectFilled(WindowPos, ImVec2(WindowPos.x + WindowSize.x, WindowPos.y + WindowSize.y), bgCol, 8.0f);

    // 返回按钮区域
    ImVec2 btn1Min(WindowPos.x + PadX, WindowPos.y + PadY);
    ImVec2 btn1Max(btn1Min.x + ButtonW, btn1Min.y + ButtonH);
    ImVec2 btn2Min(btn1Max.x + Spacing, btn1Min.y);
    ImVec2 btn2Max(btn2Min.x + ButtonW, btn2Min.y + ButtonH);

    ImGuiIO& io = ImGui::GetIO();
    bool hoverBtn1 = io.MousePos.x >= btn1Min.x && io.MousePos.x <= btn1Max.x && io.MousePos.y >= btn1Min.y && io.MousePos.y <= btn1Max.y;
    bool hoverBtn2 = io.MousePos.x >= btn2Min.x && io.MousePos.x <= btn2Max.x && io.MousePos.y >= btn2Min.y && io.MousePos.y <= btn2Max.y;

    ImU32 btnNormalCol = ImGui::GetColorU32(ImGuiCol_Button);
    ImU32 btnHoverCol = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);

    fgDraw->AddRectFilled(btn1Min, btn1Max, hoverBtn1 ? btnHoverCol : btnNormalCol, 6.0f);
    fgDraw->AddRectFilled(btn2Min, btn2Max, hoverBtn2 ? btnHoverCol : btnNormalCol, 6.0f);

    const char* text1 = "返回";
    const char* text2 = "重置";
    ImVec2 ts1 = ImGui::CalcTextSize(text1);
    ImVec2 ts2 = ImGui::CalcTextSize(text2);
    fgDraw->AddText(ImVec2(btn1Min.x + (ButtonW - ts1.x) * 0.5f, btn1Min.y + (ButtonH - ts1.y) * 0.5f), textCol, text1);
    fgDraw->AddText(ImVec2(btn2Min.x + (ButtonW - ts2.x) * 0.5f, btn2Min.y + (ButtonH - ts2.y) * 0.5f), textCol, text2);

    // 处理点击
    if (ImGui::IsMouseClicked(0))
    {
        if (hoverBtn1)
        {
            EditMode::g_active = false;
            EditMode::g_draggedWindow.clear();
            EditMode::g_resizingWindow.clear();
            EditMode::g_resizeDir = EditMode::ResizeDir_None;
            EditMode::g_hoverResizeDir = EditMode::ResizeDir_None;
            EditMode::g_hoverResizeWindow.clear();
            {
                g_navPanel->SetSelectedIndex(g_lastSelectedIndex);
                // 同步恢复进入位置调整前的页面
                switch (g_lastSelectedIndex)
                {
                case 0: g_currentPage = SettingsPage::ComponentList; break;
                case 2: g_currentPage = SettingsPage::GlobalSettings; break;
                }
            }
            g_showSettings = true;
            SaveConfigNow();  // 位置调整结束返回时保存一次
        }
        else if (hoverBtn2)
            ResetToDefaultLayout();
    }
}
