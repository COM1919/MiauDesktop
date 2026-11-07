#include "pch.h"
#include "MainWindow.xaml.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <microsoft.ui.xaml.window.h>
#include <cwchar>
#include <thread>
#include <atomic>

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Controls::Primitives;
    using namespace Microsoft::UI::Xaml::Media;
    using namespace Microsoft::UI::Xaml::Media::Animation;
    using namespace Microsoft::UI::Dispatching;
    using namespace Windows::Graphics;
}

using namespace Miau::Settings;

namespace
{
    // 重复启动事件名（与 App.xaml.cpp 保持一致）
    constexpr const wchar_t* kShowEventName = L"Local\\MiauSettings.Show";

    winrt::hstring ToH(std::string const& text)
    {
        return winrt::to_hstring(text);
    }

    winrt::Style FindStyle(winrt::hstring const& key)
    {
        auto resources = winrt::Application::Current().Resources();
        if (resources.HasKey(winrt::box_value(key)))
        {
            if (auto style = resources.Lookup(winrt::box_value(key)).try_as<winrt::Style>())
                return style;
        }
        return nullptr;
    }

    winrt::TextBlock MakeText(winrt::hstring const& text, winrt::hstring const& styleKey, double opacity = 1.0)
    {
        winrt::TextBlock block;
        block.Text(text);
        if (auto style = FindStyle(styleKey))
            block.Style(style);
        if (opacity < 1.0)
            block.Opacity(opacity);
        return block;
    }

    winrt::hstring FormatNumber(double value, int decimals, std::string const& suffix)
    {
        wchar_t buffer[64];
        swprintf_s(buffer, L"%.*f", decimals < 0 ? 0 : decimals, value);
        std::wstring text(buffer);
        if (!suffix.empty())
            text += ToH(suffix).c_str();
        return winrt::hstring(text);
    }

    // 通用的"设置项卡片"：左侧标题/说明，右侧控件
    winrt::Border BuildRowShell(winrt::TextBlock const& title, winrt::TextBlock const* description, winrt::UIElement const& control)
    {
        winrt::Border card;
        if (auto style = FindStyle(L"SettingsCardStyle"))
            card.Style(style);

        winrt::Grid grid;
        winrt::ColumnDefinition left;
        left.Width(winrt::GridLength(1, winrt::GridUnitType::Star));
        winrt::ColumnDefinition right;
        right.Width(winrt::GridLength(0, winrt::GridUnitType::Auto));
        grid.ColumnDefinitions().Append(left);
        grid.ColumnDefinitions().Append(right);

        winrt::StackPanel textPanel;
        textPanel.VerticalAlignment(winrt::VerticalAlignment::Center);
        textPanel.Spacing(2);
        textPanel.Children().Append(title);
        if (description != nullptr)
            textPanel.Children().Append(*description);
        winrt::Grid::SetColumn(textPanel, 0);
        grid.Children().Append(textPanel);

        winrt::FrameworkElement controlElement = control.as<winrt::FrameworkElement>();
        controlElement.VerticalAlignment(winrt::VerticalAlignment::Center);
        controlElement.Margin(winrt::Thickness{ 16, 0, 0, 0 });
        winrt::Grid::SetColumn(controlElement, 1);
        grid.Children().Append(controlElement);

        card.Child(grid);
        return card;
    }
}

namespace winrt::MiauSettings::implementation
{
    // ---------------------------------------------------------------------
    // 构造 / 析构
    // ---------------------------------------------------------------------
    MainWindow::MainWindow()
    {
        InitializeComponent();

        Title(L"MiauDesktop 设置");

        // 窗口尺寸（像素，按当前 DPI 缩放由系统处理）
        if (auto appWindow = this->AppWindow())
            appWindow.Resize(winrt::SizeInt32{ 1040, 780 });

        // Win11 Mica 背景
        try
        {
            SystemBackdrop(winrt::MicaBackdrop{});
        }
        catch (...)
        {
            // 旧系统或远程桌面下 Mica 不可用，忽略即可
        }

        Nav().SelectionChanged({ this, &MainWindow::OnNavSelectionChanged });
        RefreshButton().Click({ this, &MainWindow::OnRefreshClick });
        ExitAppButton().Click({ this, &MainWindow::OnExitAppClick });

        m_client = std::make_unique<SettingsClient>();
        RefreshState();

        Nav().SelectedItem(Nav().MenuItems().GetAt(0));
        RebuildContent();

        StartShowEventWatcher();
    }

