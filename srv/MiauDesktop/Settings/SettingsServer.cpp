#include "SettingsServer.h"
#include "../Framework.h"
#include "../Component.h"
#include "../AppSettings/AppSettings.h"
#include "../../Shared/SettingsProtocol.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

using namespace Miau::Settings;

namespace
{
    // 一次请求/响应（服务线程创建，主线程填响应）
    struct Request
    {
        std::string payload;
        std::string response;
        bool done = false;
    };

    std::mutex g_mutex;
    std::condition_variable g_responseCv;
    std::vector<std::shared_ptr<Request>> g_pending;
    std::thread g_thread;
    std::atomic<bool> g_running{ false };

    // 当前已连接的管道句柄（仅服务线程读写，用于 Stop 时打断阻塞的读写）
    std::mutex g_pipeMutex;
    HANDLE g_currentPipe = INVALID_HANDLE_VALUE;

    // -------------------------------------------------------------------------
    // 图标 / 设置项构造
    // -------------------------------------------------------------------------
    std::string Utf8FromCodepoint(char32_t codepoint)
    {
        std::string out;
        if (codepoint < 0x80)
        {
            out.push_back(static_cast<char>(codepoint));
        }
        else if (codepoint < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else if (codepoint < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        return out;
    }

    // 组件名称 -> Segoe Fluent Icons 字形（留空则由设置程序使用默认图标）
    std::string GlyphForComponent(const std::string& name)
    {
        if (name == "性能监控")     return Utf8FromCodepoint(0xE9D9); // SpeedHigh
        if (name == "进程管理")     return Utf8FromCodepoint(0xE8FD); // List
        if (name == "音乐信息显示") return Utf8FromCodepoint(0xE8D6); // MusicInfo
        if (name == "DebugDemo")    return Utf8FromCodepoint(0xE7FC); // DeveloperTools
        return {};
    }

    // 构造全局设置项（软件自身设置，挂在配置根节点 Settings.*）
    std::vector<SettingItem> CollectGlobalItems()
    {
        std::vector<SettingItem> items;

        SettingItem anim;
        anim.key = "Settings.Animations.Enabled";
        anim.label = "启用组件动画";
        anim.group = "动画";
        anim.description = "关闭后组件动画直接跳到目标值";
        anim.kind = ItemKind::Toggle;
        anim.valueKind = ValueKind::Bool;
        anim.boolValue = AppSettings::g_animEnabled;
        items.push_back(anim);

        SettingItem speed;
        speed.key = "Settings.Animations.Speed";
        speed.label = "动画速度";
        speed.group = "动画";
        speed.description = "数值越大过渡越快";
        speed.kind = ItemKind::Slider;
        speed.valueKind = ValueKind::Number;
        speed.numberValue = static_cast<double>(AppSettings::g_animSpeed);
        speed.minValue = 0.2;
        speed.maxValue = 3.0;
        speed.step = 0.1;
        speed.decimals = 1;
        speed.suffix = "x";
        items.push_back(speed);

        SettingItem ease;
        ease.key = "Settings.Animations.Ease";
        ease.label = "优先动画公式";
        ease.group = "动画";
        ease.description = "组件按需取用，可自行覆盖";
        ease.kind = ItemKind::Choice;
        ease.valueKind = ValueKind::Number;
        ease.options = {
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
        ease.selectedIndex = AppSettings::g_animEaseIndex;
        ease.numberValue = static_cast<double>(AppSettings::g_animEaseIndex);
        items.push_back(ease);

        return items;
    }

    // -------------------------------------------------------------------------
    // 状态收集（主线程）
    // -------------------------------------------------------------------------
    AppState CollectState()
    {
        AppState state;
        state.editModeActive = EditMode::g_active;

        for (const auto& component : g_compManager.GetComponents())
        {
            ComponentState compState;
            compState.name = component->GetName();
            const char* description = component->GetDescription();
            compState.description = description != nullptr ? description : "";
            compState.iconGlyph = GlyphForComponent(compState.name);
            compState.visible = component->IsVisible();
            component->CollectSettings(compState.items);
            state.components.push_back(std::move(compState));
        }

        state.globalItems = CollectGlobalItems();
        return state;
    }

    // -------------------------------------------------------------------------
    // 命令处理（主线程）
    // -------------------------------------------------------------------------
    void ApplyGlobalSetting(const Command& command)
    {
        auto& config = Config::Instance();
        if (command.key == "Settings.Animations.Enabled")
        {
            AppSettings::g_animEnabled = command.boolValue;
            config.Set(command.key, AppSettings::g_animEnabled);
        }
        else if (command.key == "Settings.Animations.Speed")
        {
            AppSettings::g_animSpeed = static_cast<float>(command.numberValue);
            config.Set(command.key, AppSettings::g_animSpeed);
        }
        else if (command.key == "Settings.Animations.Ease")
        {
            int index = static_cast<int>(command.numberValue + 0.5);
            if (index < 0) index = 0;
            if (index > 8) index = 8;
            AppSettings::g_animEaseIndex = index;
            config.Set(command.key, AppSettings::g_animEaseIndex);
        }
        else
        {
            // 未识别的全局设置项：忽略
            return;
        }
        config.WriteConfig();
    }

    void ExitEditMode()
    {
        EditMode::g_active = false;
        EditMode::g_draggedWindow.clear();
        EditMode::g_resizingWindow.clear();
        EditMode::g_resizeDir = EditMode::ResizeDir_None;
        EditMode::g_hoverResizeDir = EditMode::ResizeDir_None;
        EditMode::g_hoverResizeWindow.clear();
    }

    std::string HandleRequest(const std::string& payload)
    {
        Command command;
        if (!CommandFromJson(payload, command))
            return StateToJson(CollectState());

        if (command.type == CommandType::kSetVisible)
        {
            if (Component* comp = g_compManager.FindByName(command.name))
            {
                comp->SetVisible(command.boolValue);
                AppSettings::SaveConfigNow();
            }
        }
        else if (command.type == CommandType::kSetSetting)
        {
            if (Component* comp = g_compManager.FindByName(command.name))
            {
                // 用命令中的值构造一个临时设置项，交给组件自己写配置并生效
                SettingItem item;
                item.key = command.key;
                item.valueKind = command.valueKind;
                item.boolValue = command.boolValue;
                item.numberValue = command.numberValue;
                item.stringValue = command.stringValue;
                comp->ApplySetting(item);
                AppSettings::SaveConfigNow();
            }
        }
        else if (command.type == CommandType::kSetGlobal)
        {
            ApplyGlobalSetting(command);
        }
        else if (command.type == CommandType::kResetComponent)
        {
            if (Component* comp = g_compManager.FindByName(command.name))
            {
                comp->ResetConfig();
                AppSettings::SaveConfigNow();
            }
        }
        else if (command.type == CommandType::kResetLayout)
        {
            AppSettings::ResetToDefaultLayout();
        }
        else if (command.type == CommandType::kEnterEditMode)
        {
            if (!EditMode::g_active)
            {
                AppSettings::SaveConfigNow();  // 进入位置调整前落盘一次
                ExitEditMode();
                EditMode::g_active = true;
            }
        }
        else if (command.type == CommandType::kExitEditMode)
        {
            if (EditMode::g_active)
            {
                ExitEditMode();
                AppSettings::SaveConfigNow();  // 位置调整结束保存一次
            }
        }
        else if (command.type == CommandType::kExitApp)
        {
            PostQuitMessage(0);
        }
        // kHello / kRefresh 无需额外处理，直接返回最新状态

        return StateToJson(CollectState());
    }

    // -------------------------------------------------------------------------
    // 服务线程
    // -------------------------------------------------------------------------
    void ServerThread()
    {
        while (g_running.load())
        {
            HANDLE pipe = CreateNamedPipeW(
                kPipeName,
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                64 * 1024,
                64 * 1024,
                0,
                nullptr);

            if (pipe == INVALID_HANDLE_VALUE)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            const BOOL connected = ConnectNamedPipe(pipe, nullptr);
            if (!connected && GetLastError() != ERROR_PIPE_CONNECTED)
            {
                CloseHandle(pipe);
                continue;
            }

            if (!g_running.load())
            {
                CloseHandle(pipe);
                break;
            }

            {
                std::lock_guard<std::mutex> lock(g_pipeMutex);
                g_currentPipe = pipe;
            }

            while (g_running.load())
            {
                std::string payload;
                if (!ReadMessage(pipe, payload))
                    break;

                auto request = std::make_shared<Request>();
                request->payload = std::move(payload);

                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_pending.push_back(request);
                }

                {
                    std::unique_lock<std::mutex> lock(g_mutex);
                    g_responseCv.wait(lock, [&request]()
                        {
                            return request->done || !g_running.load();
                        });
                }

                if (!g_running.load())
                    break;

                if (!WriteMessage(pipe, request->response))
                    break;
            }

            {
                std::lock_guard<std::mutex> lock(g_pipeMutex);
                g_currentPipe = INVALID_HANDLE_VALUE;
            }

            FlushFileBuffers(pipe);
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }
    }
}

void SettingsServer::Start()
{
    if (g_running.exchange(true))
        return;
    g_thread = std::thread(ServerThread);
}

void SettingsServer::Stop()
{
    if (!g_running.exchange(false))
        return;

    // 唤醒等待响应的服务线程（此时不再写回，直接退出）
    g_responseCv.notify_all();

    {
        std::lock_guard<std::mutex> lock(g_pipeMutex);
        if (g_currentPipe != INVALID_HANDLE_VALUE)
            DisconnectNamedPipe(g_currentPipe);
    }

    // 唤醒可能阻塞在 ConnectNamedPipe 上的服务线程
    HANDLE wake = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (wake != INVALID_HANDLE_VALUE)
        CloseHandle(wake);

    if (g_thread.joinable())
        g_thread.join();

    std::lock_guard<std::mutex> lock(g_mutex);
    g_pending.clear();
}

void SettingsServer::Tick()
{
    std::vector<std::shared_ptr<Request>> requests;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_pending.empty())
            return;
        requests.swap(g_pending);
    }

    for (auto& request : requests)
    {
        request->response = HandleRequest(request->payload);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            request->done = true;
        }
    }

    g_responseCv.notify_all();
}
