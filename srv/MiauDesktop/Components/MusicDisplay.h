#pragma once
#include "../Framework.h"
#include "../Component.h"
#include "../GuiBase/GuiBase.h"

class MusicDisplay : public Component
{
public:
    bool m_showPicture = true;
    bool m_showProgress = true;

    // 配置加载：基础配置已由基类处理，这里只读自己的特有配置项
    void OnLoadConfig() override
    {
        m_showPicture = config.Get<int>("ShowPicture");
        m_showProgress = config.Get<bool>("ShowProgress");
    }
    void OnSaveConfig() override
    {
        config.Set("ShowPicture", m_showPicture);
        config.Set("ShowProgress", m_showProgress);
    }

    MusicDisplay() : Component("音乐信息显示")
    {
        // 初始状态
        {
            std::lock_guard<std::mutex> lock(m_DataMutex);
            m_Title = "未在播放";
            m_Artist = "等待媒体...";
            m_CurrentLyric = "♪";
            m_NextLyric = "";
            m_Progress = 0.0f;
            m_AlbumCover = nullptr;
        }

        SetWindowSize(ImVec2(400, 160));
        windowFlags |= ImGuiWindowFlags_NoResize;           // 禁止调整大小
        windowFlags |= ImGuiWindowFlags_NoScrollbar;        // 禁止滚动条
        windowFlags |= ImGuiWindowFlags_NoScrollWithMouse;  // 禁止鼠标滚轮滚动
        windowFlags |= ImGuiWindowFlags_NoTitleBar;         // 禁止标题栏
        windowFlags |= ImGuiWindowFlags_NoMove;             // 禁止移动

        m_UpdaterFuture = std::async(std::launch::async, [this]()
        {
            try
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            }
            catch (...) {}

            while (!m_StopUpdater)
            {
                FetchMediaInfoAsync();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });
    }

    const char* GetIcon() const override
    {
        return ICON_FA_MUSIC;
    }
    const char* GetDescription() const override
    {
        return "显示当前系统播放的音乐信息";
    }

