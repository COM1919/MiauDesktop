#pragma once
#include "../Framework.h"
#include "../Component.h"
#include "../GuiBase/GuiBase.h"

// GuiGallery：GuiBase 自定义控件的演示画廊
// 逐个展示本轮从 EUI-NEO 移植的控件（导航栏 / 抽屉 / 面板 / 布局调试 /
// 滚动视图 / 虚拟列表 / 瀑布流 / 文本 / 图片 / 鼠标区域 / workshop 组件），
// 便于运行 exe 直接验收视觉效果与交互。
class GuiGallery : public Component
{
public:
    GuiGallery() : Component("控件画廊", 0, true)
    {
        SetWindowSize(ImVec2(760.0f, 780.0f));
    }

    const char* GetIcon() const override { return ICON_FA_LAYER_GROUP; }
    const char* GetDescription() const override { return "GuiBase 全部自定义控件的演示画廊"; }

protected:
    void ComponentRender() override
    {
        // 记录窗口内容起点：抽屉需要从窗口顶部铺开，而不是从当前光标处
        const ImVec2 contentOrigin = ImGui::GetCursorScreenPos();
        const float fullWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x);

        // =========================================================================
        // 容器与布局
        // =========================================================================
        ImGui::TextDisabled("容器与布局（GuiNavbar / BeginGuiPanel / GuiLayoutDebugOverlay）");

        if (GuiBase::GuiButton("切换抽屉##gallery_toggle", ImVec2(120.0f, 32.0f)))
        {
            m_sidebarOpen = !m_sidebarOpen;
        }
        ImGui::SameLine();
        ImGui::Text("GuNavbar 选中项：value = %d", m_navIndex);

        // 导航栏（竖向）
        ImGui::BeginGroup();
        GuiBase::GuiNavbarOptions navOptions;
        navOptions.width = 190.0f;
        navOptions.height = 300.0f;
        navOptions.brandTitle = "Miau";
        navOptions.brandIcon = ICON_FA_LAYER_GROUP;
        navOptions.subtitle = "控件画廊";
        navOptions.footerText = "退出";
        navOptions.footerIcon = ICON_FA_XMARK;
        navOptions.footerClicked = &m_footerClicked;

        static const GuiBase::GuiNavbarItem navItems[] = {
            { "总览", ICON_FA_HOUSE, 1 },
            { "图库", ICON_FA_IMAGE, 2 },
            { "设置", ICON_FA_GEAR, 3 },
            { "调色", ICON_FA_PALETTE, 4 },
        };
        GuiBase::GuiNavbar("##gallery_nav", navItems, 4, &m_navIndex, navOptions);
        ImGui::EndGroup();
        ImGui::SameLine();

        ImGui::BeginGroup();
        // 面板容器
        if (GuiBase::BeginGuiPanel("##gallery_panel", ImVec2(320.0f, 92.0f)))
        {
            ImGui::Text("GuiPanel 内容区");
            ImGui::TextDisabled("Card 背景 + padding + 裁剪 + 软阴影");
        }
        GuiBase::EndGuiPanel();

        // 布局调试覆盖层
        GuiBase::GuiLayoutDebugOverlay("##gallery_layout_debug", ImVec2(320.0f, 70.0f), 10.0f, "inset 10");

        // 文本（左 / 右 / 居中，带换行）
        GuiBase::GuiText("##gallery_text_left", "GuiText：左对齐、超宽自动换行的一段说明文字。",
                         16.0f, GuiBase::GuiColor(0.0f, 0.0f, 0.0f, 0.0f),
                         GuiBase::GuiTextAlign::Left, 300.0f);
        GuiBase::GuiText("##gallery_text_center", "居中对齐 · subtitle", 20.0f,
                         GuiBase::GuiColor(0.0f, 0.0f, 0.0f, 0.0f),
                         GuiBase::GuiTextAlign::Center, 300.0f);
        GuiBase::GuiText("##gallery_text_right", "右对齐 · caption", 13.0f,
                         GuiBase::GuiColor(0.0f, 0.0f, 0.0f, 0.0f),
                         GuiBase::GuiTextAlign::Right, 300.0f);
        ImGui::EndGroup();

