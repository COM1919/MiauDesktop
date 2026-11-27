// GUIBase.cpp
// 基础控件 + 纹理系统 + D3D11/DirectComposition 初始化的实现（声明见 BaseUI/GUIBase.h）

#include "GUIBase.h"

#include <shobjidl.h>
#include <cstdio>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "../Stb/stb_image.h"
#include "../Stb/stb_image_write.h"

void GuiBase::TitleText(const char* label)
{
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text(label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
}

void GuiBase::DrawGlowBorder(ImVec2 min, ImVec2 max, ImU32 color, float thickness)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 绘制多层半透明边框，从内到外逐渐变淡
    const int count = thickness * 10;
    for (int i = 0; i < count; i++)
    {
        float alpha = 1.0f - (static_cast<float>(i + 1) / count);
        float offset = i * 0.1f;

        ImU32 GlowColor = ImGui::GetColorU32(
            IM_COL32(
                (color >> IM_COL32_R_SHIFT) & 0xFF,
                (color >> IM_COL32_G_SHIFT) & 0xFF,
                (color >> IM_COL32_B_SHIFT) & 0xFF,
                int(alpha * 200)));

        draw_list->AddRect(
            ImVec2(min.x - offset, min.y - offset),
            ImVec2(max.x + offset, max.y + offset),
            GlowColor, style.ChildRounding,
            0.1f, 0);
    }
}

namespace
{
    // 共用的圆弧绘制：沿圆环采样后一次提交 polyline
    // ProgressBarArc 与 GuiGauge 都走这里，避免各自重复写采样循环
    void DrawGuiArcPolyline(ImDrawList* drawList, ImVec2 center, float radius,
                            float startAngle, float sweepAngle, int segments,
                            ImU32 color, float thickness)
    {
        if (drawList == nullptr || segments <= 0)
        {
            return;
        }
        std::vector<ImVec2> points;
        points.reserve(static_cast<std::size_t>(segments) + 1);
        for (int index = 0; index <= segments; ++index)
        {
            const float angle = startAngle + sweepAngle * static_cast<float>(index) / static_cast<float>(segments);
            points.emplace_back(center.x + cosf(angle) * radius,
                                center.y + sinf(angle) * radius);
        }
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color, thickness, ImDrawFlags_None);
    }
}

