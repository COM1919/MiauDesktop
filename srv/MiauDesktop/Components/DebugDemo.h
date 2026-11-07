#pragma once
#include "../Framework.h"
#include "../Component.h"

class DebugDemo : public Component
{
public:
    DebugDemo() : Component("调试演示", ImGuiWindowFlags_AlwaysAutoResize)
    {
        m_useGuiRendering = false;  // Debug 组件使用原始 ImGui 风格
    }

    const char* GetIcon() const override
    {
        return ICON_FA_BUG;
    }
    const char* GetDescription() const override
    {
        return "ImGui 调试与演示窗口";
    }

protected:
    void ComponentRender() override
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Text("我去NB");
        ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);

        if (ImGui::Button("Button"))
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    }

    void BeforeEnd() override
    {
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
			// 对于额外维护的Begin，需要手动注册为组件窗口，以便穿透逻辑生效
            g_compManager.RegisterCurrentWindow();

            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
    }

private:
    bool show_demo_window = false;
    bool show_another_window = false;
};