        // 图片占位 + 鼠标区域
        ImGui::BeginGroup();
        GuiBase::GuiImage("##gallery_image", nullptr, ImVec2(84.0f, 84.0f), GuiBase::GuiColor(), 12.0f);
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        GuiBase::GuiMouseAreaOptions mouseOptions;
        mouseOptions.rounding = 10.0f;
        mouseOptions.color = GuiBase::GuiColor(0.22f, 0.44f, 0.88f, 1.0f);
        mouseOptions.cursor = ImGuiMouseCursor_Hand;
        mouseOptions.tooltip = "GuiMouseArea 悬浮提示";
        if (GuiBase::GuiMouseArea("##gallery_mouse_area", ImVec2(200.0f, 40.0f), mouseOptions))
        {
            ++m_mouseClicks;
        }
        ImGui::Text("GuiMouseArea 点击次数：%d", m_mouseClicks);
        ImGui::TextDisabled("GuiImage：无纹理时显示占位图标");
        ImGui::EndGroup();

        // =========================================================================
        // 滚动与虚拟化
        // =========================================================================
        ImGui::TextDisabled("滚动与虚拟化（GuiScrollView / GuiVirtualList / GuiVirtualMasonry）");

        // 滚动视图：滚动条绘在右侧，内容由 BeginChild 裁剪
        ImGui::BeginGroup();
        if (GuiBase::BeginGuiScrollView("##gallery_scroll", ImVec2(320.0f, 140.0f), &m_scrollOffset))
        {
            for (int index = 0; index < 16; ++index)
            {
                ImGui::Text("滚动内容行 #%d", index);
            }
        }
        GuiBase::EndGuiScrollView();
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("offset = %.1f", m_scrollOffset);
        ImGui::TextDisabled("滚轮 / 拖拽滚动条");
        ImGui::EndGroup();

        // 虚拟列表：5000 行，只绘制可见区间
        const GuiBase::GuiVirtualRange listRange =
            GuiBase::GuiVirtualList("##gallery_list", ImVec2(fullWidth, 150.0f), 5000, 28.0f, &m_listOffset);
        for (int index = listRange.first; index <= listRange.last; ++index)
        {
            if (GuiBase::GuiVirtualListRow("##gallery_list", listRange, index, 28.0f, index == m_selectedRow))
            {
                m_selectedRow = index;
            }
        }
        ImGui::Text("虚拟列表：5000 行，可见 %d..%d，选中 %d", listRange.first, listRange.last, m_selectedRow);

        // 虚拟瀑布流：3 列，cell 高度按索引变化
        const std::function<float(int, float)> masonryHeight = [](int index, float width)
        {
            (void)width;
            return 40.0f + static_cast<float>(index % 4) * 16.0f;
        };
        const GuiBase::GuiMasonryRange masonryRange = GuiBase::GuiVirtualMasonry(
            "##gallery_masonry", ImVec2(fullWidth, 170.0f), 240, 3, 8.0f, &m_masonryOffset, masonryHeight);
        for (int index = masonryRange.first; index <= masonryRange.last; ++index)
        {
            if (GuiBase::GuiVirtualMasonryCell("##gallery_masonry", index, index == m_selectedCell))
            {
                m_selectedCell = index;
            }
        }
        ImGui::Text("瀑布流：240 项 / 3 列，可见 %d..%d，选中 %d",
                    masonryRange.first, masonryRange.last, m_selectedCell);

        // =========================================================================
        // workshop 组件
        // =========================================================================
        ImGui::TextDisabled("workshop（GuiHeartSwitch / GuiNeumorphicButton / GuiTiltCard / GuiCardSlider）");

        GuiBase::GuiHeartSwitch("##gallery_heart", &m_heartLiked, ImVec2(40.0f, 40.0f));
        ImGui::SameLine();
        GuiBase::GuiNeumorphicButton("Neumorphic", ImVec2(170.0f, 44.0f));
        ImGui::SameLine();
        ImGui::Text("心形开关：%s", m_heartLiked ? "已点赞" : "未点赞");

        GuiBase::GuiTiltCard("##gallery_tilt", ImVec2(240.0f, 150.0f), "Tilt Card", "移动鼠标查看倾斜");
        ImGui::SameLine();

        static const char* slideTitles[] = { "晨光", "午后", "黄昏", "夜色" };
        static const char* slideSubtitles[] = { "Morning", "Noon", "Dusk", "Night" };
        GuiBase::GuiCardSlider("##gallery_slider", nullptr, slideTitles, slideSubtitles, 4,
                               &m_slideIndex, ImVec2(std::max(300.0f, fullWidth - 250.0f), 240.0f));