void GuiBase::ProgressBarArc(const char* label, float progress, ImVec2 size, ImVec4 Color, ImVec4 bgColor, float thickness)
{
    ImGui::PushID(label);
    ImGuiID id = ImGui::GetID(label);
    ImGuiContext& g = *GImGui;

    static std::unordered_map<ImGuiID, float> animMap;
    float& anim = animMap[id];
    float target = ImClamp(progress, 0.0f, 1.0f);

    const float speed = 1.0f - powf(1.0f - 0.15f, g.IO.DeltaTime * 60.0f);
    anim = ImLerp(anim, target, speed);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bgColor);
    ImGui::BeginChild(label, ImVec2(size.x + 2, size.y + 2), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
    {
        ImGui::EndChild();
        ImGui::PopID();
        return;
    }

    ImDrawList* draw_list = window->DrawList;
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 center(pos.x + size.x / 2, pos.y + size.y / 2);
    float radius = ImMin(size.x / 2 - thickness / 2, size.y / 2 - thickness / 2);

    ImU32 col_border = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 col_fg = ImGui::GetColorU32(Color);

    int segments = ImMax(16, (int)(radius * 0.8f));
    segments = ImMin(segments, 200);

    auto drawArc = [&](float progress_val, ImU32 color, float thick)
    {
        if (progress_val <= 0.0f)
        {
            return;
        }
        const float angle_min = 40.0f;
        const float angle_max = -(180.0f + 40.0f);
        const float angle_min_rad = angle_min * (IM_PI / 180.0f);
        const float angle_max_rad = angle_max * (IM_PI / 180.0f);
        const float angle_range = angle_max_rad - angle_min_rad;

        const int count = ImMax(2, (int)(segments * progress_val));
        DrawGuiArcPolyline(draw_list, center, radius, angle_min_rad,
                           angle_range * progress_val, count, color, thick);
    };

    // 背景半圆弧
    drawArc(1.0f, col_border, thickness);
    // 进度弧（使用动画值 anim）
    drawArc(anim, col_fg, thickness);

    // 文字布局（两行：标签在上，百分比在下，均居中）
    const float font_size = ImGui::GetFontSize();
    const float text_spacing = 2.0f;

    char percent_text[32];
    snprintf(percent_text, sizeof(percent_text), "%d%%", (int)(target * 100.0f));
    ImVec2 percent_size = ImGui::CalcTextSize(percent_text);

    bool show_label = (strstr(label, "##") != label);
    ImVec2 label_size(0, 0);
    if (show_label)
    {
        label_size = ImGui::CalcTextSize(label);
    }

    float total_height = 0.0f;
    if (show_label)
    {
        total_height = label_size.y + text_spacing + percent_size.y;
    }
    else
    {
        total_height = percent_size.y;
    }

    float start_y = pos.y + (size.y - total_height) / 2.0f;

    if (show_label)
    {
        ImVec2 label_pos(pos.x + (size.x - label_size.x) / 2.0f, start_y);
        draw_list->AddText(NULL, 0.0f, label_pos,
            ImGui::GetColorU32(ImGuiCol_Text), label);
        start_y += label_size.y + text_spacing;
    }

    ImVec2 percent_pos(pos.x + (size.x - percent_size.x) / 2.0f, start_y);
    draw_list->AddText(NULL, 0.0f, percent_pos,
        ImGui::GetColorU32(ImGuiCol_Text), percent_text);

    ImGui::EndChild();
    ImGui::PopID();
}

void GuiBase::GuiGauge(const char* label, float progress, const ImVec2& size,
                       GuiColor fillColor, const GuiGaugeOptions& options)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float value = std::clamp(progress, 0.0f, 1.0f);

    const float centerX = cursor.x + size.x * 0.5f;
    const float centerY = cursor.y + options.centerYOffset;
    const float outerRadius = options.outerRadius;
    const float thickness = options.thickness;

    // 轨道圆环：使用 StrokeCenter，让轨道与进度弧覆盖同一半径范围
    drawList->AddCircle(ImVec2(centerX, centerY), outerRadius,
                        options.trackColor.ToImU32(), 64, thickness, ImDrawFlags_StrokeCenter);

    // 进度弧（顶部起始，顺时针）
    if (value > 0.001f)
    {
        const float startAngle = -IM_PI * 0.5f;
        const float sweep = value * IM_PI * 2.0f;
        const int segments = std::max(16, static_cast<int>(value * 64.0f));
        DrawGuiArcPolyline(drawList, ImVec2(centerX, centerY), outerRadius,
                           startAngle, sweep, segments, fillColor.ToImU32(), thickness);
    }

    // 中心百分比
    char percentText[16];
    std::snprintf(percentText, sizeof(percentText), "%d%%", static_cast<int>(value * 100.0f));
    const ImVec2 percentSize = ImGui::CalcTextSize(percentText);
    drawList->AddText(ImGui::GetFont(), options.percentFontSize,
                      ImVec2(centerX - percentSize.x * 0.5f, centerY - percentSize.y * 0.5f),
                      options.textColor.ToImU32(), percentText);

    // 底部标签
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    drawList->AddText(ImGui::GetFont(), options.labelFontSize,
                      ImVec2(centerX - labelSize.x * 0.5f, centerY + outerRadius + options.labelGap),
                      options.labelColor.ToImU32(), label);

    ImGui::Dummy(size);
}

bool GuiBase::CircularButton(const char* label, ImVec2 size)
{
    bool ret;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 24.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ret = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    return ret;
}

struct TextureInfo
{
    Texture2D texture;
    int width;
    int height;
};

