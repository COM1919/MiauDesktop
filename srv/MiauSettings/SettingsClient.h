#pragma once
#include "pch.h"

#include "../../Shared/SettingsProtocol.h"

// 与主程序（MiauDesktop.exe）通信的命名管道客户端
// 采用"同步请求-响应"模型：每条命令都会带回一份最新状态快照，
// 界面无需额外的状态同步逻辑。
class SettingsClient
{
public:
    SettingsClient() = default;
    ~SettingsClient();

    SettingsClient(const SettingsClient&) = delete;
    SettingsClient& operator=(const SettingsClient&) = delete;

    // 连接主程序；失败返回 false
    bool Connect();
    bool IsConnected() const { return m_pipe != INVALID_HANDLE_VALUE; }
    void Disconnect();

    // 请求一次完整状态
    bool RequestState(Miau::Settings::AppState& state);

    // 发送命令并取回最新状态
    bool SendCommand(const Miau::Settings::Command& command, Miau::Settings::AppState& state);

private:
    // 发送文本请求并读取一条响应
    bool Transact(const std::string& request, std::string& response);

    HANDLE m_pipe = INVALID_HANDLE_VALUE;
};
