#pragma once
// =============================================================================
// SettingsProtocol.h
// MiauDesktop 主程序 <-> MiauSettings（WinUI3 设置程序）之间的通信协议
//
// 设计原则：
//   - 双方只共享"数据结构 + 序列化 + 管道收发"这三件事，不含任何业务逻辑；
//   - 传输格式：命名管道 + 4 字节小端长度前缀 + UTF-8 JSON 负载；
//   - 设置项采用数据驱动模型（SettingItem），主程序负责"收集/应用"，
//     设置程序只负责"渲染"，新增设置项无需改动设置程序。
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

#include "../MiauDesktop/PicoJson/picojson.h"

namespace Miau
{
namespace Settings
{
    // 管道名（主程序单实例运行，故用固定名字）
    constexpr const wchar_t* kPipeName = L"\\\\.\\pipe\\MiauDesktop.Settings";
    // 协议版本：双方不一致时设置程序提示需要更新
    constexpr int kProtocolVersion = 1;
    // 单条消息最大长度（防御性上限，8 MB）
    constexpr uint32_t kMaxMessageSize = 8u * 1024u * 1024u;

    // 设置项控件类型
    enum class ItemKind
    {
        Toggle,   // 开关
        Slider,   // 滑块（数值）
        Choice,   // 下拉选择
        Action,   // 按钮（无值，仅触发命令）
    };

    // 设置项取值类型
    enum class ValueKind
    {
        Bool,
        Number,
        String,
    };

    // 单个设置项
    struct SettingItem
    {
        std::string key;                    // 唯一键（组件内唯一 / 全局设置全局唯一）
        std::string label;                  // 显示名称
        std::string group;                  // 所属分组标题（同组连续排列）
        std::string description;            // 说明文字（可为空）

        ItemKind kind = ItemKind::Toggle;
        ValueKind valueKind = ValueKind::Bool;

        // 当前值（按 valueKind 取用其一）
        bool boolValue = false;
        double numberValue = 0.0;
        std::string stringValue;

        // 数值型参数（Slider）
        double minValue = 0.0;
        double maxValue = 1.0;
        double step = 0.01;
        int decimals = 0;                   // 显示小数位
        std::string suffix;                 // 数值后缀（如 "x"、" ms"）

        // 枚举型参数（Choice）：options[selectedIndex] 为当前值
        std::vector<std::string> options;
        int selectedIndex = 0;
    };

    // 单个组件状态
    struct ComponentState
    {
        std::string name;                   // 组件名（唯一标识）
        std::string description;            // 组件描述
        std::string iconGlyph;              // Segoe Fluent Icons 字形（UTF-8 单字符）
        bool visible = true;                // 是否启用（显示）
        std::vector<SettingItem> items;     // 组件专属设置项
    };

    // 完整状态快照
    struct AppState
    {
        bool editModeActive = false;        // 是否处于"位置调整"模式
        std::vector<ComponentState> components;
        std::vector<SettingItem> globalItems; // 全局设置项（key 如 "Animations.Enabled"）
    };

    // 客户端 -> 服务端命令
    struct Command
    {
        std::string type;                   // 见下方 SendCommand 支持的取值
        std::string name;                   // 组件名（组件相关命令）
        std::string key;                    // 设置项键

        ValueKind valueKind = ValueKind::Bool;
        bool boolValue = false;
        double numberValue = 0.0;
        std::string stringValue;
    };

    // 命令类型常量
    namespace CommandType
    {
        inline constexpr const char* kHello = "hello";                  // 请求状态
        inline constexpr const char* kRefresh = "refresh";              // 重新请求状态
        inline constexpr const char* kSetVisible = "set_visible";       // name + bool
        inline constexpr const char* kSetSetting = "set_setting";       // name + key + value
        inline constexpr const char* kSetGlobal = "set_global";         // key + value
        inline constexpr const char* kResetComponent = "reset_component"; // name
        inline constexpr const char* kResetLayout = "reset_layout";     // 恢复默认位置
        inline constexpr const char* kEnterEditMode = "enter_edit_mode";
        inline constexpr const char* kExitEditMode = "exit_edit_mode";
        inline constexpr const char* kExitApp = "exit_app";             // 退出主程序
    }

    // -------------------------------------------------------------------------
    // JSON 序列化
    // -------------------------------------------------------------------------
    namespace Detail
    {
        inline const char* ToString(ItemKind kind)
        {
            switch (kind)
            {
            case ItemKind::Toggle: return "toggle";
            case ItemKind::Slider: return "slider";
            case ItemKind::Choice: return "choice";
            case ItemKind::Action: return "action";
            }
            return "toggle";
        }

        inline ItemKind ItemKindFromString(const std::string& text)
        {
            if (text == "slider") return ItemKind::Slider;
            if (text == "choice") return ItemKind::Choice;
            if (text == "action") return ItemKind::Action;
            return ItemKind::Toggle;
        }

        inline const char* ToString(ValueKind kind)
        {
            switch (kind)
            {
            case ValueKind::Bool: return "bool";
            case ValueKind::Number: return "number";
            case ValueKind::String: return "string";
            }
            return "bool";
        }

