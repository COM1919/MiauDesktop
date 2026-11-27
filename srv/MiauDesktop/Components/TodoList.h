#pragma once
#include "../Framework.h"
#include "../Component.h"
#include "../GuiBase/GuiBase.h"
#include "../AppSettings/AppSettings.h"

#include <cfloat>
#include <cstdio>
#include <mutex>

// =============================================================================
// TodoList：待办清单组件
// 持久化：条目以 picojson 数组存进配置项 "Items"，每次修改即时落盘
// =============================================================================
class TodoList : public Component
{
public:
    bool m_showCompleted = true;   // 是否显示已完成项
    bool m_compact = false;        // 紧凑模式（行高与字号更小）
    int m_sortMode = 0;            // 0 = 手动拖拽，1 = 未完成优先

    TodoList() : Component("待办")
    {
        SetWindowSize(ImVec2(360.0f, 460.0f));
        windowFlags |= ImGuiWindowFlags_NoScrollbar;
        windowFlags |= ImGuiWindowFlags_NoScrollWithMouse;
        windowFlags |= ImGuiWindowFlags_NoTitleBar;
        windowFlags |= ImGuiWindowFlags_NoMove;

        // 注册组件特有配置项（自动挂在 "待办" 根节点下）
        config.AddItem("ShowCompleted", true);
        config.AddItem("CompactMode", false);
        config.AddItem("SortMode", 0);
        config.AddItem("Items", std::string("[]"));
    }

    const char* GetIcon() const override { return ICON_FA_LIST_CHECK; }
    const char* GetDescription() const override { return "组件内可直接添加、勾选、编辑与拖拽排序的待办清单"; }

    // 配置加载：基础配置已由基类处理，这里只读自己的特有配置项
    void OnLoadConfig() override
    {
        m_showCompleted = config.Get<bool>("ShowCompleted");
        m_compact = config.Get<bool>("CompactMode");
        m_sortMode = std::clamp(config.Get<int>("SortMode"), 0, 1);

        // 设置程序在服务线程里调用本函数：条目的重建交给渲染线程执行，
        // 这里只把配置里的 JSON 暂存下来（加锁，避免与渲染线程并发访问字符串）
        {
            std::lock_guard<std::mutex> lock(m_reloadMutex);
            m_pendingReloadJson = config.Get<std::string>("Items");
            m_reloadRequested = true;
        }

        // 配置变化会重建条目，同时退出编辑态并丢弃未提交的结构性修改
        m_editingId = 0;
        m_editFocusPending = false;
        m_pendingRemoveId = 0;
        m_pendingEditActive = false;
        m_pendingMoveFrom = -1;
        m_pendingMoveTo = -1;
    }

    void OnSaveConfig() override
    {
        config.Set("ShowCompleted", m_showCompleted);
        config.Set("CompactMode", m_compact);
        config.Set("SortMode", m_sortMode);
        config.Set("Items", SerializeItems());
    }

protected:
    void CollectSettings(std::vector<Miau::Settings::SettingItem>& items) override
    {
        items.push_back(MakeToggle("ShowCompleted", "显示已完成", "显示", m_showCompleted,
                                   "关闭后列表只保留未完成项，已完成数量仍计入底部统计"));
        items.push_back(MakeToggle("CompactMode", "紧凑模式", "显示", m_compact,
                                   "减小行高与字号，适合条目较多时"));
        items.push_back(MakeChoice("SortMode", "排序方式", "排序",
                                   { "手动拖拽", "未完成优先" }, m_sortMode,
                                   "未完成优先会临时把已完成项沉底，不改变手动拖拽的顺序"));
    }

