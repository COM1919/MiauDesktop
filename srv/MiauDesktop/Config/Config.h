#pragma once
// Config.h - 略略略
// 树形配置系统
// 任何组件可通过 AddItem 注册配置项（支持嵌套节点，路径以 . 分隔，
// 如 "Network.Proxy.Port"），通过 WriteConfig / LoadConfig 将配置持久化为
// JSON 文件（picojson），通过 ResetConfig 将指定节点（或全部）恢复为默认值。
// 使用示例：
//   Config::Instance().SetConfigFile("config.json");
//   Config::Instance().AddItem("Network.Proxy.Port", 8080);
//   Config::Instance().AddItem("Display.Animations.Enabled", true);
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "../PicoJson/picojson.h"

namespace Config
{
    // 配置项类型
    enum class ItemType
    {
        Bool,
        Int,
        Float,
        Double,
        String,
    };

    // 配置项（叶子节点）：默认值 + 当前值
    struct Item
    {
        ItemType type = ItemType::String;
        picojson::value defaultValue;   // 默认值
        picojson::value value;          // 当前值
    };

    // 配置节点（树节点）：子配置项 + 子节点
    struct Node
    {
        std::string name;
        std::map<std::string, Item> items;
        std::map<std::string, std::unique_ptr<Node>> children;

        // 序列化为 JSON 值
        picojson::value ToJson() const;
        // 从 JSON 值加载（仅更新已注册的配置项，未知键忽略）
        void FromJson(const picojson::value& json);
        // 递归重置本节点及子树为默认值
        void Reset();
    };

    // 配置管理器
    class Config
    {
    public:
        // 构造函数可指定配置文件完整路径，也可稍后调用 SetConfigFile
        explicit Config(const std::string& configFile = "");

        // 设置配置文件完整路径
        void SetConfigFile(const std::string& path);

        // 添加配置项：path 以 . 分隔节点，末尾段作为配置项名
        // 中间节点不存在时自动创建
        void AddItem(const std::string& path, bool defaultValue);
        void AddItem(const std::string& path, int defaultValue);
        void AddItem(const std::string& path, unsigned int defaultValue);
        void AddItem(const std::string& path, long long defaultValue);
        void AddItem(const std::string& path, float defaultValue);
        void AddItem(const std::string& path, double defaultValue);
        void AddItem(const std::string& path, const std::string& defaultValue);
        void AddItem(const std::string& path, const char* defaultValue);

        // 判断配置项是否存在
        bool HasItem(const std::string& path) const;

        // 读取配置项（T 支持 bool / 数值类型 / std::string）
        template <typename T>
        T Get(const std::string& path) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            const Item* item = FindItem(path);
            if (item == nullptr) return T{};
            const picojson::value& jsonValue = item->value;
            if constexpr (std::is_same_v<T, bool>)
            {
                if (jsonValue.is<bool>())         return jsonValue.get<bool>();
                return false;
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                if (jsonValue.is<double>())       return static_cast<T>(jsonValue.get<double>());
                return T{};
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if (jsonValue.is<double>())       return static_cast<T>(jsonValue.get<double>());
                return T{};
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                if (jsonValue.is<std::string>())  return jsonValue.get<std::string>();
                return std::string{};
            }
            return T{};
        }

        // 写入配置项（运行时修改当前值）
        template <typename T>
        void Set(const std::string& path, const T& value)
        {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            Item* item = FindItem(path);
            if (item == nullptr)  return;
            if constexpr (std::is_same_v<T, bool>)             item->value = picojson::value(value);
            else if constexpr (std::is_floating_point_v<T>)    item->value = picojson::value(static_cast<double>(value));
            else if constexpr (std::is_integral_v<T>)          item->value = picojson::value(static_cast<double>(value));
            else if constexpr (std::is_same_v<T, std::string>) item->value = picojson::value(value);
            else if constexpr (std::is_same_v<T, const char*>) item->value = picojson::value(value == nullptr ? "" : value);
        }

        // 从文件加载配置（仅覆盖已注册的配置项）
        void LoadConfig();