        // =========================================================================
        // 参数级变体（EUI Builder 开关：图标/禁用、进制步进、多行输入、
        // 多级菜单、Toast 标题、Tooltip 绑定、图文轮播、卡片轮播参数、Markdown）
        // =========================================================================
        ImGui::TextDisabled("参数级变体（GuiButtonEx / GuiStepperEx / GuiInputTextEx / GuiContextMenuTree…）");

        GuiBase::GuiButtonEx("保存##variant_save", ImVec2(116.0f, 34.0f), ICON_FA_CIRCLE_CHECK, false);
        ImGui::SameLine();
        GuiBase::GuiButtonEx("禁用##variant_disabled", ImVec2(116.0f, 34.0f), ICON_FA_XMARK, true);
        ImGui::SameLine();
        GuiBase::GuiButtonEx(ICON_FA_GEAR, ImVec2(40.0f, 34.0f), ICON_FA_GEAR, false);

        // 十六进制步进器（base / bitWidth / 前缀 / 大写）
        GuiBase::GuiStepperOptions stepperOptions;
        stepperOptions.step = 16;
        stepperOptions.minValue = 0;
        stepperOptions.maxValue = 0xFFFF;
        stepperOptions.base = 16;
        stepperOptions.bitWidth = 16;
        stepperOptions.showBasePrefix = true;
        stepperOptions.uppercase = true;
        GuiBase::GuiStepperEx("##variant_stepper", &m_stepperValue, stepperOptions);
        ImGui::SameLine();
        ImGui::Text("步进器值 = %d", m_stepperValue);

        // 多行输入 + 占位符
        GuiBase::GuiInputOptions inputOptions;
        inputOptions.multiline = true;
        inputOptions.visibleLines = 3;
        inputOptions.placeholder = "多行输入，支持滚轮滚动（GuiInputTextEx）";
        GuiBase::GuiInputTextEx("##variant_input", &m_inputText, ImVec2(fullWidth, 0.0f), inputOptions);

        // 时间选择器：5 分钟步进
        GuiBase::GuiTimePicker("##variant_time", &m_hour, &m_minute, 5);
        ImGui::SameLine();
        ImGui::Text("%02d:%02d（5 分钟步进）", m_hour, m_minute);

        // 对话框变体（次级按钮 + 内容回调）
        if (GuiBase::GuiButton("打开对话框##variant_dialog", ImVec2(150.0f, 34.0f)))
        {
            m_dialogOpen = true;
        }
        ImGui::SameLine();
        if (GuiBase::GuiButton("弹出 Toast##variant_toast", ImVec2(150.0f, 34.0f)))
        {
            m_toastUntil = ImGui::GetTime() + 2.6;
        }
        ImGui::SameLine();
        ImGui::Text("多级菜单选中值：%d", m_menuValue);

        // 多级右键菜单（在下方按钮区域右键）
        GuiBase::GuiButton("在此按钮上右键##variant_ctx", ImVec2(200.0f, 34.0f));
        GuiBase::GuiTooltipFor("##variant_tip", "GuiTooltipFor：绑定到上一个控件");
        {
            static const GuiBase::GuiMenuItem exportItems[] = {
                { "导出 PNG", nullptr, 0, 101 },
                { "导出 SVG", nullptr, 0, 102 },
            };
            static const GuiBase::GuiMenuItem menuItems[] = {
                { "新建", nullptr, 0, 1 },
                { "导出", exportItems, 2, -1 },
                { "删除", nullptr, 0, 3 },
            };
            GuiBase::GuiContextMenuTree("##variant_menu", menuItems, 3, &m_menuValue);
        }

        // 图文轮播（堆叠参数）
        {
            static const GuiBase::GuiCarouselItem carouselItems[] = {
                { "晨光", "Morning", nullptr },
                { "午后", "Noon", nullptr },
                { "黄昏", "Dusk", nullptr },
            };
            GuiBase::GuiCarouselEx("##variant_carousel", carouselItems, 3, &m_carouselIndex,
                                   fullWidth, 200.0f);
        }

