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
        OpenSettingsApp();
        break;
    case 2:
        PostQuitMessage(0);
        break;
    }
    DestroyMenu(hMenu);
}

// 启动（或激活）独立的 WinUI3 设置程序
// 设置程序与本程序同目录（srv\bin\<Platform>\<Config>）；若已运行则通过命名事件唤出其窗口
void AppSettings::OpenSettingsApp()
{
    HANDLE showEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\MiauSettings.Show");
    if (showEvent != nullptr)
    {
        SetEvent(showEvent);
        CloseHandle(showEvent);
        return;
    }

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring settingsPath(exePath);
    const size_t slash = settingsPath.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return;
    settingsPath.resize(slash + 1);
    settingsPath += L"MiauSettings.exe";

    std::wstring commandLine = L"\"" + settingsPath + L"\"";
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(settingsPath.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                       0, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
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
            SaveConfigNow();  // 位置调整结束返回时保存一次
        }
        else if (hoverBtn2)
            ResetToDefaultLayout();
    }
}
