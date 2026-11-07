#include "pch.h"

#include "App.xaml.h"
#include "MainWindow.xaml.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Microsoft::UI::Xaml;
}

namespace winrt::MiauSettings::implementation
{
    App::App()
    {
        InitializeComponent();

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](winrt::IInspectable const&, winrt::UnhandledExceptionEventArgs const& e)
            {
                if (IsDebuggerPresent())
                {
                    auto errorMessage = e.Message();
                    (void)errorMessage;
                    __debugbreak();
                }
            });
#endif
    }

    void App::OnLaunched(winrt::LaunchActivatedEventArgs const&)
    {
        // 单实例：已存在实例时，唤起它并退出自己
        static HANDLE s_instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\MiauSettings.SingleInstance");
        if (s_instanceMutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (HANDLE showEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\MiauSettings.Show"))
            {
                SetEvent(showEvent);
                CloseHandle(showEvent);
            }
            Exit();
            return;
        }

        window = winrt::make<MainWindow>();
        window.Activate();
    }
}