        inline ValueKind ValueKindFromString(const std::string& text)
        {
            if (text == "number") return ValueKind::Number;
            if (text == "string") return ValueKind::String;
            return ValueKind::Bool;
        }

        inline std::string GetString(const picojson::value& json, const char* key)
        {
            if (!json.contains(key)) return {};
            const picojson::value& v = json.get(key);
            return v.is<std::string>() ? v.get<std::string>() : std::string{};
        }

        inline bool GetBool(const picojson::value& json, const char* key, bool fallback = false)
        {
            if (!json.contains(key)) return fallback;
            const picojson::value& v = json.get(key);
            return v.is<bool>() ? v.get<bool>() : fallback;
        }

        inline double GetNumber(const picojson::value& json, const char* key, double fallback = 0.0)
        {
            if (!json.contains(key)) return fallback;
            const picojson::value& v = json.get(key);
            return v.is<double>() ? v.get<double>() : fallback;
        }

        inline int GetInt(const picojson::value& json, const char* key, int fallback = 0)
        {
            return static_cast<int>(GetNumber(json, key, static_cast<double>(fallback)));
        }

        inline picojson::value ItemToJson(const SettingItem& item)
        {
            picojson::object obj;
            obj["key"] = picojson::value(item.key);
            obj["label"] = picojson::value(item.label);
            obj["group"] = picojson::value(item.group);
            obj["kind"] = picojson::value(ToString(item.kind));
            obj["valueKind"] = picojson::value(ToString(item.valueKind));
            switch (item.valueKind)
            {
            case ValueKind::Bool:
                obj["value"] = picojson::value(item.boolValue);
                break;
            case ValueKind::Number:
                obj["value"] = picojson::value(item.numberValue);
                break;
            case ValueKind::String:
                obj["value"] = picojson::value(item.stringValue);
                break;
            }
            if (!item.description.empty())
                obj["description"] = picojson::value(item.description);
            if (item.kind == ItemKind::Slider)
            {
                obj["min"] = picojson::value(item.minValue);
                obj["max"] = picojson::value(item.maxValue);
                obj["step"] = picojson::value(item.step);
                obj["decimals"] = picojson::value(static_cast<double>(item.decimals));
                if (!item.suffix.empty())
                    obj["suffix"] = picojson::value(item.suffix);
            }
            if (item.kind == ItemKind::Choice)
            {
                picojson::array options;
                options.reserve(item.options.size());
                for (const auto& option : item.options)
                    options.emplace_back(option);
                obj["options"] = picojson::value(options);
                obj["selectedIndex"] = picojson::value(static_cast<double>(item.selectedIndex));
            }
            return picojson::value(obj);
        }

        inline SettingItem ItemFromJson(const picojson::value& json)
        {
            SettingItem item;
            if (!json.is<picojson::object>())
                return item;
            item.key = GetString(json, "key");
            item.label = GetString(json, "label");
            item.group = GetString(json, "group");
            item.description = GetString(json, "description");
            item.kind = ItemKindFromString(GetString(json, "kind"));
            item.valueKind = ValueKindFromString(GetString(json, "valueKind"));
            switch (item.valueKind)
            {
            case ValueKind::Bool:
                item.boolValue = GetBool(json, "value");
                break;
            case ValueKind::Number:
                item.numberValue = GetNumber(json, "value");
                break;
            case ValueKind::String:
                item.stringValue = GetString(json, "value");
                break;
            }
            item.minValue = GetNumber(json, "min", 0.0);
            item.maxValue = GetNumber(json, "max", 1.0);
            item.step = GetNumber(json, "step", 0.01);
            item.decimals = GetInt(json, "decimals", 0);
            item.suffix = GetString(json, "suffix");
            if (json.contains("options") && json.get("options").is<picojson::array>())
            {
                for (const auto& option : json.get("options").get<picojson::array>())
                {
                    if (option.is<std::string>())
                        item.options.push_back(option.get<std::string>());
                }
            }
            item.selectedIndex = GetInt(json, "selectedIndex", 0);
            return item;
        }
    } // namespace Detail

    // AppState -> JSON 字符串（消息体）
    inline std::string StateToJson(const AppState& state)
    {
        picojson::object root;
        root["type"] = picojson::value("state");
        root["version"] = picojson::value(static_cast<double>(kProtocolVersion));
        root["editModeActive"] = picojson::value(state.editModeActive);

        picojson::array components;
        components.reserve(state.components.size());
        for (const auto& comp : state.components)
        {
            picojson::object obj;
            obj["name"] = picojson::value(comp.name);
            obj["description"] = picojson::value(comp.description);
            obj["icon"] = picojson::value(comp.iconGlyph);
            obj["visible"] = picojson::value(comp.visible);
            picojson::array items;
            items.reserve(comp.items.size());
            for (const auto& item : comp.items)
                items.emplace_back(Detail::ItemToJson(item));
            obj["items"] = picojson::value(items);
            components.emplace_back(obj);
        }
        root["components"] = picojson::value(components);

        picojson::array globals;
        globals.reserve(state.globalItems.size());
        for (const auto& item : state.globalItems)
            globals.emplace_back(Detail::ItemToJson(item));
        root["global"] = picojson::value(globals);

        return picojson::value(root).serialize();
    }

