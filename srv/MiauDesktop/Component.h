#pragma once
#include "Framework.h"
#include "GuiBase/GuiBase.h"
#include "../Shared/SettingsProtocol.h"
#include <unordered_set>

// 主窗口句柄（在 MainGUI.h 中定义，供 ComponentManager 坐标转换使用）
extern HWND g_hWnd;

class Component
{
public:
    // 构造函数：传入窗口名称和可选标志（派生类可自定义，但会强制加上一些固定标志）
    // lockSizeInEdit：传 true 表示禁止在编辑模式下调整窗口大小（默认允许）
    Component(const std::string& name, ImGuiWindowFlags flags = 0, bool lockSizeInEdit = false)
        : m_name(name)
		, windowFlags(flags | ImGuiWindowFlags_NoCollapse)  // 默认不能折叠
		, m_visible(true), m_windowSize(200, 200)           // 默认窗口大小
		, m_allowResizeInEdit(!lockSizeInEdit)              // 编辑模式默认允许调整大小
    {
        // 配置视图自动绑定到本组件根节点（节点名 = m_name），
        // 配置文件路径由全局 Config::SetConfigFile 统一设置，继承者无需关心
        config.Bind(name);
        // 注册基础配置项（可见性 / 窗口大小 / 窗口位置）
        config.AddItem("Visible", true);
        // 尺寸默认 0 表示"未配置"：首次运行保持组件自然大小（如构造时 SetWindowSize 的值），保存时写入真实大小
        config.AddItem("WindowSize.Width", 0.0f);
        config.AddItem("WindowSize.Height", 0.0f);
        // 位置默认 -1 表示"未配置"：首次运行保持默认位置，保存时写入真实位置
        config.AddItem("WindowPos.X", -1.0f);
        config.AddItem("WindowPos.Y", -1.0f);
    }

    virtual ~Component() = default;

    // 禁止拷贝和赋值
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // ===================== 配置读写（由 ComponentManager 在全局加载/保存时统一分发调用）=====================
    // 基础配置（可见性 / 窗口大小 / 窗口位置）在这里强制处理，每个组件都有，不受继承者影响；
    // 继承者只需重写 OnLoadConfig / OnSaveConfig 读写自己的特有配置项，
    // 用 config.Get / config.Set 即可，路径自动挂在 m_name 根节点下，无需拼路径或关心配置文件。
    void LoadConfig();
    void SaveConfig();

    // 绘制入口：基类控制 Begin/End，确保参数一致
    void Draw();
    // 收集本组件的专属设置项（数据驱动，由 WinUI3 设置程序渲染）
    // 派生类重写该函数即可新增设置项，无需改动设置程序
    virtual void CollectSettings(std::vector<Miau::Settings::SettingItem>& items);
    // 应用一个设置项：写入配置树 + 同步成员变量 + 调用 OnSettingsApplied
    void ApplySetting(Miau::Settings::SettingItem const& item);
    // 组件图标（FontAwesome 图标字符串，用于设置界面卡片显示）
    virtual const char* GetIcon() const;
    // 组件描述（用于设置界面卡片显示）
    virtual const char* GetDescription() const;

	// 设置组件的渲染方面的属性
    void SetVisible(bool visible); // 相当于控制组件是否启用，至于为什么叫 是否可见，我觉得这样好理解一点
    bool IsVisible() const;
    void SetWindowSize(const ImVec2& size);
    void SetFixedWindow(bool fixed);        // 是否设置为固定的窗口，如果是，父窗口Begin就不显示滚动条也不允许滚动
    void SetWindowPos(const ImVec2& pos);   // 设置窗口期望位置（下一帧生效）
    ImVec2 GetWindowSize() const;           // 获取已存储的窗口尺寸

    // 组件配置项控制
	void ResetConfig();   // 重置组件配置为默认值（并同步到组件变量）

    // 获取窗口名称（用于外部查找）
    const std::string& GetName() const;

    // 编辑模式下是否允许调整窗口大小（构造函数 lockSizeInEdit = true 时为 false）
    bool AllowResizeInEdit() const;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiWindowFlags windowFlags;

    // 组件配置视图：绑定到 m_name 根节点，继承者用它注册 / 读写配置项
    ::Config::ComponentConfig config;

protected:
    // 派生类必须实现绘制内容（在 Begin/End 之间调用）
    virtual void ComponentRender() = 0;
	// 一些额外的 Begin/End前后 的处理逻辑，派生类可选择实现
	virtual void BeforeEnd() {}
	virtual void AfterBegin() {}

    // 继承者重写：读写自己的特有配置项
    // 基础配置已由基类 LoadConfig / SaveConfig 自动处理，这里不需要也不应调用基类版本
    virtual void OnLoadConfig() {}
    virtual void OnSaveConfig() {}
    // 设置项生效后的附加处理（默认空实现，如需要根据设置调整窗口尺寸时重写）
    virtual void OnSettingsApplied() {}

    // ===== 设置项构造辅助（供派生类在 CollectSettings 中使用）=====
    static Miau::Settings::SettingItem MakeToggle(std::string key, std::string label, std::string group,
                                                  bool value, std::string description = {});
    static Miau::Settings::SettingItem MakeSlider(std::string key, std::string label, std::string group,
                                                  double value, double minValue, double maxValue,
                                                  double step, int decimals, std::string suffix = {},
                                                  std::string description = {});
    static Miau::Settings::SettingItem MakeChoice(std::string key, std::string label, std::string group,
                                                  std::vector<std::string> options, int selectedIndex,
                                                  std::string description = {});