    ~MusicDisplay() override
    {
        m_StopUpdater = true;
        if (m_UpdaterFuture.valid()) m_UpdaterFuture.wait();
    }

protected:
    void AfterBegin() override
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    }

    void ComponentRender() override
    {
        ImGui::PopStyleVar();

        // 封面加载
        if (m_NeedLoadCover.exchange(false))
        {
            std::vector<uint8_t> imageData;
            {
                std::lock_guard<std::mutex> lock(m_DataMutex);
                imageData = std::move(m_PendingCoverData);
                m_PendingCoverData.clear();
            }
            if (m_AlbumCover)
            {
                GuiBase::UnloadTexture("MusicCover");
                m_AlbumCover = nullptr;
            }
            if (!imageData.empty())
                m_AlbumCover = GuiBase::LoadTextureFromMemory(imageData.data(), imageData.size(), "MusicCover");
        }

        // 数据快照
        std::string title, artist, currentLyric, nextLyric;
        float snapshotProgress = 0.0f;
        ULONGLONG startTick = 0;      // 歌曲起点时间戳（毫秒）
        float totalSeconds = 0.0f;    // 歌曲总时长（秒）
        auto playStatus = GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped;
        {
            std::lock_guard<std::mutex> lock(m_DataMutex);
            title = m_Title;
            artist = m_Artist;
            currentLyric = m_CurrentLyric;
            nextLyric = m_NextLyric;
            snapshotProgress = m_Progress;
            startTick = m_startTick;
            totalSeconds = m_totalSeconds;
            playStatus = m_playStatus;
        }

        // 渲染时用时间戳推算进度（每帧连续推进，不受后台 200ms 更新间隔影响）
        // 当前播放时长 = 当前时间戳 - 歌曲起点时间戳；进度 = 播放时长 / 总时长
        float progress = 0.0f;
        if (playStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing && totalSeconds > 1.0f)
        {
            double elapsed = (double)(GetTickCount64() - startTick) / 1000.0;   // 毫秒 → 秒
            double p = elapsed / totalSeconds;
            progress = (float)(p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p));
        }
        else if (playStatus == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused)
        {
            progress = snapshotProgress;   // 暂停：进度保持
        }
        // 停止：进度为 0

        GuiBase::GuiThemeTokens tokens = GuiBase::Theme::Dark();
        GuiBase::GuiColor textColor = tokens.text;                    // 白色
        GuiBase::GuiColor subColor(0.55f, 0.55f, 0.60f, 1.0f);       // 次要文字
        GuiBase::GuiColor accentColor(0.98f, 0.62f, 0.15f, 1.0f);    // 暖色强调（歌词）
        GuiBase::GuiColor surfaceColor(0.15f, 0.15f, 0.18f, 1.0f);   // 表面色（占位封面底）
        GuiBase::GuiColor borderColor(0.25f, 0.25f, 0.30f, 1.0f);    // 边框/分割线
        GuiBase::GuiColor trackColor(0.20f, 0.20f, 0.24f, 1.0f);     // 进度条轨道
        GuiBase::GuiColor progressColor(0.98f, 0.62f, 0.15f, 1.0f);  // 进度条填充（暖色）

        // 绘制 UI
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGui::Columns(2, "MusicLayout", false);

        // 左侧封面
        {
            const float coverSize = 80.0f;
            ImGui::SetColumnWidth(0, coverSize + 15.0f);

            if (m_AlbumCover)
            {
                ImGui::ImageRounded((ImTextureID)m_AlbumCover, ImVec2(coverSize, coverSize), 8.0f);
            }
            else
            {
                ImVec2 p_min = ImGui::GetCursorScreenPos();
                ImVec2 p_max = ImVec2(p_min.x + coverSize, p_min.y + coverSize);
                draw_list->AddRectFilled(p_min, p_max, surfaceColor.ToImU32(), 8.0f);
                const char* placeholder = "无封面";
                ImVec2 textSize = ImGui::CalcTextSize(placeholder);
                draw_list->AddText(ImVec2(p_min.x + (coverSize - textSize.x) * 0.5f, p_min.y + (coverSize - textSize.y) * 0.5f),
                                   subColor.ToImU32(), placeholder);
                ImGui::Dummy(ImVec2(coverSize, coverSize));
            }
            ImGui::NextColumn();
        }

        // 右侧文字信息
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
            ImGui::TextColored(textColor.ToImVec4(), "%s", title.c_str());
            ImGui::TextColored(subColor.ToImVec4(), "%s", artist.c_str());

            ImGui::Spacing();
            ImVec2 line_start = ImGui::GetCursorScreenPos();
            line_start.x -= 5.0f;
            draw_list->AddLine(line_start, ImVec2(line_start.x + ImGui::GetColumnWidth() - 10, line_start.y),
                               borderColor.ToImU32(), 1.0f);
            ImGui::Spacing();

            const char* text = currentLyric.empty() ? "♪" : currentLyric.c_str();
            ImGui::TextColored(accentColor.ToImVec4(), "%s", text);

            ImGui::TextColored(subColor.ToImVec4(), "%s", nextLyric.c_str());
            ImGui::Columns(1);
        }

        // 底部 EUI 胶囊进度条
        {
            float barH = 6.0f;
            float barMargin = 10.0f;
            ImVec2 windowSize = ImGui::GetWindowSize();
            // 将进度条贴在窗口底部
            float barY = ImGui::GetWindowPos().y + windowSize.y - barMargin - barH;
            ImVec2 barPos(ImGui::GetWindowPos().x + barMargin, barY);
            ImVec2 barSize(windowSize.x - barMargin * 2.0f, barH);
            GuiBase::DrawGuiProgress(draw_list, barPos, barSize, progress, progressColor, trackColor);
        }
    }

