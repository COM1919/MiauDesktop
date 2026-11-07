#pragma once
#include "Framework.h"
#include "Component.h"
#include "GuiBase/GuiBase.h"
#include "AppSettings/AppSettings.h"

namespace Components
{
#include "Components/PerformanceMonitor.h"
#include "Components/ProcessManager.h"
#include "Components/DebugDemo.h"
#include "Components/MusicDisplay.h"
}

namespace ScrollAnim
{
	// 滑动条动画的全局变量，用于累积滚动能量
	static float g_EnergyX = 0.0f;
	static float g_EnergyY = 0.0f;
	const float SCROLL_MULTIPLIER = 1.6f;  // 速度倍率
	const float SMOOTHING = 19.0f;         // 平滑系数
	void HandleScrolling();
}

HWND g_hWnd = nullptr;

int RenderGui(HINSTANCE hInstance);
void SetupImGuiStyle();
void UpdateWindowRgn();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace FilePath
{
	const std::string Font = "C:\\Windows\\Fonts\\msyhbd.ttc";
	const std::string Config = "Config\\config.json";
}

#define MD_DEBUG  // 开启开发者Debug模式

#ifdef MD_DEBUG

#define MD_DEBUG_CMD
#define MD_TOPSHOW // 让窗口显示在上层，方便测试UI

#endif // MD_DEBUG