    MainWindow::~MainWindow()
    {
        m_showWatcherStop.store(true);
        if (m_showEvent != nullptr)
            SetEvent(m_showEvent);
        if (m_showWatcher.joinable())
            m_showWatcher.join();
        if (m_showEvent != nullptr)
        {
            CloseHandle(m_showEvent);
            m_showEvent = nullptr;
        }
    }

    // ---------------------------------------------------------------------
    // 单实例：已运行时再次启动会被唤起（前置窗口）
    // ---------------------------------------------------------------------
    void MainWindow::StartShowEventWatcher()
    {
        m_showEvent = CreateEventW(nullptr, FALSE, FALSE, kShowEventName);
        if (m_showEvent == nullptr)
            return;

        HWND hwnd = nullptr;
        Window self = *this;
        self.as<IWindowNative>()->get_WindowHandle(&hwnd);

        m_showWatcher = std::thread([this, hwnd]()
            {
                while (!m_showWatcherStop.load())
                {
                    if (WaitForSingleObject(m_showEvent, 300) != WAIT_OBJECT_0)
                        continue;
                    if (m_showWatcherStop.load())
                        break;
                    DispatcherQueue().TryEnqueue([this, hwnd]()
                        {
                            if (hwnd != nullptr)
                            {
                                ShowWindow(hwnd, SW_RESTORE);
                                SetForegroundWindow(hwnd);
                            }
                            Activate();
                        });
                }
            });
    }