    // 是否使用 GUI 风格渲染（默认 true，DebugDemo 设为 false 使用原始 ImGui 风格）
    // 这个是脑袋一抽写出来的，自己写的也不太清楚当时怎么想的，算了，就保留吧
    bool m_useGuiRendering = true; 
private:
    std::string m_name;
    bool m_visible;
    ImVec2 m_windowSize;
    bool m_isWindowSizeUpdate = true;     // 窗口大小是否更新（默认是true，用于第一次初始化）
    bool m_hasDesiredPos = false;         // 是否有期望位置（用于重置布局）
    ImVec2 m_desiredPos = ImVec2(0, 0);   // 期望的窗口位置
    float m_windowPosX = -1.0f;           // 窗口位置 X（配置持久化用，-1 表示未配置）
    float m_windowPosY = -1.0f;           // 窗口位置 Y（配置持久化用，-1 表示未配置）
    bool m_allowResizeInEdit = true;      // 编辑模式下是否允许调整窗口大小（构造函数 lockSizeInEdit 控制）
};





class ComponentManager
{
public:
    void AddComponent(std::unique_ptr<Component> comp);

    void DrawAll();

    Component* FindByName(const std::string& name);

	void RegisterCurrentWindow();                  // 注册当前窗口为组件窗口
	bool IsComponentWindow(ImGuiWindow* window);   // 检查窗口是否为组件窗口
	void ResetComponentWindowList();               // 重置组件窗口列表
	void HandleWindowsPassthrough();               // 处理组件窗口与空白区的鼠标穿透

	// === 鼠标穿透（SetWindowRgn 方案）===
	// 每帧渲染后调用：把窗口区域裁剪为所有可见组件窗口矩形的并集
	// 区域外：系统层面自动穿透（不需要 WM_NCHITTEST 处理）
	// 区域内：正常接收鼠标输入
	// 编辑模式下使用 NULL 区域（整个窗口可点击，方便拖拽和辅助线交互）
	void UpdateWindowRgn();

    // 获取所有组件（用于外部遍历，如设置窗口）
    const std::vector<std::unique_ptr<Component>>& GetComponents() const { return m_components; }

    // 配置统一分发：启动时（全局 LoadConfig 之后）调用每个组件的 LoadConfig
    void LoadAllConfigs()
    {
        for (auto& comp : m_components)
            comp->LoadConfig();
    }
    // 配置统一分发：退出时（全局 WriteConfig 之前）调用每个组件的 SaveConfig
    void SaveAllConfigs()
    {
        for (auto& comp : m_components)
            comp->SaveConfig();
    }

    // 清空并析构所有组件
    // 在 main 返回前显式调用，避免组件在静态析构阶段析构时
    // 访问已被销毁的全局资源（如纹理缓存）导致崩溃
    void Clear() { m_components.clear(); }

private:
    std::unordered_set<ImGuiID> g_componentWindowIds;
    std::vector<std::unique_ptr<Component>> m_components;
};

// 全局组件管理器，仅此一个
extern ComponentManager g_compManager;

// 位置调整模式的全局状态
namespace EditMode
{
    inline bool g_active = false;                // 是否处于位置调整模式
    inline std::string g_draggedWindow;          // 正在拖拽的窗口名称
    inline std::string g_resizingWindow;         // 正在调整大小的窗口名称（空表示未在调整）
    inline ImVec2 g_dragOffset = ImVec2(0, 0);   // 鼠标相对窗口左上角的偏移
    inline ImVec2 g_dragNewPos = ImVec2(0, 0);   // 拖拽时的新位置（含对齐修正）
    inline bool g_showAlignLineV = false;        // 是否显示垂直对齐辅助线
    inline bool g_showAlignLineH = false;        // 是否显示水平对齐辅助线
    inline float g_alignLineX = 0.0f;            // 垂直辅助线的 X 坐标
    inline float g_alignLineY = 0.0f;            // 水平辅助线的 Y 坐标

    // === 边缘调整大小（Windows 风格）===
    // 方向位标志：与 Windows 一致，四边 + 四角共 8 个方向
    constexpr int ResizeDir_None  = 0;           // 不在任何边缘
    constexpr int ResizeDir_Left  = 1 << 0;      // 左边缘
    constexpr int ResizeDir_Right = 1 << 1;      // 右边缘
    constexpr int ResizeDir_Up    = 1 << 2;      // 上边缘
    constexpr int ResizeDir_Down  = 1 << 3;      // 下边缘

    inline int g_resizeDir = ResizeDir_None;           // 当前正在调整的方向
    inline int g_hoverResizeDir = ResizeDir_None;      // 鼠标悬停的调整方向（光标 + 边缘高亮提示用）
    inline std::string g_hoverResizeWindow;            // 鼠标悬停在哪个组件的边缘上（边缘高亮用）
    inline ImVec2 g_resizeStartMouse = ImVec2(0, 0);   // 按下瞬间的鼠标位置
    inline ImVec2 g_resizeStartPos = ImVec2(0, 0);     // 按下瞬间的窗口位置（左上角）
    inline ImVec2 g_resizeStartSize = ImVec2(0, 0);    // 按下瞬间的窗口大小
    inline ImVec2 g_resizeNewPos = ImVec2(0, 0);       // 拖拽中计算出的新窗口位置（由目标窗口自己的 Draw 应用）
    inline ImVec2 g_resizeNewSize = ImVec2(0, 0);      // 拖拽中计算出的新窗口大小（由目标窗口自己的 Draw 应用）
}
