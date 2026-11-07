#include "Config.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace Config
{
    // 构造函数：可指定配置文件完整路径
    Config::Config(const std::string& configFile)
        : m_configFile(configFile)
    {}

    // 设置配置文件完整路径
    void Config::SetConfigFile(const std::string& path)
    {
        // 如果文件不存在就创建
        if (!std::filesystem::exists(path))
            std::ofstream{ path };
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_configFile = path;
    }

    // 将路径按分隔符切分为段
    std::vector<std::string> Config::SplitPath(const std::string& path)
    {
        std::vector<std::string> segments;
        std::string current;
        for (char ch : path)
        {
            if (ch == '.')
            {
                if (!current.empty())
                {
                    segments.push_back(current);
                    current.clear();
                }
            }
            else
                current.push_back(ch);
        }
        if (!current.empty())
            segments.push_back(current);
        return segments;
    }

    // 通用注册逻辑：按路径创建节点树，末尾段作为配置项名
    void Config::AddItemImpl(const std::string& path, ItemType type, const picojson::value& defaultValue)
    {
        std::vector<std::string> segments = SplitPath(path);
        if (segments.empty())
            return;

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        Node* node = &m_root;
        // 创建中间节点路径（除最后一段外）
        for (size_t i = 0; i + 1 < segments.size(); ++i)
        {
            auto it = node->children.find(segments[i]);
            if (it == node->children.end())
            {
                auto child = std::make_unique<Node>();
                child->name = segments[i];
                it = node->children.emplace(segments[i], std::move(child)).first;
            }
            node = it->second.get();
        }

        // 末尾段作为配置项写入
        Item item;
        item.type = type;
        item.defaultValue = defaultValue;
        item.value = defaultValue;
        node->items[segments.back()] = std::move(item);
    }

    void Config::AddItem(const std::string& path, bool defaultValue)
    {
        AddItemImpl(path, ItemType::Bool, picojson::value(defaultValue));
    }

    void Config::AddItem(const std::string& path, int defaultValue)
    {
        AddItemImpl(path, ItemType::Int, picojson::value(static_cast<double>(defaultValue)));
    }

    void Config::AddItem(const std::string& path, unsigned int defaultValue)
    {
        AddItemImpl(path, ItemType::Int, picojson::value(static_cast<double>(defaultValue)));
    }

    void Config::AddItem(const std::string& path, long long defaultValue)
    {
        AddItemImpl(path, ItemType::Int, picojson::value(static_cast<double>(defaultValue)));
    }

    void Config::AddItem(const std::string& path, float defaultValue)
    {
        AddItemImpl(path, ItemType::Float, picojson::value(static_cast<double>(defaultValue)));
    }

    void Config::AddItem(const std::string& path, double defaultValue)
    {
        AddItemImpl(path, ItemType::Double, picojson::value(defaultValue));
    }

    void Config::AddItem(const std::string& path, const std::string& defaultValue)
    {
        AddItemImpl(path, ItemType::String, picojson::value(defaultValue));
    }

    void Config::AddItem(const std::string& path, const char* defaultValue)
    {
        AddItemImpl(path, ItemType::String, picojson::value(defaultValue == nullptr ? "" : defaultValue));
    }

    // 判断配置项是否存在
    bool Config::HasItem(const std::string& path) const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return FindItem(path) != nullptr;
    }

    // 从文件加载配置
    void Config::LoadConfig()
    {
        std::string configFile;
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            configFile = m_configFile;
        }
        if (configFile.empty())
            return;

        std::ifstream ifs(configFile);
        if (!ifs.is_open()) // 文件不存在：保持默认值
            return;

        picojson::value rootJson;
        const std::string err = picojson::parse(rootJson, ifs);
        if (!err.empty())
            return;

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_root.FromJson(rootJson);
    }

    // 将当前配置写入文件
    void Config::WriteConfig() const
    {
        std::string configFile;
        picojson::value rootJson;
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            configFile = m_configFile;
            rootJson = m_root.ToJson();
        }
        if (configFile.empty())
            return;

        // 确保父目录存在
        const std::filesystem::path filePath(configFile);
        const std::filesystem::path parentDir = filePath.parent_path();
        if (!parentDir.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(parentDir, ec);
        }

        std::ofstream ofs(configFile);
        if (ofs.is_open())
            ofs << rootJson.serialize(true);
    }

    // 重置配置为默认值
    // nodePath 为空表示根节点（重置全部）；指定节点则重置该节点及其子树
    void Config::ResetConfig(const std::string& nodePath)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (nodePath.empty())
        {
            m_root.Reset();
            return;
        }
        Node* node = FindNode(nodePath);
        if (node != nullptr)
            node->Reset();
    }

    // 访问根节点（只读）
    const Node& Config::Root() const
    {
        return m_root;
    }

    // 查找配置项（调用方需持有锁）
    Item* Config::FindItem(const std::string& path)
    {
        std::vector<std::string> segments = SplitPath(path);
        if (segments.empty())
            return nullptr;
        Node* node = &m_root;
        for (size_t i = 0; i + 1 < segments.size(); ++i)
        {
            auto it = node->children.find(segments[i]);
            if (it == node->children.end())
                return nullptr;
            node = it->second.get();
        }
        auto it = node->items.find(segments.back());
        if (it == node->items.end())
            return nullptr;
        return &it->second;
    }

    const Item* Config::FindItem(const std::string& path) const
    {
        std::vector<std::string> segments = SplitPath(path);
        if (segments.empty())
            return nullptr;
        const Node* node = &m_root;
        for (size_t i = 0; i + 1 < segments.size(); ++i)
        {
            auto it = node->children.find(segments[i]);
            if (it == node->children.end())
                return nullptr;
            node = it->second.get();
        }
        auto it = node->items.find(segments.back());
        if (it == node->items.end())
            return nullptr;
        return &it->second;
    }

    // 查找节点（调用方需持有锁）
    Node* Config::FindNode(const std::string& path)
    {
        if (path.empty())
            return &m_root;
        std::vector<std::string> segments = SplitPath(path);
        Node* node = &m_root;
        for (const std::string& seg : segments)
        {
            auto it = node->children.find(seg);
            if (it == node->children.end())
                return nullptr;
            node = it->second.get();
        }
        return node;
    }

    const Node* Config::FindNode(const std::string& path) const
    {
        if (path.empty())
            return &m_root;
        std::vector<std::string> segments = SplitPath(path);
        const Node* node = &m_root;
        for (const std::string& seg : segments)
        {
            auto it = node->children.find(seg);
            if (it == node->children.end())
                return nullptr;
            node = it->second.get();
        }
        return node;
    }

    // 序列化为 JSON 值
    picojson::value Node::ToJson() const
    {
        picojson::object obj;
        for (const auto& kv : items)
            obj[kv.first] = kv.second.value;
        for (const auto& kv : children)
            obj[kv.first] = kv.second->ToJson();
        return picojson::value(obj);
    }

    // 判断 JSON 值与配置项类型是否兼容
    static bool TypeCompatible(const Item& item, const picojson::value& jsonValue)
    {
        switch (item.type)
        {
            case ItemType::Bool:
                return jsonValue.is<bool>() || jsonValue.is<double>();
            case ItemType::Int:
            case ItemType::Float:
            case ItemType::Double:
                return jsonValue.is<double>();
            case ItemType::String:
                return jsonValue.is<std::string>();
        }
        return false;
    }

    // 从 JSON 值加载（仅更新已注册的配置项，未知键忽略）
    void Node::FromJson(const picojson::value& json)
    {
        if (!json.is<picojson::object>())
            return;
        const picojson::object& obj = json.get<picojson::object>();
        for (const auto& kv : obj)
        {
            // 是配置项
            auto itemIt = items.find(kv.first);
            if (itemIt != items.end())
            {
                if (TypeCompatible(itemIt->second, kv.second))
                    itemIt->second.value = kv.second;
                continue;
            }
            // 是子节点
            auto childIt = children.find(kv.first);
            if (childIt != children.end())
                childIt->second->FromJson(kv.second);
        }
    }

    // 递归重置本节点及子树为默认值
    void Node::Reset()
    {
        for (auto& kv : items)
            kv.second.value = kv.second.defaultValue;
        for (auto& kv : children)
            kv.second->Reset();
    }

    // =========================================================================
    // 组件配置视图（ComponentConfig）
    // =========================================================================

    // 绑定根节点路径
    ComponentConfig::ComponentConfig(const std::string& rootPath)
        : m_rootPath(rootPath)
    {
    }

    // 绑定根节点路径（组件名）
    void ComponentConfig::Bind(const std::string& rootPath)
    {
        m_rootPath = rootPath;
    }

    // 当前根节点路径
    const std::string& ComponentConfig::RootPath() const
    {
        return m_rootPath;
    }

    // 拼接完整路径：根节点.配置项名
    std::string ComponentConfig::MakePath(const std::string& name) const
    {
        if (m_rootPath.empty())
            return name;
        return m_rootPath + "." + name;
    }

    void ComponentConfig::AddItem(const std::string& name, bool defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    void ComponentConfig::AddItem(const std::string& name, int defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    void ComponentConfig::AddItem(const std::string& name, unsigned int defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    void ComponentConfig::AddItem(const std::string& name, long long defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    void ComponentConfig::AddItem(const std::string& name, float defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    void ComponentConfig::AddItem(const std::string& name, double defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    void ComponentConfig::AddItem(const std::string& name, const std::string& defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    void ComponentConfig::AddItem(const std::string& name, const char* defaultValue)
    {
        Instance().AddItem(MakePath(name), defaultValue);
    }

    // 重置本组件全部配置为默认值
    void ComponentConfig::Reset()
    {
        Instance().ResetConfig(m_rootPath);
    }
}