private:
    mutable std::mutex m_DataMutex;

    // 歌曲时间线推算状态（m_DataMutex 保护，主线程渲染时读取）
    // 推算原理（单位统一为秒）：
    //   · 只有手动拖动进度条 / 切歌 / 状态切换时，系统才会更新 Position
    //   · 收到更新时校准：歌曲起点时间戳 = 当前时间戳 - Position（秒）
    //   · 渲染时：当前播放时长 = 当前时间戳 - 起点时间戳；进度 = 播放时长 / 总时长
    ULONGLONG m_startTick = 0;                // 歌曲起点时间戳（GetTickCount64 毫秒）
    float m_totalSeconds = 0.0f;              // 歌曲总时长（秒，EndTime 换算）
    float m_Progress = 0.0f;                  // 推算进度（供歌词生成与暂停保持）
    GlobalSystemMediaTransportControlsSessionPlaybackStatus m_playStatus =
        GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped;  // 播放状态
    std::string m_Title, m_Artist, m_CurrentLyric, m_NextLyric;
    Texture2D m_AlbumCover = nullptr;

    // 上次校准的系统 Position（仅后台更新线程访问，用于检测"收到新的进度更新"）
    float m_lastRawProgress = -1.0f;

    std::atomic<bool> m_StopUpdater{ false };
    std::future<void> m_UpdaterFuture;
    std::atomic<bool> m_NeedLoadCover{ false };
    std::vector<uint8_t> m_PendingCoverData;

    std::string m_LastTitle;  // 用于检测歌曲变化

    // 后台数据同步
    void FetchMediaInfoAsync()
    {
        try
        {
            static GlobalSystemMediaTransportControlsSessionManager sessionManager = nullptr;
            if (!sessionManager)
                sessionManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            if (!sessionManager)
                return;

            auto session = sessionManager.GetCurrentSession();
            if (!session)
            {
                std::lock_guard<std::mutex> lock(m_DataMutex);
                if (m_Title != "未在播放")
                {
                    m_Title = "未在播放";
                    m_Artist = "";
                    m_CurrentLyric = "♪";
                    m_Progress = 0.0f;
                    m_LastTitle.clear();
                    m_PendingCoverData.clear();
                    m_NeedLoadCover = true;
                }
                // 重置时间线推算状态（恢复播放时重新校准）
                m_startTick = GetTickCount64();
                m_totalSeconds = 0.0f;
                m_playStatus = GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped;
                m_lastRawProgress = -1.0f;
                return;
            }

            auto mediaProps = session.TryGetMediaPropertiesAsync().get();
            if (!mediaProps) return;

            std::string newTitle = winrt::to_string(mediaProps.Title());
            std::string newArtist = winrt::to_string(mediaProps.Artist());
            if (newTitle.empty()) newTitle = "未知歌曲";
            if (newArtist.empty()) newArtist = "未知艺术家";

            // 获取播放状态
            auto playbackInfo = session.GetPlaybackInfo();
            auto status = playbackInfo ? playbackInfo.PlaybackStatus() : GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped;

            // 从系统时间线读取原始进度（Position/EndTime，单位 100ns）
            // 注意：Windows 只在手动拖动进度条 / 切歌 / 状态切换时更新 Position，
            //       自然播放时 Position 不会变化。因此这里只把读取到的值当作
            //       "校准锚点"，实际进度由时间戳推算（见下方校准逻辑）。
            auto timeline = session.GetTimelineProperties();
            float rawProgress = -1.0f;   // -1 表示系统值无效（EndTime 为 0）
            float totalSeconds = 0.0f;   // 歌曲总时长（秒）
            bool timelineValid = false;
            if (timeline)
            {
                auto end = timeline.EndTime();
                if (end.count() > 0)
                {
                    auto pos = timeline.Position();
                    totalSeconds = (float)((double)end.count() / 10000000.0);   // 100ns → 秒
                    float p = (float)((double)pos.count() / (double)end.count());
                    if (p >= 0.0f && p <= 1.0f)
                    {
                        rawProgress = p;
                        timelineValid = true;
                    }
                }
            }

            bool songChanged = false;
            {
                std::lock_guard<std::mutex> lock(m_DataMutex);

                // 检测歌曲变化
                if (newTitle != m_LastTitle)
                {
                    songChanged = true;
                    m_LastTitle = newTitle;
                }

                // ---- 更新成员变量 ----
                m_Title = newTitle;
                m_Artist = newArtist;

                // ---- 校准歌曲起点时间戳（仅在有更新时执行） ----
                // 歌曲起点时间戳 = 当前时间戳 - 当前播放时长（Position）
                // 之后渲染时：进度 = (当前时间戳 - 起点时间戳) / 总时长
                ULONGLONG nowTick = GetTickCount64();
                if (timelineValid && totalSeconds > 1.0f)
                    m_totalSeconds = totalSeconds;   // 总时长总是可用时更新

                if (songChanged)
                {
                    // 切歌：重新校准（新歌 Position 通常为 0）
                    m_lastRawProgress = -1.0f;
                    m_startTick = nowTick;
                    if (timelineValid && m_totalSeconds > 1.0f)
                        m_startTick = nowTick - (ULONGLONG)(rawProgress * m_totalSeconds * 1000.0);
                }
                else if (timelineValid && m_totalSeconds > 1.0f)
                {
                    // 收到新的进度更新（Position 变化 = 用户拖动进度条）或状态切换 → 重新校准
                    bool posUpdated = (m_lastRawProgress >= 0.0f
                        && fabsf(rawProgress - m_lastRawProgress) > 0.0005f);
                    if (posUpdated || status != m_playStatus)
                    {
                        m_startTick = nowTick - (ULONGLONG)(rawProgress * m_totalSeconds * 1000.0);
                        m_lastRawProgress = rawProgress;
                    }
                }

                // ---- 推算当前进度（供歌词生成与暂停保持） ----
                bool playing = (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                bool paused = (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused);
                if (playing && m_totalSeconds > 1.0f)
                {
                    double elapsed = (double)(nowTick - m_startTick) / 1000.0;   // 毫秒 → 秒
                    double p = elapsed / m_totalSeconds;
                    m_Progress = (float)(p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p));
                }
                else if (paused)
                {
                    // 暂停：进度保持（不推进）
                }
                else
                {
                    m_Progress = 0.0f;   // 停止：归零
                }
                m_playStatus = status;

                // 简单歌词模拟（基于推算进度）
                if (m_Progress < 0.3f)
                {
                    m_CurrentLyric = "正在播放: " + newTitle;
                    m_NextLyric = "...";
                }
                else if (m_Progress < 0.7f)
                {
                    m_CurrentLyric = "高潮部分";
                    m_NextLyric = "即将结束...";
                }
                else
                {
                    m_CurrentLyric = "歌曲即将结束";
                    m_NextLyric = "...";
                }
            }

            // ---- 封面加载（如果歌曲变化） ----
            if (songChanged)
            {
                bool hasCover = false;
                std::vector<uint8_t> fileData;

                auto thumbnailRef = mediaProps.Thumbnail();
                if (thumbnailRef)
                {
                    try
                    {
                        auto stream = thumbnailRef.OpenReadAsync().get();
                        auto size = stream.Size();
                        if (size > 0)
                        {
                            fileData.resize(static_cast<size_t>(size));
                            auto reader = DataReader(stream);
                            reader.LoadAsync(static_cast<uint32_t>(size)).get();
                            reader.ReadBytes(fileData);
                            hasCover = true;
                        }
                    }
                    catch (...) { /* 忽略 */ }
                }

                {
                    std::lock_guard<std::mutex> lock(m_DataMutex);
                    if (hasCover)
                        m_PendingCoverData = std::move(fileData);
                    else
                        m_PendingCoverData.clear();
                }
                m_NeedLoadCover = true;
            }
        }
        catch (const winrt::hresult_error& e)
        {
            std::cerr << "[MusicDisplay] WinRT错误: " << winrt::to_string(e.message()) << std::endl;
        }
        catch (...) {}
    }
};