        // 文本层级
        GuiBase::GuiTextTitle("##variant_title", "GuiTextTitle 标题层级");
        GuiBase::GuiTextSubtitle("##variant_subtitle", "GuiTextSubtitle 副标题层级");
        GuiBase::GuiTextBody("##variant_body",
                             "GuiTextBody 正文层级：超宽自动换行，字号与颜色取自主题令牌。", fullWidth);

        // Markdown 文档控件（单一字体 + 动态字号）
        static const char* kMarkdownSample =
            "# 一级标题\n"
            "## 二级标题\n"
            "### 三级标题\n\n"
            "正文段落：单一字体文件 + ImGui 动态字号烘焙，不需要为每个字号加载字体。\n\n"
            "- 列表项一\n"
            "- 列表项二\n\n"
            "| 列 A | 列 B |\n"
            "| --- | --- |\n"
            "| 1 | 2 |\n\n"
            "```cpp\nint main() { return 0; }\n```\n";
        GuiBase::GuiMarkdown("##variant_markdown", kMarkdownSample, &m_markdownOffset,
                             ImVec2(fullWidth, 220.0f));

        // 卡片轮播变体（间距 / 倾斜 / 背景开关）
        {
            GuiBase::GuiCardSliderOptions sliderOptions;
            sliderOptions.cardSpacing = 8.0f;
            sliderOptions.tilt = true;
            sliderOptions.background = true;
            static const char* variantTitles[] = { "晨光", "午后", "黄昏", "夜色" };
            static const char* variantSubtitles[] = { "Morning", "Noon", "Dusk", "Night" };
            GuiBase::GuiCardSliderEx("##variant_slider", nullptr, variantTitles, variantSubtitles, 4,
                                     &m_slideIndex, ImVec2(fullWidth, 240.0f), sliderOptions);
        }

        // Toast 变体：按时间窗口显示
        if (ImGui::GetTime() < m_toastUntil)
        {
            GuiBase::GuiToastOptions toastOptions;
            toastOptions.title = "已保存";
            toastOptions.icon = ICON_FA_CIRCLE_CHECK;
            GuiBase::GuiToastEx("变体 Toast：带标题、图标与关闭按钮", 2.5f, toastOptions);
        }

        // 对话框变体：次级按钮 + 内容回调
        {
            GuiBase::GuiDialogOptions dialogOptions;
            dialogOptions.primaryText = "确认";
            dialogOptions.secondaryText = "取消";
            dialogOptions.content = []()
            {
                static const size_t kItemCount = 3;
                const char* items[kItemCount] = { "保留组件配置", "清理缓存", "重启设置服务" };
                for (size_t index = 0; index < kItemCount; ++index)
                {
                    ImGui::Text("- %s", items[index]);
                }
            };
            GuiBase::GuiDialogEx("变体对话框", &m_dialogOpen, "带次级按钮与自定义内容的对话框。",
                                 dialogOptions);
        }

        // =========================================================================
        // 抽屉（最后绘制，保证处于最上层）
        // =========================================================================
        const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(contentOrigin);

        GuiBase::GuiSidebarOptions sidebarOptions;
        sidebarOptions.width = fullWidth;
        sidebarOptions.height = ImGui::GetWindowSize().y;
        sidebarOptions.title = "控件抽屉";
        sidebarOptions.eyebrow = "GALLERY";
        GuiBase::GuiSidebar("##gallery_sidebar", &m_sidebarOpen, sidebarOptions, []()
        {
            ImGui::Text("这是 GuiSidebar 的内容区");
            ImGui::TextDisabled("点击左侧遮罩或右上角 X 关闭");
        });

        ImGui::SetCursorScreenPos(savedCursor);
    }

private:
    int m_navIndex = 0;
    bool m_footerClicked = false;
    bool m_sidebarOpen = false;
    float m_scrollOffset = 0.0f;
    float m_listOffset = 0.0f;
    float m_masonryOffset = 0.0f;
    int m_selectedRow = -1;
    int m_selectedCell = -1;
    int m_mouseClicks = 0;
    bool m_heartLiked = true;
    int m_slideIndex = 0;

    // 参数级变体演示状态
    std::string m_inputText;
    int m_stepperValue = 255;
    int m_hour = 9;
    int m_minute = 30;
    bool m_dialogOpen = false;
    int m_menuValue = -1;
    double m_toastUntil = 0.0;
    int m_carouselIndex = 0;
    float m_markdownOffset = 0.0f;
};
