#pragma once
#include "pch.h"
#include "MainWindow.g.h"
#include "SettingsClient.h"

namespace winrt::MiauSettings::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

    private:
        // ---- 事件 ----
        void OnNavSelectionChanged(
            winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);
        void OnRefreshClick(winrt::Windows::Foundation::IInspectable const& sender,
                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnExitAppClick(winrt::Windows::Foundation::IInspectable const& sender,
                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        // ---- 数据 ----
        bool RefreshState();
        void SendCommand(const Miau::Settings::Command& command);
        void UpdateConnectionStatus();
        void RebuildContent();
        void BuildComponentsPage();
        void BuildGlobalPage();
        void BuildLayoutPage();
        void BuildAboutPage();

        // ---- 控件构造辅助 ----
        void AddPageHeader(winrt::hstring const& title, winrt::hstring const& subtitle);
        void AddGroupHeader(std::string const& group);
        winrt::Microsoft::UI::Xaml::Controls::Border CreateSettingRow(
            Miau::Settings::SettingItem const& item,
            std::string const& componentName);

        // 监听"再次启动设置程序"事件（已运行实例被唤起）
        void StartShowEventWatcher();

        std::unique_ptr<SettingsClient> m_client;
        Miau::Settings::AppState m_state;
        std::wstring m_currentTag{ L"components" };

        HANDLE m_showEvent = nullptr;
        std::atomic<bool> m_showWatcherStop{ false };
        std::thread m_showWatcher;
    };
}

namespace winrt::MiauSettings::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