namespace TextureCache
{
    // 使用堆分配的 inline 引用：对象永不析构（进程退出时由系统回收）
    // 原因：组件（如 ProcessManager）是全局对象，在 main 返回后的静态析构阶段析构，
    //       若这里使用普通的全局静态 map，可能先于组件被销毁，
    //       导致 UnloadTexture 访问已析构的容器而崩溃（读取访问权限冲突）。
    //       堆分配的对象不会参与静态析构，无论调用时机如何都安全。
    inline std::shared_mutex& mutex = *new std::shared_mutex;
    inline std::map<std::string, TextureInfo>& g_list = *new std::map<std::string, TextureInfo>;
}

Texture2D GuiBase::LoadTextureFromMemory(const unsigned char* buffer, int len, const char* id, bool CacheDisabled, int* w, int* h)
{
    // 检查缓存
    if (!CacheDisabled)
    {
        std::shared_lock<std::shared_mutex> lock(TextureCache::mutex);
        auto it = TextureCache::g_list.find(id);
        if (it != TextureCache::g_list.end())
        {
            if (w != nullptr)
            {
                *w = it->second.width;
            }
            if (h != nullptr)
            {
                *h = it->second.height;
            }
            return it->second.texture;
        }
    }

    // 加载图片（强制 4 通道 RGBA）
    int image_width = 0;
    int image_height = 0;
    int nrChannels = 0;

    unsigned char* image_data = stbi_load_from_memory(buffer, len, &image_width, &image_height, &nrChannels, 4);
    if (image_data == nullptr)
    {
        std::cerr << "[Render] 从内存加载图片失败，id: " << id << std::endl;
        return nullptr;
    }

    Texture2D textureSRV = LoadTexture(image_data, image_width, image_height, w, h, id);

    stbi_image_free(image_data);

    // 添加到缓存
    if (!CacheDisabled && textureSRV != nullptr)
    {
        std::unique_lock<std::shared_mutex> lock(TextureCache::mutex);
        TextureCache::g_list[id].texture = textureSRV;
        TextureCache::g_list[id].width = image_width;
        TextureCache::g_list[id].height = image_height;
    }

    return textureSRV;
}

Texture2D GuiBase::LoadTexture(unsigned char* data, int& image_width, int& image_height, int* w, int* h, const char* id)
{
    if (image_width <= 0 || image_height <= 0)
    {
        image_width = 32;
        image_height = 32;
    }
    if (w != nullptr)
    {
        *w = image_width;
    }
    if (h != nullptr)
    {
        *h = image_height;
    }

    ID3D11Texture2D* pTexture = nullptr;
    Texture2D textureSRV = nullptr;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = data;
    initData.SysMemPitch = image_width * 4;
    initData.SysMemSlicePitch = image_height * initData.SysMemPitch;

    HRESULT hr = RenderData::g_pd3dDevice->CreateTexture2D(&desc, &initData, &pTexture);
    if (FAILED(hr))
    {
        std::cerr << "[Render] 创建纹理失败，id: " << id;
        return nullptr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = RenderData::g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &textureSRV);
    pTexture->Release();

    if (FAILED(hr))
    {
        std::cerr << "[Render] 创建着色器资源视图失败，id: " << id << std::endl;
        return nullptr;
    }
    return textureSRV;
}