    // ---------------------------------------------------------------------
    // 事件
    // ---------------------------------------------------------------------
    void MainWindow::OnNavSelectionChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& args)
    {
        auto item = args.SelectedItem().try_as<NavigationViewItem>();
        if (!item)
            return;

        auto tag = item.Tag();
        std::wstring tagText;
        if (tag)
            tagText = winrt::unbox_value<hstring>(tag).c_str();

        if (tagText == m_currentTag)
            return;

        m_currentTag = tagText;
        ContentScroll().ChangeView(nullptr, 0.0, nullptr, true);
        RebuildContent();
    }

    void MainWindow::OnRefreshClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
        RefreshState();
        RebuildContent();
    }

    void MainWindow::OnExitAppClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
        Command command;
        command.type = CommandType::kExitApp;
        SendCommand(command);
        Close();
    }

    // ---------------------------------------------------------------------
    // 数据
    // ---------------------------------------------------------------------
    bool MainWindow::RefreshState()
    {
        AppState state;
        bool ok = m_client && m_client->RequestState(state);
        if (ok)
            m_state = std::move(state);
        UpdateConnectionStatus();
        return ok;
    }

    void MainWindow::SendCommand(Command const& command)
    {
        AppState state;
        if (m_client && m_client->SendCommand(command, state))
        {
            m_state = std::move(state);
            StatusBar().IsOpen(false);
        }
        else
        {
            UpdateConnectionStatus();
        }
    }

    void MainWindow::UpdateConnectionStatus()
    {
        const bool connected = m_client && m_client->IsConnected();
        StatusBar().Severity(connected ? InfoBarSeverity::Success : InfoBarSeverity::Error);
        StatusBar().Title(connected ? L"已连接" : L"未连接到 MiauDesktop");
        StatusBar().Message(connected
            ? L""
            : L"请先启动 MiauDesktop 主程序，然后点击左下角\"刷新状态\"重试。");
        StatusBar().IsOpen(!connected);
    }

    // ---------------------------------------------------------------------
    // 页面构建
    // ---------------------------------------------------------------------
    void MainWindow::AddPageHeader(hstring const& title, hstring const& subtitle)
    {
        ContentHost().Children().Append(MakeText(title, L"PageTitleStyle"));
        ContentHost().Children().Append(MakeText(subtitle, L"PageSubtitleStyle"));
    }

    void MainWindow::AddGroupHeader(std::string const& group)
    {
        ContentHost().Children().Append(MakeText(ToH(group), L"GroupHeaderStyle"));
    }

    Border MainWindow::CreateSettingRow(SettingItem const& item, std::string const& componentName)
    {
        const bool isGlobal = componentName.empty();

        TextBlock title = MakeText(ToH(item.label), L"ItemTitleStyle");
        TextBlock description;
        TextBlock* descriptionPtr = nullptr;
        if (!item.description.empty())
        {
            description = MakeText(ToH(item.description), L"ItemDescriptionStyle");
            descriptionPtr = &description;
        }

        auto makeCommand = [item, componentName, isGlobal]()
            {
                Command command;
                command.type = isGlobal ? CommandType::kSetGlobal : CommandType::kSetSetting;
                command.name = componentName;
                command.key = item.key;
                return command;
            };

        switch (item.kind)
        {
        case ItemKind::Toggle:
        {
            ToggleSwitch toggle;
            toggle.MinWidth(0);
            toggle.OnContent(winrt::box_value(L""));
            toggle.OffContent(winrt::box_value(L""));
            toggle.IsOn(item.boolValue);
            toggle.Toggled([this, makeCommand](winrt::IInspectable const& sender, winrt::RoutedEventArgs const&)
                {
                    Command command = makeCommand();
                    command.valueKind = ValueKind::Bool;
                    command.boolValue = sender.as<ToggleSwitch>().IsOn();
                    SendCommand(command);
                });
            return BuildRowShell(title, descriptionPtr, toggle);
        }
        case ItemKind::Slider:
        {
            Slider slider;
            slider.Width(240);
            slider.Minimum(item.minValue);
            slider.Maximum(item.maxValue);
            slider.StepFrequency(item.step > 0.0 ? item.step : 0.01);
            slider.Value(item.numberValue);

            TextBlock valueText = MakeText(FormatNumber(item.numberValue, item.decimals, item.suffix), L"ItemTitleStyle");
            valueText.Width(72);
            valueText.TextAlignment(winrt::TextAlignment::Right);
            valueText.VerticalAlignment(winrt::VerticalAlignment::Center);
            valueText.Opacity(0.8);

            StackPanel panel;
            panel.Orientation(winrt::Orientation::Horizontal);
            panel.Spacing(8);
            panel.VerticalAlignment(winrt::VerticalAlignment::Center);
            panel.Children().Append(slider);
            panel.Children().Append(valueText);

            // 拖动过程中高频触发，做 180ms 防抖后再下发，避免刷屏写盘
            auto timer = DispatcherQueue().CreateTimer();
            timer.Interval(std::chrono::milliseconds(180));
            timer.IsRepeating(false);
            timer.Tick([this, makeCommand, slider](DispatcherQueueTimer const& sender, winrt::IInspectable const&)
                {
                    sender.Stop();
                    Command command = makeCommand();
                    command.valueKind = ValueKind::Number;
                    command.numberValue = slider.Value();
                    SendCommand(command);
                });
            slider.ValueChanged([timer, valueText, item](winrt::IInspectable const& sender, RangeBaseValueChangedEventArgs const& args)
                {
                    valueText.Text(FormatNumber(args.NewValue(), item.decimals, item.suffix));
                    timer.Stop();
                    timer.Start();
                });

            return BuildRowShell(title, descriptionPtr, panel);
        }
        case ItemKind::Choice:
        {
            ComboBox combo;
            combo.Width(240);
            for (auto const& option : item.options)
                combo.Items().Append(winrt::box_value(ToH(option)));
            if (item.selectedIndex >= 0 && item.selectedIndex < static_cast<int>(item.options.size()))
                combo.SelectedIndex(item.selectedIndex);
            combo.SelectionChanged([this, makeCommand](winrt::IInspectable const& sender, SelectionChangedEventArgs const&)
                {
                    const int index = sender.as<ComboBox>().SelectedIndex();
                    if (index < 0)
                        return;
                    Command command = makeCommand();
                    command.valueKind = ValueKind::Number;
                    command.numberValue = static_cast<double>(index);
                    SendCommand(command);
                });
            return BuildRowShell(title, descriptionPtr, combo);
        }
        case ItemKind::Action:
        default:
        {
            Button button;
            button.Content(winrt::box_value(L"执行"));
            button.Click([this, makeCommand](winrt::IInspectable const&, winrt::RoutedEventArgs const&)
                {
                    Command command = makeCommand();
                    command.valueKind = ValueKind::Bool;
                    command.boolValue = true;
                    SendCommand(command);
                });
            return BuildRowShell(title, descriptionPtr, button);
        }
        }
    }

    void MainWindow::RebuildContent()
    {
        ContentHost().Children().Clear();

        if (m_currentTag == L"components")
            BuildComponentsPage();
        else if (m_currentTag == L"global")
            BuildGlobalPage();
        else if (m_currentTag == L"layout")
            BuildLayoutPage();
        else
            BuildAboutPage();
    }

    void MainWindow::BuildComponentsPage()
    {
        AddPageHeader(L"组件", L"控制组件是否显示在桌面上；展开卡片可调整该组件的专属选项。");

        if (m_state.components.empty())
        {
            ContentHost().Children().Append(MakeText(
                L"没有可用的组件。请确认 MiauDesktop 正在运行，然后点击左下角\"刷新状态\"。",
                L"PageSubtitleStyle"));
            return;
        }

        for (auto const& component : m_state.components)
        {
            Expander expander;
            expander.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
            expander.HorizontalContentAlignment(winrt::HorizontalAlignment::Stretch);
            expander.Margin(winrt::Thickness{ 0, 0, 0, 8 });

            // ---- 头部：图标 + 名称/描述 + 启用开关 ----
            Grid header;
            header.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
            header.Margin(winrt::Thickness{ 0, 4, 8, 4 });

            ColumnDefinition headerLeft;
            headerLeft.Width(winrt::GridLength(1, winrt::GridUnitType::Star));
            ColumnDefinition headerRight;
            headerRight.Width(winrt::GridLength(0, winrt::GridUnitType::Auto));
            header.ColumnDefinitions().Append(headerLeft);
            header.ColumnDefinitions().Append(headerRight);

            StackPanel titlePanel;
            titlePanel.Orientation(winrt::Orientation::Horizontal);
            titlePanel.Spacing(12);
            titlePanel.VerticalAlignment(winrt::VerticalAlignment::Center);

            FontIcon icon;
            icon.FontFamily(FontFamily(L"Segoe Fluent Icons"));
            icon.FontSize(20);
            icon.VerticalAlignment(winrt::VerticalAlignment::Center);
            icon.Glyph(component.iconGlyph.empty() ? hstring(L"\uE8B9") : ToH(component.iconGlyph));

            StackPanel textPanel;
            textPanel.Spacing(2);
            textPanel.VerticalAlignment(winrt::VerticalAlignment::Center);

            TextBlock name;
            name.Text(ToH(component.name));
            name.FontSize(14);
            name.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            textPanel.Children().Append(name);

            if (!component.description.empty())
            {
                TextBlock description;
                description.Text(ToH(component.description));
                description.FontSize(12);
                description.Opacity(0.7);
                textPanel.Children().Append(description);
            }

            titlePanel.Children().Append(icon);
            titlePanel.Children().Append(textPanel);
            Grid::SetColumn(titlePanel, 0);
            header.Children().Append(titlePanel);

            ToggleSwitch visibilityToggle;
            visibilityToggle.MinWidth(0);
            visibilityToggle.OnContent(winrt::box_value(L""));
            visibilityToggle.OffContent(winrt::box_value(L""));
            visibilityToggle.VerticalAlignment(winrt::VerticalAlignment::Center);
            visibilityToggle.IsOn(component.visible);

            const std::string componentName = component.name;
            visibilityToggle.Toggled([this, componentName](winrt::IInspectable const& sender, winrt::RoutedEventArgs const&)
                {
                    Command command;
                    command.type = CommandType::kSetVisible;
                    command.name = componentName;
                    command.valueKind = ValueKind::Bool;
                    command.boolValue = sender.as<ToggleSwitch>().IsOn();
                    SendCommand(command);
                });
            Grid::SetColumn(visibilityToggle, 1);
            header.Children().Append(visibilityToggle);

            expander.Header(header);

            // ---- 内容：组件专属设置 ----
            StackPanel content;
            content.Spacing(0);
            content.Margin(winrt::Thickness{ 0, 4, 0, 8 });

            if (component.items.empty())
            {
                content.Children().Append(MakeText(L"该组件没有可调整的选项。", L"ItemDescriptionStyle", 0.7));
            }
            else
            {
                std::string lastGroup;
                bool first = true;
                for (auto const& item : component.items)
                {
                    if (first || item.group != lastGroup)
                    {
                        if (!item.group.empty())
                        {
                            TextBlock groupText;
                            groupText.Text(ToH(item.group));
                            groupText.FontSize(13);
                            groupText.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                            groupText.Opacity(0.8);
                            groupText.Margin(winrt::Thickness{ 0.0, first ? 0.0 : 12.0, 0.0, 6.0 });
                            content.Children().Append(groupText);
                        }
                        lastGroup = item.group;
                        first = false;
                    }
                    content.Children().Append(CreateSettingRow(item, componentName));
                }
            }

            expander.Content(content);
            ContentHost().Children().Append(expander);
        }
    }

    void MainWindow::BuildGlobalPage()
    {
        AddPageHeader(L"全局设置", L"作用于所有组件的通用行为设置。");

        if (m_state.globalItems.empty())
        {
            ContentHost().Children().Append(MakeText(
                L"暂时没有可用的全局设置项。请确认 MiauDesktop 正在运行。",
                L"PageSubtitleStyle"));
            return;
        }

        std::string lastGroup;
        bool first = true;
        for (auto const& item : m_state.globalItems)
        {
            if (first || item.group != lastGroup)
            {
                if (!item.group.empty())
                    AddGroupHeader(item.group);
                lastGroup = item.group;
                first = false;
            }
            ContentHost().Children().Append(CreateSettingRow(item, std::string{}));
        }
    }

    void MainWindow::BuildLayoutPage()
    {
        AddPageHeader(L"位置调整", L"在桌面上拖动组件、调整组件大小，并把布局恢复为默认位置。");

        if (m_state.editModeActive)
        {
            InfoBar activeBar;
            activeBar.Severity(InfoBarSeverity::Success);
            activeBar.Title(L"当前处于位置调整模式");
            activeBar.Message(L"可以在桌面上拖动或调整组件；完成后点击下方\"退出位置调整\"。");
            activeBar.IsOpen(true);
            activeBar.IsClosable(false);
            activeBar.Margin(winrt::Thickness{ 0, 0, 0, 12 });
            ContentHost().Children().Append(activeBar);
        }

        Border card;
        if (auto style = FindStyle(L"SettingsCardStyle"))
            card.Style(style);

        StackPanel panel;
        panel.Spacing(12);

        panel.Children().Append(MakeText(
            L"位置调整由 MiauDesktop 主程序负责绘制辅助线与吸附，本窗口可以保持打开。",
            L"ItemDescriptionStyle", 0.8));

        StackPanel buttons;
        buttons.Orientation(winrt::Orientation::Horizontal);
        buttons.Spacing(8);

        Button enterButton;
        enterButton.Content(winrt::box_value(m_state.editModeActive ? L"重新进入位置调整" : L"进入位置调整"));
        enterButton.Style([]() -> Style
            {
                auto resources = winrt::Application::Current().Resources();
                if (resources.HasKey(winrt::box_value(L"AccentButtonStyle")))
                    return resources.Lookup(winrt::box_value(L"AccentButtonStyle")).try_as<Style>();
                return nullptr;
            }());
        enterButton.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&)
            {
                Command command;
                command.type = CommandType::kEnterEditMode;
                SendCommand(command);
                RebuildContent();
            });
        buttons.Children().Append(enterButton);

        Button exitButton;
        exitButton.Content(winrt::box_value(L"退出位置调整"));
        exitButton.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&)
            {
                Command command;
                command.type = CommandType::kExitEditMode;
                SendCommand(command);
                RebuildContent();
            });
        buttons.Children().Append(exitButton);

        Button resetButton;
        resetButton.Content(winrt::box_value(L"恢复默认布局"));
        resetButton.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&)
            {
                Command command;
                command.type = CommandType::kResetLayout;
                SendCommand(command);
            });
        buttons.Children().Append(resetButton);

        panel.Children().Append(buttons);
        card.Child(panel);
        ContentHost().Children().Append(card);
    }

    void MainWindow::BuildAboutPage()
    {
        AddPageHeader(L"MiauDesktop 设置", L"基于 Windows App SDK / WinUI 3 的新版设置界面。");

        Border card;
        if (auto style = FindStyle(L"SettingsCardStyle"))
            card.Style(style);

        StackPanel panel;
        panel.Spacing(8);
        panel.Children().Append(MakeText(L"通信方式：命名管道 + JSON（本机进程间通信）", L"ItemTitleStyle"));
        panel.Children().Append(MakeText(
            L"设置程序与 MiauDesktop 主程序各自独立运行：主程序负责桌面组件渲染与位置调整，"
            L"设置程序只负责展示与下发设置，因此界面可以随时关闭而不影响主程序。",
            L"ItemDescriptionStyle", 0.8));
        panel.Children().Append(MakeText(
            L"协议版本：" + ToH(std::to_string(kProtocolVersion)),
            L"ItemDescriptionStyle", 0.8));

        card.Child(panel);
        ContentHost().Children().Append(card);
    }
}
