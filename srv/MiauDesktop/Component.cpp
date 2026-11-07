#include "Component.h"
#include "GuiBase/GuiBase.h"

// 全局组件管理器，仅此一个
ComponentManager g_compManager;

// 注册当前窗口（在 Begin 之后调用）
void ComponentManager::RegisterCurrentWindow()
{
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow)
    {
        ImGuiID id = g.CurrentWindow->RootWindow->ID;
        g_componentWindowIds.insert(id);
    }
}

// 判断窗口是否属于组件列表
bool ComponentManager::IsComponentWindow(ImGuiWindow* window)
{
    if (!window) return false;
    ImGuiID id = window->RootWindow->ID;
    return g_componentWindowIds.find(id) != g_componentWindowIds.end();
}

void ComponentManager::HandleWindowsPassthrough()
{
    // 如果鼠标正在交互组件上，绝对不能放行
    if (ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive())
        return;

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.HoveredWindow;

    // 如果鼠标没有悬停在任何窗口上，或者悬停的不是我们的组件，则直接返回（不做任何事）
    if (!window || !IsComponentWindow(window))
        return;

    // 如果鼠标在窗口的标题栏区域（包括关闭按钮等），保留捕获
    // TitleBarRect() 是 ImGuiWindow 的内部方法，返回标题栏矩形
    if (window->TitleBarRect().Contains(g.IO.MousePos))
        return;

    // 如果用户正在拖拽窗口（ActiveId == MoveId），也保留捕获
    if (g.ActiveId == window->MoveId)
        return;

    // 其他情况（在窗口内容区域的空白处）→ 释放鼠标，实现穿透
    ImGui::SetNextFrameWantCaptureMouse(false);
}

// 每帧在主循环中调用，清空列表（在 NewFrame 之后、绘制窗口之前）
void ComponentManager::ResetComponentWindowList()
{
    g_componentWindowIds.clear();
}

const char* Component::GetDescription() const
{
    return "桌面组件";
}

const char* Component::GetIcon() const
{
    return ICON_FA_CUBE;
}

const std::string& Component::GetName() const
{
    return m_name;
}

ImVec2 Component::GetWindowSize() const
{
    return m_windowSize;
}

bool Component::AllowResizeInEdit() const
{
    return m_allowResizeInEdit;
}







