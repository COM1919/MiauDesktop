#pragma once
#include "../Framework.h"
#include "../Component.h"

// 托盘图标自定义消息
#define WM_TRAYICON (WM_USER + 100)

namespace AppSettings
{
    inline NOTIFYICONDATAW g_nid = {};    // 托盘图标数据
    inline HWND g_hWnd = nullptr;         // 托盘句柄

    // === 全局动画设置（缓存，启动时从配置加载，设置程序修改时写回配置并触发保存）===
    // 配置路径：Settings.Animations.*（挂在配置根节点，非组件名下）
    inline bool g_animEnabled = true;    // 是否启用组件动画
    inline float g_animSpeed = 1.0f;     // 动画速度倍速
    inline int g_animEaseIndex = 5;      // 优先动画公式索引：0 = Lerp（指数平滑），1..8 = GuiEase(索引-1)，默认 OutCubic

    // 主循环钩子
    void PostDraw();                 // 每帧绘制结束后调用（编辑模式辅助线与按钮）
    void PreDraw();                  // 每帧绘制开始前调用（编辑模式拖拽/缩放处理）
    void ResetToDefaultLayout();     // 恢复组件默认布局

    // 托盘
    void ShowTrayMenu();
    void RemoveTrayIcon();
    void InitTrayIcon(HWND hWnd);

    // 启动（或激活）独立的 WinUI3 设置程序
    void OpenSettingsApp();

    // 动画设置读取 API（供组件按需取用："优先"而非强制）
    bool IsAnimationEnabled();
    float GetAnimationSpeed();
    bool IsLerpPreferred();
    GuiBase::GuiEase GetPreferredEase();
    // 从配置加载全局设置（启动时调用）
    void LoadGlobalSettings();
    // 实时保存配置（各组件配置写回 + 全局配置写盘）
    void SaveConfigNow();
}
