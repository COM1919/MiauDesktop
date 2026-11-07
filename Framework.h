#pragma once

#include <winrt/base.h> // 需要在Windows.h前
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "runtimeobject.lib")

// 简化代码
using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Storage::Streams;

#include <windows.h>
#include <wincodec.h>
#include <iostream>
#include <mmsystem.h>
#include <math.h>
#include <io.h>
#include <shlobj.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <chrono>
#include <time.h>
#include <sstream>
#include <fstream>
#include <thread>
#include <array>
#include <codecvt>
#include <future>
#include <comdef.h>
#include <map>
#include <algorithm>
#include <deque>
#include <queue>
#include <shared_mutex>
#include <filesystem>

#include <pdh.h>
#pragma comment(lib, "pdh.lib")

#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include "ImGui/imconfig.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imstb_rectpack.h"
#include "ImGui/imstb_textedit.h"
#include "ImGui/imstb_truetype.h"
#include "ImGui/implot.h"
#include "ImGui/implot_internal.h"
#include "ImGui/imgui_range_slider.h"
#include "ImGui/implot3d.h"
#include "ImGui/imgui_stdlib.h"
#include "ImGui/TextEditor/TextEditor.h"
#include "ImGui/ImGuiToggle/imgui_toggle.h"
#include "ImGui/Icons/IconsFontAwesome6.h"
#include "ImGui/Icons/fa.h"
#include "ImGui/ImGuiMD/imgui_md.h"
#include "ImGui/ImAnim/im_anim.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// DirectComposition：用于实现 per-pixel 半透明窗口（替代 LWA_COLORKEY 色键方案）
#include <dcomp.h>
#pragma comment(lib, "dcomp.lib")

typedef ID3D11ShaderResourceView* Texture2D;

// 树形配置系统（任何组件可通过 Config::Instance() 使用）
#include "Config/Config.h"

#undef min
#undef max