#pragma once
#include "../Framework.h"
#include "../Component.h"

// 托盘图标自定义消息
#define WM_TRAYICON (WM_USER + 100)

// 设置窗口尺寸常量（必须在 SettingsUI 之前定义）
constexpr float settingsPadding = 20.0f;
constexpr ImVec2 settingMenuSize = ImVec2(900, 556);
constexpr ImVec2 mainPanelSize = ImVec2(settingMenuSize.x - 190.0f, settingMenuSize.y);
constexpr float moduleCardWidth = mainPanelSize.x - settingsPadding * 2.0f;

#include "SettingsPanel.h"
#include "ComponentCard.h"
#include "SettingsButton.h"

// 设置界面的导航页面类型
enum class SettingsPage
{
    ComponentList,      // 组件列表
    ComponentSettings,  // 单个组件的设置页面
    GlobalSettings,     // 全局设置（软件自身设置）
    MorePanel,          // 更多面板
    AboutPanel          // 关于面板
};

namespace AppSettings
{
    inline bool g_showSettings = false;   // 是否显示设置窗口
    inline NOTIFYICONDATAW g_nid = {};    // 托盘图标数据
    inline HWND g_hWnd = nullptr;         // 托盘句柄

    // 设置界面状态
    inline SettingsPage g_currentPage = SettingsPage::ComponentList;
    inline Component* g_selectedComponent = nullptr;
    inline bool g_isInModuleSettings = false;

    // 左栏导航
    inline NavPanel* g_navPanel = nullptr;
    inline int g_lastSelectedIndex = 0;

    // 分类栏
    inline FilterBar* g_filterBar = nullptr;

    // 组件卡片列表
    inline std::vector<ComponentCard*> g_moduleCards;

    // 动画数值变量
    inline float g_animAlpha = 0.0f;

    // === 全局动画设置（缓存，启动时从配置加载，设置页修改时写回配置并触发保存）===
    // 配置路径：Settings.Animations.*（挂在配置根节点，非组件名下）
    inline bool g_animEnabled = true;    // 是否启用组件动画
    inline float g_animSpeed = 1.0f;     // 动画速度倍速
    inline int g_animEaseIndex = 5;      // 优先动画公式索引：0 = Lerp（指数平滑），1..8 = GuiEase(索引-1)，默认 OutCubic

    void PostDraw();
    void ResetToDefaultLayout();
    void PreDraw();
    void RenderSettingsWindow();
    void ShowTrayMenu();
    void RemoveTrayIcon();
    void InitTrayIcon(HWND hWnd);
    void EnterSettingsUI();

    // 组件子设置页面渲染
    void RenderComponentSettings(Component* comp);
    // 全局"设置"页渲染（软件自身设置）
    void RenderGlobalSettings();
    // 动画设置读取 API（供组件按需取用："优先"而非强制）
    bool IsAnimationEnabled();
    float GetAnimationSpeed();
    bool IsLerpPreferred();
    GuiBase::GuiEase GetPreferredEase();
    // 从配置加载全局设置（启动时调用）
    void LoadGlobalSettings();
    // 实时保存配置（各组件配置写回 + 全局配置写盘）
    void SaveConfigNow();

    // === 设置界面专属主题 ===
    // 集中管理设置窗口的 ColorStyle（Push/Pop 整组），组件渲染使用全局默认 ColorStyle，
    // 两者互不影响——添加新组件无需再写任何 PushStyleColor
    void PushSettingsTheme(float dimAlpha);   // dimAlpha：遮罩淡入透明度（0~1）
    void PopSettingsTheme();

    // 初始化/销毁设置界面
    void InitSettingsUI();
    void CleanupSettingsUI();
}