    void AfterBegin() override
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPaddingX, kPaddingY));
    }

    void ComponentRender() override
    {
        ImGui::PopStyleVar();   // AfterBegin 的 WindowPadding

        // 应用设置线程暂存的配置条目（重建只发生在渲染线程）
        {
            std::string reloadJson;
            bool reload = false;
            {
                std::lock_guard<std::mutex> lock(m_reloadMutex);
                if (m_reloadRequested)
                {
                    reload = true;
                    m_reloadRequested = false;
                    reloadJson = std::move(m_pendingReloadJson);
                    m_pendingReloadJson.clear();
                }
            }
            if (reload)
                DeserializeItems(reloadJson);
        }

        const GuiBase::GuiThemeTokens tokens = GuiBase::Theme::Dark();
        const float rowHeight = m_compact ? 32.0f : 40.0f;
        const float inputHeight = tokens.control.field;   // 35
        const float footerHeight = 22.0f;

        // ---- 输入行：组件内唯一的主操作 ----
        const ImVec2 inputRowPos = ImGui::GetCursorScreenPos();
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const float addSize = inputHeight;
        const float inputWidth = std::max(80.0f, rowWidth - addSize - tokens.spacing.compact);

        GuiBase::GuiInputOptions inputOptions;
        inputOptions.placeholder = "添加待办，回车确认";
        inputOptions.onEnter = [this]() { AddFromInput(); };
        GuiBase::GuiInputTextEx("##todo_input", &m_inputBuffer, ImVec2(inputWidth, inputHeight), inputOptions);

        // 加号按钮与输入框外框顶端对齐（GuiInputTextEx 内部 item 比外框矮）
        ImGui::SetCursorScreenPos(ImVec2(inputRowPos.x + inputWidth + tokens.spacing.compact, inputRowPos.y));
        DrawAddButton(tokens, addSize);

        // 列表从输入行下方开始
        ImGui::SetCursorScreenPos(ImVec2(inputRowPos.x, inputRowPos.y + inputHeight + tokens.spacing.content));

        // ---- 列表：剩余高度全部给它（原生 child 滚动，滚轮/滚动条直接可用）----
        const float listHeight = ImGui::GetContentRegionAvail().y - footerHeight - tokens.spacing.section;
        if (listHeight > 24.0f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            if (ImGui::BeginChild("##todo_list", ImVec2(ImGui::GetContentRegionAvail().x, listHeight),
                                  ImGuiChildFlags_None))
            {
                DrawList(tokens, rowHeight);

                // 新增条目后滚到底部（SetScrollHereY 在下一帧生效）
                if (m_scrollToBottom)
                {
                    ImGui::SetScrollHereY(1.0f);
                    m_scrollToBottom = false;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        // ---- 底部栏：有内容时才出现（空状态不重复说“没有待办”）----
        if (!m_items.empty())
            DrawFooter(tokens, footerHeight);

        // ---- 应用本帧记录的结构性修改（不在遍历容器时直接改它）----
        ApplyPendingChanges();
    }

private:
    // =========================================================================
    // 数据模型
    // =========================================================================
    struct TodoItem
    {
        int id = 0;             // 运行时稳定 ID（ImGui ID / 拖拽 / 延迟修改都用它）
        std::string text;
        bool done = false;
    };

    std::vector<TodoItem> m_items;
    int m_nextId = 1;

    // 线程安全：设置程序在服务线程里触发 ApplySetting / SaveConfigNow，
    // 因此条目的重建推迟到渲染线程，容器读写用锁保护
    mutable std::mutex m_itemsMutex;
    std::mutex m_reloadMutex;
    std::string m_pendingReloadJson;
    bool m_reloadRequested = false;

    std::string m_inputBuffer;          // 顶部输入框
    std::string m_editBuffer;           // 行内编辑缓冲
    int m_editingId = 0;                // 正在编辑的条目 ID（0 = 无）
    bool m_editFocusPending = false;    // 下一帧把焦点交给编辑框
    bool m_scrollToBottom = false;      // 新增条目后滚到底部
    float m_rowDragSqr = 0.0f;          // 当前行的最大拖动距离（区分点击与拖拽）

    int m_pendingRemoveId = 0;          // 待删除条目
    int m_pendingEditId = 0;            // 待提交的编辑
    bool m_pendingEditActive = false;
    std::string m_pendingEditText;
    int m_pendingMoveFrom = -1;         // 待应用的拖拽 / 上移 / 下移
    int m_pendingMoveTo = -1;

    bool m_clearArmed = false;          // “清空已完成”二次确认状态
    float m_clearArmedUntil = 0.0f;

    static constexpr float kPaddingX = 16.0f;
    static constexpr float kPaddingY = 14.0f;

    // =========================================================================
    // 列表
    // =========================================================================
    void DrawList(const GuiBase::GuiThemeTokens& tokens, float rowHeight)
    {
        const std::vector<int> order = BuildVisibleOrder();
        if (order.empty())
        {
            DrawEmptyState(tokens, !m_items.empty());
            return;
        }

        const float rowWidth = ImGui::GetContentRegionAvail().x;
        for (size_t i = 0; i < order.size(); ++i)
            DrawRow(tokens, order[i], static_cast<int>(i), rowWidth, rowHeight);
    }

    // 可见条目顺序：先按“显示已完成”过滤，再按排序方式稳定分区
    std::vector<int> BuildVisibleOrder() const
    {
        std::vector<int> order;
        order.reserve(m_items.size());
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        {
            if (!m_showCompleted && m_items[i].done)
                continue;
            order.push_back(i);
        }

        if (m_sortMode == 1)
        {
            std::stable_partition(order.begin(), order.end(),
                                  [this](int index) { return !m_items[index].done; });
        }
        return order;
    }

    void DrawRow(const GuiBase::GuiThemeTokens& tokens, int itemIndex,
                 int displayIndex, float rowWidth, float rowHeight)
    {
        TodoItem& item = m_items[itemIndex];
        ImGui::PushID(item.id);

        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
        const ImVec2 rowMax(rowPos.x + rowWidth, rowPos.y + rowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // ---- 编辑态：整行替换为输入框 ----
        if (m_editingId == item.id)
        {
            DrawEditRow(tokens, item, rowPos, rowWidth, rowHeight);
            ImGui::PopID();
            return;
        }

        // ---- 命中：整行一个 item，按鼠标横向位置分区 ----
        const bool pressed = ImGui::InvisibleButton("##row", ImVec2(rowWidth, rowHeight));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (active)
            m_rowDragSqr = ImGui::GetIO().MouseDragMaxDistanceSqr[0];

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool onToggle = hovered && mouse.x < rowPos.x + 34.0f;
        const bool onDelete = hovered && mouse.x > rowMax.x - 30.0f;

        // ---- 行背景：用底色区分，不画分割线 ----
        if (hovered || active)
        {
            drawList->AddRectFilled(rowPos, rowMax,
                                    GuiBase::WithAlpha(tokens.surfaceHover, 0.55f).ToImU32(),
                                    tokens.radius.small);
        }

        // ---- 勾选圆点 ----
        DrawToggle(tokens, ImVec2(rowPos.x + 19.0f, rowPos.y + rowHeight * 0.5f), item.done, onToggle);

        // ---- 文本（超宽省略，悬停看全文）----
        ImFont* font = ImGui::GetFont();
        const float fontSize = m_compact ? tokens.typography.label : tokens.typography.body;
        const float textLeft = rowPos.x + 40.0f;
        const float textMaxWidth = std::max(40.0f, rowWidth - 40.0f - 28.0f);
        const std::string shown = ElideText(font, fontSize, item.text, textMaxWidth);
        const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, shown.c_str());
        const float textY = rowPos.y + (rowHeight - textSize.y) * 0.5f;

        const GuiBase::GuiColor textColor = item.done
                                                ? GuiBase::WithAlpha(tokens.text, 0.38f)
                                                : GuiBase::WithAlpha(tokens.text, 0.94f);
        drawList->AddText(font, fontSize, ImVec2(textLeft, textY), textColor.ToImU32(), shown.c_str());
        if (item.done)
        {
            const float lineY = textY + textSize.y * 0.55f;
            drawList->AddLine(ImVec2(textLeft, lineY),
                              ImVec2(textLeft + textSize.x, lineY),
                              textColor.ToImU32(), 1.0f);
        }

        if (hovered && shown != item.text)
            ImGui::SetTooltip("%s", item.text.c_str());

        // ---- 删除按钮：悬停行时才出现 ----
        if (hovered)
        {
            const GuiBase::GuiColor iconColor = onDelete
                                                    ? GuiBase::GuiColor(0.96f, 0.52f, 0.42f, 0.95f)
                                                    : GuiBase::WithAlpha(tokens.text, 0.45f);
            const float iconSize = 13.0f;
            const ImVec2 iconSize2 = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, ICON_FA_XMARK);
            drawList->AddText(font, iconSize,
                              ImVec2(rowMax.x - 16.0f - iconSize2.x * 0.5f,
                                     rowPos.y + (rowHeight - iconSize2.y) * 0.5f),
                              iconColor.ToImU32(), ICON_FA_XMARK);
        }

        // ---- 点击：释放时提交；拖动过的不算点击 ----
        if (pressed && m_rowDragSqr < 36.0f)
        {
            if (onDelete)
                m_pendingRemoveId = item.id;
            else if (onToggle)
                ToggleDone(itemIndex);
            else
                BeginEdit(item);
        }

        // ---- 拖拽排序（仅手动模式；未完成优先下顺序由排序规则决定）----
        if (m_sortMode == 0)
        {
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("TODO_ITEM", &itemIndex, sizeof(int));
                ImGui::TextUnformatted(ElideText(font, fontSize, item.text, 240.0f).c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TODO_ITEM"))
                {
                    const int from = *static_cast<const int*>(payload->Data);
                    if (from != itemIndex)
                    {
                        m_pendingMoveFrom = from;
                        m_pendingMoveTo = itemIndex;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        // ---- 右键菜单：不常用操作收进来，不占行内空间 ----
        if (ImGui::BeginPopupContextItem("##context"))
        {
            if (ImGui::MenuItem(item.done ? "标记为未完成" : "标记为完成"))
                ToggleDone(itemIndex);
            if (ImGui::MenuItem("编辑"))
                BeginEdit(item);
            if (m_sortMode == 0)
            {
                const bool canMoveUp = itemIndex > 0;
                const bool canMoveDown = itemIndex + 1 < static_cast<int>(m_items.size());
                if (ImGui::MenuItem("上移", nullptr, false, canMoveUp))
                {
                    m_pendingMoveFrom = itemIndex;
                    m_pendingMoveTo = itemIndex - 1;
                }
                if (ImGui::MenuItem("下移", nullptr, false, canMoveDown))
                {
                    m_pendingMoveFrom = itemIndex;
                    m_pendingMoveTo = itemIndex + 1;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("删除"))
                m_pendingRemoveId = item.id;
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    void DrawEditRow(const GuiBase::GuiThemeTokens& tokens, TodoItem& item,
                     const ImVec2& rowPos, float rowWidth, float rowHeight)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 rowMax(rowPos.x + rowWidth, rowPos.y + rowHeight);

        // 用输入框底色提示当前处于编辑态
        drawList->AddRectFilled(rowPos, rowMax,
                                GuiBase::WithAlpha(tokens.surfaceHover, 0.65f).ToImU32(),
                                tokens.radius.small);

        const float editLeft = rowPos.x + 36.0f;
        const float editWidth = std::max(60.0f, rowWidth - 36.0f - 8.0f);
        ImGui::SetCursorScreenPos(ImVec2(editLeft, rowPos.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f));
        ImGui::SetNextItemWidth(editWidth);

        if (m_editFocusPending)
        {
            ImGui::SetKeyboardFocusHere();
            m_editFocusPending = false;
        }

        const bool submitted = ImGui::InputText("##edit", &m_editBuffer,
                                                ImGuiInputTextFlags_EnterReturnsTrue |
                                                    ImGuiInputTextFlags_AutoSelectAll);
        const bool deactivated = ImGui::IsItemDeactivated();
        const bool cancelled = deactivated && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (submitted)
        {
            m_pendingEditId = item.id;
            m_pendingEditText = m_editBuffer;
            m_pendingEditActive = true;
            m_editingId = 0;
        }
        else if (deactivated)
        {
            if (cancelled)
            {
                m_editingId = 0;   // Esc：放弃修改
            }
            else
            {
                m_pendingEditId = item.id;   // 失焦：按提交处理，避免白编辑
                m_pendingEditText = m_editBuffer;
                m_pendingEditActive = true;
                m_editingId = 0;
            }
        }
    }

    // 空状态：图标 + 主文案 + 一句引导（功能的一部分，不是补丁）
    void DrawEmptyState(const GuiBase::GuiThemeTokens& tokens, bool allCompleted)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();

        const float centerX = start.x + avail.x * 0.5f;
        const float centerY = start.y + std::max(avail.y, 120.0f) * 0.40f;

        const float iconSize = 30.0f;
        const ImVec2 iconSize2 = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, ICON_FA_CLIPBOARD_LIST);
        drawList->AddText(font, iconSize,
                          ImVec2(centerX - iconSize2.x * 0.5f, centerY - iconSize2.y),
                          GuiBase::WithAlpha(tokens.text, 0.20f).ToImU32(), ICON_FA_CLIPBOARD_LIST);

        const char* title = allCompleted ? "没有未完成的待办" : "还没有待办";
        const char* hint = allCompleted ? "已完成项已隐藏，可在设置中打开「显示已完成」"
                                        : "在上方输入内容，按回车添加";

        const float titleSize = tokens.typography.subtitle;
        const float hintSize = tokens.typography.caption;

        const ImVec2 titleSize2 = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title);
        drawList->AddText(font, titleSize,
                          ImVec2(centerX - titleSize2.x * 0.5f, centerY + 10.0f),
                          GuiBase::WithAlpha(tokens.text, 0.78f).ToImU32(), title);

        const ImVec2 hintSize2 = font->CalcTextSizeA(hintSize, FLT_MAX, 0.0f, hint);
        drawList->AddText(font, hintSize,
                          ImVec2(centerX - hintSize2.x * 0.5f, centerY + 12.0f + titleSize2.y),
                          GuiBase::WithAlpha(tokens.text, 0.45f).ToImU32(), hint);
    }

    // =========================================================================
    // 组件内的控件
    // =========================================================================
    void DrawAddButton(const GuiBase::GuiThemeTokens& tokens, float size)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##todo_add", ImVec2(size, size));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        GuiBase::GuiColor bg = tokens.primary;
        if (active)
            bg = GuiBase::MixColor(tokens.primary, GuiBase::GuiColor(0.0f, 0.0f, 0.0f, 1.0f), 0.12f);
        else if (hovered)
            bg = GuiBase::MixColor(tokens.primary, GuiBase::GuiColor(1.0f, 1.0f, 1.0f, 1.0f), 0.08f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size),
                                bg.ToImU32(), tokens.radius.control);

        ImFont* font = ImGui::GetFont();
        const float iconSize = 17.0f;
        const ImVec2 iconSize2 = font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, ICON_FA_PLUS);
        drawList->AddText(font, iconSize,
                          ImVec2(pos.x + (size - iconSize2.x) * 0.5f,
                                 pos.y + (size - iconSize2.y) * 0.5f),
                          IM_COL32(255, 255, 255, 245), ICON_FA_PLUS);

        if (pressed)
            AddFromInput();
    }

    void DrawToggle(const GuiBase::GuiThemeTokens& tokens, const ImVec2& center, bool done, bool highlighted)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float radius = 8.5f;

        if (done)
        {
            drawList->AddCircleFilled(center, radius, tokens.primary.ToImU32(), 24);

            const ImU32 checkColor = IM_COL32(255, 255, 255, 240);
            drawList->AddLine(ImVec2(center.x - 3.6f, center.y + 0.2f),
                              ImVec2(center.x - 1.0f, center.y + 2.8f), checkColor, 1.8f);
            drawList->AddLine(ImVec2(center.x - 1.0f, center.y + 2.8f),
                              ImVec2(center.x + 3.8f, center.y - 3.0f), checkColor, 1.8f);
        }
        else
        {
            const GuiBase::GuiColor ring = highlighted
                                               ? GuiBase::WithAlpha(tokens.primary, 0.95f)
                                               : GuiBase::WithAlpha(tokens.text, 0.34f);
            drawList->AddCircle(center, radius, ring.ToImU32(), 24, highlighted ? 2.0f : 1.6f);
        }
    }

    void DrawFooter(const GuiBase::GuiThemeTokens& tokens, float footerHeight)
    {
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 winSize = ImGui::GetWindowSize();
        const float left = winPos.x + kPaddingX;
        const float right = winPos.x + winSize.x - kPaddingX;
        const float top = winPos.y + winSize.y - kPaddingY - footerHeight;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float fontSize = tokens.typography.caption;

        int remaining = 0;
        int completed = 0;
        for (const TodoItem& item : m_items)
            (item.done ? completed : remaining)++;

        char countText[64];
        if (completed > 0)
            std::snprintf(countText, sizeof(countText), "剩余 %d 项 · 已完成 %d 项", remaining, completed);
        else
            std::snprintf(countText, sizeof(countText), "剩余 %d 项", remaining);

        const ImVec2 countSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, countText);
        drawList->AddText(font, fontSize,
                          ImVec2(left, top + (footerHeight - countSize.y) * 0.5f),
                          GuiBase::WithAlpha(tokens.text, 0.55f).ToImU32(), countText);

        // 「清空已完成」只在有完成项时出现
        if (completed <= 0)
            return;

        if (m_clearArmed && ImGui::GetTime() > m_clearArmedUntil)
            m_clearArmed = false;

        const char* clearText = m_clearArmed ? "确认清空？" : "清空已完成";
        const ImVec2 clearSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, clearText);
        const ImVec2 clearPos(right - clearSize.x, top + (footerHeight - clearSize.y) * 0.5f);

        ImGui::SetCursorScreenPos(ImVec2(clearPos.x - 6.0f, top));
        ImGui::InvisibleButton("##todo_clear", ImVec2(clearSize.x + 12.0f, footerHeight));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
        {
            if (m_clearArmed)
            {
                m_clearArmed = false;
                ClearCompleted();
            }
            else
            {
                m_clearArmed = true;
                m_clearArmedUntil = static_cast<float>(ImGui::GetTime()) + 3.0f;
            }
        }

        const GuiBase::GuiColor clearColor = m_clearArmed
                                                 ? GuiBase::GuiColor(0.96f, 0.52f, 0.42f, 0.95f)
                                                 : (hovered ? GuiBase::WithAlpha(tokens.text, 0.78f)
                                                            : GuiBase::WithAlpha(tokens.text, 0.45f));
        drawList->AddText(font, fontSize, clearPos, clearColor.ToImU32(), clearText);
    }

    // =========================================================================
    // 交互动作
    // =========================================================================
    void AddFromInput()
    {
        const std::string text = Trim(m_inputBuffer);
        if (text.empty())
            return;

        {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            TodoItem item;
            item.id = m_nextId++;
            item.text = text;
            item.done = false;
            m_items.push_back(std::move(item));
        }

        m_inputBuffer.clear();
        m_scrollToBottom = true;
        SaveItems();
    }

    void ToggleDone(int itemIndex)
    {
        {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            if (itemIndex < 0 || itemIndex >= static_cast<int>(m_items.size()))
                return;
            m_items[itemIndex].done = !m_items[itemIndex].done;
        }
        SaveItems();
    }

    void BeginEdit(TodoItem& item)
    {
        // 另点一行时先把上一行未提交的编辑按提交处理，避免静默丢内容
        //（延迟到帧末应用，避免在列表遍历中修改容器导致引用失效）
        if (m_editingId != 0 && m_editingId != item.id)
        {
            m_pendingEditId = m_editingId;
            m_pendingEditText = m_editBuffer;
            m_pendingEditActive = true;
        }

        m_editingId = item.id;
        m_editBuffer = item.text;
        m_editFocusPending = true;
    }

    void ClearCompleted()
    {
        {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            std::erase_if(m_items, [](const TodoItem& item) { return item.done; });
        }
        SaveItems();
    }

    // 把遍历中记录的修改统一应用：删除 → 编辑 → 移动（三者不会同时发生）
    void ApplyPendingChanges()
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_itemsMutex);

            if (m_pendingRemoveId != 0)
            {
                const int id = m_pendingRemoveId;
                m_pendingRemoveId = 0;
                std::erase_if(m_items, [id](const TodoItem& item) { return item.id == id; });
                changed = true;
            }

            if (m_pendingEditActive)
            {
                const int id = m_pendingEditId;
                const std::string text = Trim(m_pendingEditText);
                m_pendingEditActive = false;
                m_pendingEditId = 0;

                auto it = std::find_if(m_items.begin(), m_items.end(),
                                       [id](const TodoItem& item) { return item.id == id; });
                if (it != m_items.end())
                {
                    if (text.empty())
                        m_items.erase(it);   // 清空内容 = 删除（符合行内编辑的直觉）
                    else
                        it->text = text;
                    changed = true;
                }
            }

            if (m_pendingMoveFrom >= 0 && m_pendingMoveTo >= 0)
            {
                const int from = m_pendingMoveFrom;
                const int to = m_pendingMoveTo;
                m_pendingMoveFrom = -1;
                m_pendingMoveTo = -1;

                if (from >= 0 && from < static_cast<int>(m_items.size()) &&
                    to >= 0 && to < static_cast<int>(m_items.size()) && from != to)
                {
                    TodoItem moved = std::move(m_items[from]);
                    m_items.erase(m_items.begin() + from);

                    // 目标索引取“被拖到的那一行的原位置”：移除源行后若源在目标之前，索引前移一位
                    int target = (from < to) ? to - 1 : to;
                    target = std::clamp(target, 0, static_cast<int>(m_items.size()));
                    m_items.insert(m_items.begin() + target, std::move(moved));
                    changed = true;
                }
            }
        }

        if (changed)
            SaveItems();
    }

    // =========================================================================
    // 工具
    // =========================================================================
    static std::string Trim(const std::string& text)
    {
        const size_t begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
            return {};
        const size_t end = text.find_last_not_of(" \t\r\n");
        return text.substr(begin, end - begin + 1);
    }

    // 按 UTF-8 码点累加宽度，超宽时截断并追加省略号（不会切断多字节字符）
    static std::string ElideText(ImFont* font, float fontSize, const std::string& text, float maxWidth)
    {
        if (font == nullptr || maxWidth <= 0.0f)
            return {};

        if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()).x <= maxWidth)
            return text;

        static const char* kEllipsis = "…";
        const float ellipsisWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, kEllipsis).x;
        const float limit = maxWidth - ellipsisWidth;

        std::string out;
        float width = 0.0f;
        for (size_t i = 0; i < text.size();)
        {
            const unsigned char lead = static_cast<unsigned char>(text[i]);
            size_t length = 1;
            if ((lead & 0xE0) == 0xC0)
                length = 2;
            else if ((lead & 0xF0) == 0xE0)
                length = 3;
            else if ((lead & 0xF8) == 0xF0)
                length = 4;
            if (i + length > text.size())
                break;

            const std::string glyph = text.substr(i, length);
            const float glyphWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, glyph.c_str()).x;
            if (width + glyphWidth > limit)
                break;

            out += glyph;
            width += glyphWidth;
            i += length;
        }

        out += kEllipsis;
        return out;
    }

    // =========================================================================
    // 持久化（picojson）
    // =========================================================================
    std::string SerializeItems() const
    {
        std::lock_guard<std::mutex> lock(m_itemsMutex);

        picojson::array array;
        array.reserve(m_items.size());
        for (const TodoItem& item : m_items)
        {
            picojson::object obj;
            obj["text"] = picojson::value(item.text);
            obj["done"] = picojson::value(item.done);
            array.push_back(picojson::value(obj));
        }
        return picojson::value(array).serialize();
    }

    void DeserializeItems(const std::string& json)
    {
        std::lock_guard<std::mutex> lock(m_itemsMutex);

        m_items.clear();
        m_nextId = 1;

        picojson::value root;
        if (!picojson::parse(root, json).empty() || !root.is<picojson::array>())
            return;

        for (const picojson::value& entry : root.get<picojson::array>())
        {
            if (!entry.is<picojson::object>())
                continue;

            TodoItem item;
            item.id = m_nextId++;
            if (entry.contains("text") && entry.get("text").is<std::string>())
                item.text = entry.get("text").get<std::string>();
            if (entry.contains("done") && entry.get("done").is<bool>())
                item.done = entry.get("done").get<bool>();
            if (!item.text.empty())
                m_items.push_back(std::move(item));
        }
    }

    void SaveItems()
    {
        config.Set("Items", SerializeItems());
        AppSettings::SaveConfigNow();   // 每次修改即时落盘，避免异常退出丢数据
    }
};