Texture2D GuiBase::LoadTextureFromFile(const std::string& filename, bool CacheDisabled, int* w, int* h)
{
    // 检查缓存
    if (!CacheDisabled)
    {
        std::shared_lock<std::shared_mutex> lock(TextureCache::mutex);
        auto it = TextureCache::g_list.find(filename);
        if (it != TextureCache::g_list.end())
        {
            if (w != nullptr)
            {
                *w = it->second.width;
            }
            if (h != nullptr)
            {
                *h = it->second.height;
            }
            return it->second.texture;
        }
    }

    // 检查文件是否存在
    if (_access(filename.c_str(), 0) != 0)
    {
        std::cerr << "[Render] 无效的图片路径: " << filename << std::endl;
        return nullptr;
    }

    // 加载图片（强制 4 通道 RGBA）
    int image_width = 0;
    int image_height = 0;
    int nrChannels = 0;

    unsigned char* image_data = stbi_load(filename.c_str(), &image_width, &image_height, &nrChannels, 4);
    if (image_data == nullptr)
    {
        std::cerr << "[Render] 加载图片失败: " << filename << std::endl;
        return nullptr;
    }

    Texture2D textureSRV = LoadTexture(image_data, image_width, image_height, w, h, filename.c_str());

    stbi_image_free(image_data);

    // 添加到缓存
    if (!CacheDisabled && textureSRV != nullptr)
    {
        std::unique_lock<std::shared_mutex> lock(TextureCache::mutex);
        TextureCache::g_list[filename].texture = textureSRV;
        TextureCache::g_list[filename].width = image_width;
        TextureCache::g_list[filename].height = image_height;
    }

    return textureSRV;
}

// 清理纹理缓存
void GuiBase::ClearAllTextureCache()
{
    std::unique_lock<std::shared_mutex> lock(TextureCache::mutex);
    for (auto& pair : TextureCache::g_list)
    {
        if (pair.second.texture != nullptr)
        {
            pair.second.texture->Release();
        }
    }
    TextureCache::g_list.clear();
}

// 释放并且删除纹理和缓存
void GuiBase::UnloadTexture(const std::string& id)
{
    std::unique_lock<std::shared_mutex> lock(TextureCache::mutex);
    auto it = TextureCache::g_list.find(id);
    if (it != TextureCache::g_list.end())
    {
        if (it->second.texture)
        {
            it->second.texture->Release();
        }
        TextureCache::g_list.erase(it);
    }
}

bool CreateDeviceD3D(HWND hWnd)
{
    // 创建 D3D11 设备（不直接创建 swap chain，因为要用 composition swap chain）
    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &RenderData::g_pd3dDevice, &featureLevel, &RenderData::g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
    {
        res = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &RenderData::g_pd3dDevice, &featureLevel, &RenderData::g_pd3dDeviceContext);
    }
    if (res != S_OK)
    {
        return false;
    }

    // 获取 DXGI Device / Factory
    IDXGIDevice* dxgiDevice = nullptr;
    if (FAILED(RenderData::g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice))))
    {
        return false;
    }
    IDXGIAdapter* dxgiAdapter = nullptr;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    IDXGIFactory* dxgiFactoryBase = nullptr;
    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactoryBase));
    IDXGIFactory2* dxgiFactory = nullptr;
    dxgiFactoryBase->QueryInterface(IID_PPV_ARGS(&dxgiFactory));
    dxgiFactoryBase->Release();
    dxgiAdapter->Release();

    // 创建 Composition Swap Chain（AlphaMode = PREMULTIPLIED 支持 per-pixel 半透明）
    RECT rc;
    GetClientRect(hWnd, &rc);
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = (rc.right - rc.left) > 0 ? (rc.right - rc.left) : 1;
    sd.Height = (rc.bottom - rc.top) > 0 ? (rc.bottom - rc.top) : 1;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.Stereo = FALSE;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    sd.Flags = 0;

    res = dxgiFactory->CreateSwapChainForComposition(dxgiDevice, &sd, nullptr, (IDXGISwapChain1**)&RenderData::g_pSwapChain);
    dxgiFactory->Release();
    if (res != S_OK)
    {
        dxgiDevice->Release();
        return false;
    }

    // 初始化 DirectComposition：把 swap chain 绑定到窗口
    if (!InitDirectComposition(dxgiDevice, hWnd))
    {
        dxgiDevice->Release();
        return false;
    }
    dxgiDevice->Release();

    CreateRenderTarget();
    return true;
}