        // 将当前配置写入文件（美化格式）
        void WriteConfig() const;

        // 重置配置为默认值
        // nodePath 为空表示根节点（重置全部）；指定节点则重置该节点及其子树
        void ResetConfig(const std::string& nodePath = "");

        // 访问根节点（只读）
        const Node& Root() const;

    private:
        // 通用注册逻辑：按路径创建节点并写入配置项
        void AddItemImpl(const std::string& path, ItemType type, const picojson::value& defaultValue);
        // 将路径按分隔符切分为段
        static std::vector<std::string> SplitPath(const std::string& path);
        // 查找配置项（调用方需持有锁）
        Item* FindItem(const std::string& path);
        const Item* FindItem(const std::string& path) const;
        // 查找节点（调用方需持有锁）
        Node* FindNode(const std::string& path);
        const Node* FindNode(const std::string& path) const;

        std::string m_configFile;
        Node m_root;
        mutable std::shared_mutex m_mutex;
    };
    // 全局配置实例（组件可直接使用 Config::Instance()）
    // 前向声明：供下方 ConfigItem / ComponentConfig 使用，定义在文件末尾
    inline Config& Instance();


    class ConfigItem
    {
    public:
        ConfigItem() = default;
        explicit ConfigItem(const std::string& path)
            : m_path(path)
        {}

        // 绑定完整路径（组件构造时调用，如 "CpuMotion.speed"）
        void Bind(const std::string& path)
        {
            m_path = path;
        }

        // 隐式类型转换：读取配置项
        template <typename T>
        operator T() const
        {
            return Instance().Get<T>(m_path);
        }

        // 赋值：写入配置项
        template <typename T>
        ConfigItem& operator=(const T& value)
        {
            Instance().Set(m_path, value);
            return *this;
        }

        // 配置项完整路径（如 "CpuMotion.speed"）
        const std::string& Path() const
        {
            return m_path;
        }

    private:
        std::string m_path;
    };

    // 组件配置视图：绑定到全局配置树中的指定节点（组件名）
    // 组件通过 config 注册 / 读写自己的配置项，路径自动带上组件名前缀
    // 使用示例：
    //   config.AddItem("windowSize", 200);          // 注册（挂到 "组件名.windowSize"）
    //   int w = config["windowSize"];               // 读取
    //   config["windowSize"] = 300;                 // 写入
    //   config.Reset();                             // 重置本组件配置
    class ComponentConfig
    {
    public:
        ComponentConfig() = default;
        explicit ComponentConfig(const std::string& rootPath);

        // 绑定根节点路径（组件名）
        void Bind(const std::string& rootPath);

        // 当前根节点路径（如 "CpuMotion"）
        const std::string& RootPath() const;

        // 注册配置项（自动挂在组件节点下，中间节点自动创建）
        void AddItem(const std::string& name, bool defaultValue);
        void AddItem(const std::string& name, int defaultValue);
        void AddItem(const std::string& name, unsigned int defaultValue);
        void AddItem(const std::string& name, long long defaultValue);
        void AddItem(const std::string& name, float defaultValue);
        void AddItem(const std::string& name, double defaultValue);
        void AddItem(const std::string& name, const std::string& defaultValue);
        void AddItem(const std::string& name, const char* defaultValue);

        // 读取配置项
        template <typename T>
        T Get(const std::string& name) const
        {
            return Instance().Get<T>(MakePath(name));
        }

        // 写入配置项
        template <typename T>
        void Set(const std::string& name, const T& value)
        {
            Instance().Set(MakePath(name), value);
        }

        // 按名字访问配置项（读取用隐式转换，写入用赋值）
        ConfigItem operator[](const std::string& name) const
        {
            return ConfigItem(MakePath(name));
        }

        // 重置本组件全部配置为默认值
        void Reset();

    private:
        // 拼接完整路径：根节点.配置项名
        std::string MakePath(const std::string& name) const;

        std::string m_rootPath;
    };

    // 全局配置实例定义
    inline Config& Instance()
    {
        static Config instance;
        return instance;
    }
}