    // 解析 AppState；失败返回 false
    inline bool StateFromJson(const std::string& text, AppState& state)
    {
        picojson::value root;
        if (!picojson::parse(root, text).empty())
            return false;
        if (!root.is<picojson::object>())
            return false;
        state = AppState{};
        state.editModeActive = Detail::GetBool(root, "editModeActive", false);

        if (root.contains("components") && root.get("components").is<picojson::array>())
        {
            for (const auto& compJson : root.get("components").get<picojson::array>())
            {
                if (!compJson.is<picojson::object>())
                    continue;
                ComponentState comp;
                comp.name = Detail::GetString(compJson, "name");
                comp.description = Detail::GetString(compJson, "description");
                comp.iconGlyph = Detail::GetString(compJson, "icon");
                comp.visible = Detail::GetBool(compJson, "visible", true);
                if (compJson.contains("items") && compJson.get("items").is<picojson::array>())
                {
                    for (const auto& itemJson : compJson.get("items").get<picojson::array>())
                        comp.items.push_back(Detail::ItemFromJson(itemJson));
                }
                state.components.push_back(std::move(comp));
            }
        }

        if (root.contains("global") && root.get("global").is<picojson::array>())
        {
            for (const auto& itemJson : root.get("global").get<picojson::array>())
                state.globalItems.push_back(Detail::ItemFromJson(itemJson));
        }
        return true;
    }

    // 命令 -> JSON 字符串
    inline std::string CommandToJson(const Command& command)
    {
        picojson::object root;
        root["type"] = picojson::value(command.type);
        if (!command.name.empty())
            root["name"] = picojson::value(command.name);
        if (!command.key.empty())
            root["key"] = picojson::value(command.key);
        root["valueKind"] = picojson::value(Detail::ToString(command.valueKind));
        switch (command.valueKind)
        {
        case ValueKind::Bool:
            root["value"] = picojson::value(command.boolValue);
            break;
        case ValueKind::Number:
            root["value"] = picojson::value(command.numberValue);
            break;
        case ValueKind::String:
            root["value"] = picojson::value(command.stringValue);
            break;
        }
        return picojson::value(root).serialize();
    }

    // 解析命令；失败返回 false
    inline bool CommandFromJson(const std::string& text, Command& command)
    {
        picojson::value root;
        if (!picojson::parse(root, text).empty())
            return false;
        if (!root.is<picojson::object>())
            return false;
        command = Command{};
        command.type = Detail::GetString(root, "type");
        command.name = Detail::GetString(root, "name");
        command.key = Detail::GetString(root, "key");
        command.valueKind = Detail::ValueKindFromString(Detail::GetString(root, "valueKind"));
        switch (command.valueKind)
        {
        case ValueKind::Bool:
            command.boolValue = Detail::GetBool(root, "value");
            break;
        case ValueKind::Number:
            command.numberValue = Detail::GetNumber(root, "value");
            break;
        case ValueKind::String:
            command.stringValue = Detail::GetString(root, "value");
            break;
        }
        return !command.type.empty();
    }

    // -------------------------------------------------------------------------
    // 管道收发：4 字节小端长度前缀 + 负载
    // -------------------------------------------------------------------------
    inline bool WriteAll(HANDLE pipe, const void* data, uint32_t size)
    {
        const char* cursor = static_cast<const char*>(data);
        uint32_t remaining = size;
        while (remaining > 0)
        {
            DWORD written = 0;
            const DWORD chunk = remaining > 0x40000000u ? 0x40000000u : remaining;
            if (!WriteFile(pipe, cursor, chunk, &written, nullptr) || written == 0)
                return false;
            cursor += written;
            remaining -= written;
        }
        return true;
    }

    inline bool ReadAll(HANDLE pipe, void* data, uint32_t size)
    {
        char* cursor = static_cast<char*>(data);
        uint32_t remaining = size;
        while (remaining > 0)
        {
            DWORD read = 0;
            const DWORD chunk = remaining > 0x40000000u ? 0x40000000u : remaining;
            if (!ReadFile(pipe, cursor, chunk, &read, nullptr) || read == 0)
                return false;
            cursor += read;
            remaining -= read;
        }
        return true;
    }

    // 写一条消息
    inline bool WriteMessage(HANDLE pipe, const std::string& payload)
    {
        if (payload.size() > kMaxMessageSize)
            return false;
        const uint32_t length = static_cast<uint32_t>(payload.size());
        if (!WriteAll(pipe, &length, sizeof(length)))
            return false;
        return WriteAll(pipe, payload.data(), length);
    }

    // 读一条消息（阻塞；连接断开返回 false）
    inline bool ReadMessage(HANDLE pipe, std::string& payload)
    {
        uint32_t length = 0;
        if (!ReadAll(pipe, &length, sizeof(length)))
            return false;
        if (length == 0 || length > kMaxMessageSize)
            return false;
        payload.resize(length);
        return ReadAll(pipe, payload.data(), length);
    }
} // namespace Settings
} // namespace Miau
