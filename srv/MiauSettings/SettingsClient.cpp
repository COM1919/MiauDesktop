#include "pch.h"
#include "SettingsClient.h"

using namespace Miau::Settings;

SettingsClient::~SettingsClient()
{
    Disconnect();
}

void SettingsClient::Disconnect()
{
    if (m_pipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
}

bool SettingsClient::Connect()
{
    if (IsConnected())
        return true;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        HANDLE pipe = CreateFileW(
            kPipeName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (pipe != INVALID_HANDLE_VALUE)
        {
            // 设置读写超时，避免主程序异常时界面卡死
            COMMTIMEOUTS timeouts{};
            timeouts.ReadTotalTimeoutConstant = 3000;
            timeouts.WriteTotalTimeoutConstant = 3000;
            SetCommTimeouts(pipe, &timeouts);

            m_pipe = pipe;
            return true;
        }

        if (GetLastError() != ERROR_PIPE_BUSY)
            return false;

        if (!WaitNamedPipeW(kPipeName, 1000))
            return false;
    }
    return false;
}

bool SettingsClient::Transact(const std::string& request, std::string& response)
{
    if (!IsConnected() && !Connect())
        return false;

    if (!WriteMessage(m_pipe, request))
    {
        Disconnect();
        return false;
    }

    DWORD available = 0;
    if (PeekNamedPipe(m_pipe, nullptr, 0, nullptr, &available, nullptr) == FALSE)
    {
        // 主程序已退出
        Disconnect();
        return false;
    }

    if (!ReadMessage(m_pipe, response))
    {
        Disconnect();
        return false;
    }
    return true;
}

bool SettingsClient::RequestState(AppState& state)
{
    Command command;
    command.type = CommandType::kHello;
    return SendCommand(command, state);
}

bool SettingsClient::SendCommand(const Command& command, AppState& state)
{
    std::string response;
    if (!Transact(CommandToJson(command), response))
        return false;
    return StateFromJson(response, state);
}
