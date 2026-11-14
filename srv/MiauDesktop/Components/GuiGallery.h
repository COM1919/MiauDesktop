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
};