bool InitDirectComposition(IDXGIDevice* dxgiDevice, HWND hWnd)
{
    if (FAILED(DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&RenderData::g_pDCompDevice))))
    {
        return false;
    }

    if (FAILED(RenderData::g_pDCompDevice->CreateTargetForHwnd(hWnd, TRUE, &RenderData::g_pDCompTarget)))
    {
        return false;
    }

    if (FAILED(RenderData::g_pDCompDevice->CreateVisual(&RenderData::g_pDCompVisual)))
    {
        return false;
    }
    if (FAILED(RenderData::g_pDCompVisual->SetContent(RenderData::g_pSwapChain)))
    {
        return false;
    }

    if (FAILED(RenderData::g_pDCompTarget->SetRoot(RenderData::g_pDCompVisual)))
    {
        return false;
    }

    if (FAILED(RenderData::g_pDCompDevice->Commit()))
    {
        return false;
    }

    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();

    // 先释放 DComp 资源（解除 swap chain 绑定）
    if (RenderData::g_pDCompVisual)
    {
        RenderData::g_pDCompVisual->Release();
        RenderData::g_pDCompVisual = nullptr;
    }
    if (RenderData::g_pDCompTarget)
    {
        RenderData::g_pDCompTarget->Release();
        RenderData::g_pDCompTarget = nullptr;
    }
    if (RenderData::g_pDCompDevice)
    {
        RenderData::g_pDCompDevice->Release();
        RenderData::g_pDCompDevice = nullptr;
    }

    if (RenderData::g_pSwapChain)
    {
        RenderData::g_pSwapChain->Release();
        RenderData::g_pSwapChain = nullptr;
    }
    if (RenderData::g_pd3dDeviceContext)
    {
        RenderData::g_pd3dDeviceContext->Release();
        RenderData::g_pd3dDeviceContext = nullptr;
    }
    if (RenderData::g_pd3dDevice)
    {
        RenderData::g_pd3dDevice->Release();
        RenderData::g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    RenderData::g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    RenderData::g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &RenderData::g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (RenderData::g_mainRenderTargetView)
    {
        RenderData::g_mainRenderTargetView->Release();
        RenderData::g_mainRenderTargetView = nullptr;
    }
}

// =============================================================================
// 原生系统对话框（Win32 IFileDialog）
// 移植自 EUI-NEO v0.6.3 core/platform/platform.cpp 的 saveFileDialog /
// selectDirectoryDialog。宿主只负责选择路径，文件读写由调用方完成。
// =============================================================================
namespace GuiBase
{
    namespace
    {
        std::wstring Utf8ToWide(const char* text)
        {
            if (text == nullptr || text[0] == '\0')
            {
                return std::wstring();
            }
            const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
            if (length <= 0)
            {
                return std::wstring();
            }
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text, -1, &result[0], length);
            result.resize(static_cast<std::size_t>(length - 1));
            return result;
        }