void Component::Draw()
{
    if (!m_visible)
        return;

    // 编辑模式：禁用所有交互（冻结组件，只能拖动移动）
    if (EditMode::g_active)
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

    AfterBegin();

    // 编辑模式：保留 NoMove 标志以禁止 ImGui 内部的标题栏拖拽，
    // 改用 PreDraw 的自定义拖拽逻辑（SetNextWindowPos）来移动窗口
    // 原因：如果清除 NoMove，带标题栏的窗口（如进程管理器）会同时被
    //       ImGui 内部拖拽和 PreDraw 逻辑移动，导致 SetNextWindowPos 被覆盖，
    //       吸附位置失效
    ImGuiWindowFlags flags = windowFlags;
    if (EditMode::g_active)
    {
        // 编辑模式统一屏蔽 ImGui 原生 resize（边缘调整大小由 PreDraw 的自定义逻辑处理）
        flags |= ImGuiWindowFlags_NoResize;
        // SetNextWindowPos/Size 必须紧贴自己的 Begin 调用，否则会应用到下一个 Begin 的窗口
        //（移动 / 调整大小的应用都放这里，按窗口名匹配，互不干扰）
        if (EditMode::g_draggedWindow == m_name)
            ImGui::SetNextWindowPos(EditMode::g_dragNewPos);
        else if (EditMode::g_resizingWindow == m_name)
        {
            ImGui::SetNextWindowPos(EditMode::g_resizeNewPos);
            ImGui::SetNextWindowSize(EditMode::g_resizeNewSize);
        }
    }

    // 如果有期望位置（重置布局），应用到下一帧
    if (m_hasDesiredPos)
    {
        ImGui::SetNextWindowPos(m_desiredPos);
        m_hasDesiredPos = false;
    }

    if (m_useGuiRendering)
    {
        flags |= ImGuiWindowFlags_NoBackground; // 移除默认背景，使用 GUI Card 自绘背景
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    }
    else
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);

    // 编辑模式：不使用ImGui的移动处理，而是自行处理
    // 原因：如果窗口没有NoMove标识，对于大小调整的功能，
    //       会出现始终是移动窗口，而不能调整窗口大小
    if (EditMode::g_active)
    {
        flags |= ImGuiWindowFlags_NoMove;
        flags &= ~ImGuiWindowFlags_AlwaysAutoResize;
    }

    // 固定 Begin 参数：不提供 p_open 指针，防止用户通过关闭按钮隐藏
    ImGui::Begin(m_name.c_str(), nullptr, flags);
    {
        ImGui::PopStyleVar(); // WindowRounding
        if (m_useGuiRendering)
            ImGui::PopStyleVar(); // WindowBorderSize

        // 设置窗口大小
        if (m_isWindowSizeUpdate)
        {
            ImGui::SetWindowSize(m_windowSize);
            m_isWindowSizeUpdate = false;
        }
        // 注册当前窗口 ID，用于鼠标穿透
        g_compManager.RegisterCurrentWindow();

        // 绘制 GUI Card 背景
        if (m_useGuiRendering)
        {
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            if (win)
            {
                ImDrawList* drawList = win->DrawList;
                GuiBase::GuiCardStyle cardStyle;
                // 使用半透明背景（alpha=0.88），与窗口透明度一致
                cardStyle.backgroundColor.a = 0.88f;
                // 边框使用 EUI 主题色，alpha 略低
                cardStyle.borderAlpha = 0.5f;
                // 窗口内部不绘制阴影（阴影在 End 后的 BackgroundDrawList 上绘制）
                cardStyle.shadow.blur = 0.0f;
                GuiBase::DrawGuiCardBackground(drawList, win->Pos, win->Size, cardStyle);
            }
        }

        // 调用派生类实现的具体绘制内容
        ComponentRender();
    }
    ImGui::End();

    // 每帧同步真实窗口位置/尺寸到持久化成员（供退出时配置保存，始终为最新值）
    // 位置：编辑模式拖拽 / 重置布局后保持最新；尺寸：边缘调整大小后保持最新
    ImGuiWindow* posWin = ImGui::FindWindowByName(m_name.c_str());
    if (posWin)
    {
        m_windowPosX = posWin->Pos.x;
        m_windowPosY = posWin->Pos.y;
        m_windowSize = posWin->Size;
    }

    // 绘制窗口软阴影（风格是高斯软阴影，panel 级：blur=24/offsetY=8/alpha=0.28）
    if (m_useGuiRendering)
    {
        ImGuiWindow* win = ImGui::FindWindowByName(m_name.c_str());
        if (win)
        {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            GuiBase::GuiShadowStyle panelShadow;
            panelShadow.blur = 24.0f;
            panelShadow.offsetX = 0.0f;
            panelShadow.offsetY = 8.0f;
            panelShadow.color = GuiBase::GuiColor(0.0f, 0.0f, 0.0f, 0.28f);
            GuiBase::DrawGuiSoftShadow(bg, win->Pos, win->Size, panelShadow, 18.0f);
        }
    }
    else
    {
        ImGuiWindow* win = ImGui::FindWindowByName(m_name.c_str());
        if (win)
        {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            const float rounding = 12.0f;
            const ImVec2 offset(4, 6);
            const float baseAlpha = 0.22f;
            // 8 层叠加（原 4 层），层间步长更小，过渡更平滑
            for (int i = 8; i >= 1; i--)
            {
                float alpha = baseAlpha * (1.0f - i * 0.10f);
                float expand = i * 1.2f;
                bg->AddRectFilled(
                    ImVec2(win->Pos.x + offset.x - expand, win->Pos.y + offset.y - expand),
                    ImVec2(win->Pos.x + win->Size.x + offset.x + expand, win->Pos.y + win->Size.y + offset.y + expand),
                    IM_COL32(0, 0, 0, (int)(alpha * 255)), rounding + expand
                );
            }
        }
    }

    BeforeEnd();

    if (EditMode::g_active)
        ImGui::PopItemFlag();
}

void Component::SetVisible(bool visible)
{
    m_visible = visible;
    // 基础配置由继承者重写的 SaveConfig 保存（可见性属于基础配置，基类 SaveConfig 已处理）
}
bool Component::IsVisible() const
{
    return m_visible;
}
void Component::SetWindowSize(const ImVec2& size)
{
    m_windowSize = size;
    m_isWindowSizeUpdate = true; // 触发SetWindowSize
}
void Component::SetFixedWindow(bool fixed)
{
    windowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}

void Component::SetWindowPos(const ImVec2& pos)
{
    m_hasDesiredPos = true;
    m_desiredPos = pos;
    // 真实位置由 Draw 每帧同步到 m_windowPosX/Y，退出保存时即为最新位置
}

void Component::ResetConfig()
{
    // 重置配置树为默认值，再调用 LoadConfig 同步到组件变量
    //（LoadConfig 内部会调用虚函数 OnLoadConfig，连同继承者的特有配置一起重置）
    ::Config::Instance().ResetConfig(m_name);
    LoadConfig();
}

void Component::LoadConfig()
{
    // 基础配置：可见性
    m_visible = config.Get<bool>("Visible");
    // 窗口尺寸：0 表示未配置（保持组件自然大小）
    float w = config.Get<float>("WindowSize.Width");
    float h = config.Get<float>("WindowSize.Height");
    if (w > 0.0f && h > 0.0f)
        m_windowSize = ImVec2(w, h);
    // 窗口位置：-1 表示未配置（保持默认位置）
    float x = config.Get<float>("WindowPos.X");
    float y = config.Get<float>("WindowPos.Y");
    if (x >= 0.0f && y >= 0.0f)
    {
        m_hasDesiredPos = true;
        m_desiredPos = ImVec2(x, y);
    }
    // 继承者特有配置
    OnLoadConfig();
}
void Component::SaveConfig()
{
    config.Set("Visible", m_visible);
    // 保存真实窗口尺寸/位置（m_windowSize / m_windowPosX/Y 每帧由 Draw 同步为最新值）
    config.Set("WindowSize.Width", m_windowSize.x);
    config.Set("WindowSize.Height", m_windowSize.y);
    config.Set("WindowPos.X", m_windowPosX);
    config.Set("WindowPos.Y", m_windowPosY);
    // 继承者特有配置
    OnSaveConfig();
}





void ComponentManager::AddComponent(std::unique_ptr<Component> comp)
{
    m_components.push_back(std::move(comp));
}

void ComponentManager::DrawAll()
{
    for (auto& comp : m_components)
        comp->Draw();
}

Component* ComponentManager::FindByName(const std::string& name)
{
    for (auto& comp : m_components)
    {
        if (comp->GetName() == name)
            return comp.get();
    }
    return nullptr;
}

// 收集本组件的设置项（默认无选项）
void Component::CollectSettings(std::vector<Miau::Settings::SettingItem>& items)
{
    (void)items;
}

Miau::Settings::SettingItem Component::MakeToggle(std::string key, std::string label, std::string group,
                                                 bool value, std::string description)
{
    Miau::Settings::SettingItem item;
    item.key = std::move(key);
    item.label = std::move(label);
    item.group = std::move(group);
    item.description = std::move(description);
    item.kind = Miau::Settings::ItemKind::Toggle;
    item.valueKind = Miau::Settings::ValueKind::Bool;
    item.boolValue = value;
    return item;
}

Miau::Settings::SettingItem Component::MakeSlider(std::string key, std::string label, std::string group,
                                                 double value, double minValue, double maxValue,
                                                 double step, int decimals, std::string suffix,
                                                 std::string description)
{
    Miau::Settings::SettingItem item;
    item.key = std::move(key);
    item.label = std::move(label);
    item.group = std::move(group);
    item.description = std::move(description);
    item.kind = Miau::Settings::ItemKind::Slider;
    item.valueKind = Miau::Settings::ValueKind::Number;
    item.numberValue = value;
    item.minValue = minValue;
    item.maxValue = maxValue;
    item.step = step;
    item.decimals = decimals;
    item.suffix = std::move(suffix);
    return item;
}

Miau::Settings::SettingItem Component::MakeChoice(std::string key, std::string label, std::string group,
                                                 std::vector<std::string> options, int selectedIndex,
                                                 std::string description)
{
    Miau::Settings::SettingItem item;
    item.key = std::move(key);
    item.label = std::move(label);
    item.group = std::move(group);
    item.description = std::move(description);
    item.kind = Miau::Settings::ItemKind::Choice;
    item.valueKind = Miau::Settings::ValueKind::Number;
    item.numberValue = static_cast<double>(selectedIndex);
    item.options = std::move(options);
    item.selectedIndex = selectedIndex;
    return item;
}

// 应用设置项：写入配置树 → 同步成员变量 → 触发生效逻辑
void Component::ApplySetting(Miau::Settings::SettingItem const& item)
{
    switch (item.valueKind)
    {
    case Miau::Settings::ValueKind::Bool:
        config.Set(item.key, item.boolValue);
        break;
    case Miau::Settings::ValueKind::Number:
        config.Set(item.key, item.numberValue);
        break;
    case Miau::Settings::ValueKind::String:
        config.Set(item.key, item.stringValue);
        break;
    }
    OnLoadConfig();
    OnSettingsApplied();
}