        std::string WideToUtf8(const wchar_t* text)
        {
            if (text == nullptr || text[0] == L'\0')
            {
                return std::string();
            }
            const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (length <= 0)
            {
                return std::string();
            }
            std::string result(static_cast<std::size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], length, nullptr, nullptr);
            result.resize(static_cast<std::size_t>(length - 1));
            return result;
        }

        std::string FormatHResultText(HRESULT value, const char* prefix)
        {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%s0x%08lX", prefix,
                          static_cast<unsigned long>(value));
            return std::string(buffer);
        }

        // save = true 走保存对话框；false 且 pickFolders = true 走目录选择
        GuiFileDialogResult ShowShellFileDialog(bool save, const GuiFileDialogOptions& options)
        {
            GuiFileDialogResult result;

            const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            const bool shouldUninitialize = SUCCEEDED(comResult);
            if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
            {
                result.status = GuiFileDialogStatus::Failed;
                result.error = FormatHResultText(comResult, "COM 初始化失败：");
                return result;
            }

            IFileDialog* dialog = nullptr;
            HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
                                          nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&dialog));
            if (FAILED(hr) || dialog == nullptr)
            {
                result.status = GuiFileDialogStatus::Failed;
                result.error = FormatHResultText(hr, "创建系统对话框失败：");
                if (shouldUninitialize)
                {
                    CoUninitialize();
                }
                return result;
            }

            FILEOPENDIALOGOPTIONS flags = 0;
            if (SUCCEEDED(dialog->GetOptions(&flags)))
            {
                flags |= FOS_FORCEFILESYSTEM;
                if (save)
                {
                    flags |= FOS_OVERWRITEPROMPT;
                }
                else
                {
                    flags |= FOS_PICKFOLDERS;
                }
                dialog->SetOptions(flags);
            }

            const std::wstring title = Utf8ToWide(options.title);
            if (!title.empty())
            {
                dialog->SetTitle(title.c_str());
            }

            const std::wstring suggestedName = Utf8ToWide(options.suggestedName);
            if (!suggestedName.empty())
            {
                dialog->SetFileName(suggestedName.c_str());
            }

            const std::wstring extension = Utf8ToWide(options.defaultExtension);
            if (!extension.empty())
            {
                dialog->SetDefaultExtension(extension.c_str());
            }

            if (options.filters != nullptr && options.filterCount > 0)
            {
                // COMDLG_FILTERSPEC 只持有指针，宽字符串需要在整个 Show 期间存活；
                // 先 reserve 再 push，避免 vector 扩容使 c_str() 失效。
                std::vector<std::wstring> storage;
                storage.reserve(static_cast<std::size_t>(options.filterCount) * 2);
                for (int index = 0; index < options.filterCount; ++index)
                {
                    storage.push_back(Utf8ToWide(options.filters[index].name));
                    storage.push_back(Utf8ToWide(options.filters[index].patterns));
                }

                std::vector<COMDLG_FILTERSPEC> specs(static_cast<std::size_t>(options.filterCount));
                for (int index = 0; index < options.filterCount; ++index)
                {
                    const std::size_t offset = static_cast<std::size_t>(index) * 2;
                    specs[static_cast<std::size_t>(index)].pszName = storage[offset].c_str();
                    specs[static_cast<std::size_t>(index)].pszSpec = storage[offset + 1].c_str();
                }
                dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
                dialog->SetFileTypeIndex(1);
            }

            HWND owner = options.owner;
            if (owner == nullptr)
            {
                if (ImGuiViewport* viewport = ImGui::GetMainViewport())
                {
                    owner = static_cast<HWND>(viewport->PlatformHandle);
                }
            }

            hr = dialog->Show(owner);
            if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            {
                result.status = GuiFileDialogStatus::Cancelled;
            }
            else if (SUCCEEDED(hr))
            {
                IShellItem* item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
                {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                    {
                        result.widePath = path;
                        result.path = WideToUtf8(path);
                        result.status = GuiFileDialogStatus::Selected;
                        CoTaskMemFree(path);
                    }
                    else
                    {
                        result.status = GuiFileDialogStatus::Failed;
                        result.error = "无法读取所选路径";
                    }
                    item->Release();
                }
                else
                {
                    result.status = GuiFileDialogStatus::Failed;
                    result.error = "系统对话框没有返回结果";
                }
            }
            else
            {
                result.status = GuiFileDialogStatus::Failed;
                result.error = FormatHResultText(hr, "系统对话框调用失败：");
            }

            // 原生模态对话框会独占鼠标消息，主窗口收不到按钮抬起事件；
            // 返回后显式复位，避免 ImGui 认为鼠标仍处于按下状态。
            ImGuiIO& io = ImGui::GetIO();
            io.AddMouseButtonEvent(0, false);
            io.AddMouseButtonEvent(1, false);
            io.AddMouseButtonEvent(2, false);

            dialog->Release();
            {
                CoUninitialize();
            }
            return result;
        }
    }

    GuiFileDialogResult GuiSaveFileDialog(const GuiFileDialogOptions& options)
    {
        return ShowShellFileDialog(true, options);
    }

    GuiFileDialogResult GuiSelectDirectoryDialog(const GuiFileDialogOptions& options)
    {
        return ShowShellFileDialog(false, options);
    }
}
