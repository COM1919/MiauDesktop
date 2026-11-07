#pragma once
#include "imgui.h"
#include "imgui_internal.h"
namespace ImGui
{

    struct DrawGrabber_element
    {
        ImVec4 c;
    };
    static float bar_width = 14;

    template <typename T>
    void clip(T& n, const T& lower, const T& upper)
    {
        n = max(lower, min(n, upper));
    }

    template <class t>
    bool drawGrabber(const char* label, ImVec2& pos, t& input, const t& min, const t& max, float width, float height, const char* format, int idnum)
    {
        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;
        ImGuiWindow* win = GetCurrentWindow();
        //const ImU32 inactive_color = ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_SliderGrab]);
        //const ImU32 active_color = ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_SliderGrabActive]);

        float scale = max / width;
        bool changed = false;
        bool hovered = false;
        int current_draw_bar = input / scale;
        static float previous_delta;
        hovered = false;
        SetCursorScreenPos({ pos.x + current_draw_bar /* (width / max)*/, pos.y });
        std::string nid = label + std::to_string(idnum);
        PushID(nid.c_str());
        const ImGuiID id = win->GetID(nid.c_str());
        //PushID(label2);
        {
            InvisibleButton(nid.c_str(), { bar_width, height });
            if (IsItemActive() || IsItemHovered())
            {
                char combined[64];
                ImFormatString(combined, IM_ARRAYSIZE(combined), format, input);
                SetTooltip(combined);
                SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            if (IsItemActive() && IsMouseDragging(0))
            {
                input += (GetIO().MousePos.x - (pos.x + current_draw_bar) - bar_width / 2) * scale;
                clip(input, min, max);
                changed = hovered = true;
            }

            static std::map <ImGuiID, DrawGrabber_element> anim;
            auto it_anim = anim.find(id);
            if (it_anim == anim.end())
            {
                anim.insert({ id, { ColorConvertU32ToFloat4(GetColorU32(ImGuiCol_SliderGrab)) } });
                it_anim = anim.find(id);
            }
            float speed = 0.1f * (1.0f - GetIO().DeltaTime);

            ImU32 dg = (IsItemActive() || IsItemHovered() || hovered) ? GetColorU32(ImGuiCol_SliderGrabActive) : GetColorU32(ImGuiCol_SliderGrab);
            ImVec4 dg_vec = ColorConvertU32ToFloat4(dg);
            it_anim->second.c = ImLerp(it_anim->second.c, dg_vec, speed);

            win->DrawList->AddRectFilled({ pos.x + current_draw_bar + 2, pos.y + 2 }, { pos.x + current_draw_bar + bar_width, pos.y + height }, ColorConvertFloat4ToU32(it_anim->second.c)/*IsItemActive() || IsItemHovered() || hovered ? active_color : inactive_color*/, g.Style.GrabRounding);
        }
        PopID();
        return changed;
    }

    struct RangeSliderScalar_element
    {
        ImVec4 B;
    };
    template <class t>
    bool RangeSlider(const char* label, ImGuiDataType data_type, const t& min, const t& max, t& low, t& high, const char* format, ImGuiSliderFlags flags)
    {
        ImGuiWindow* win = GetCurrentWindow();
        if (win->SkipItems)
            return false;
        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;

        const ImU32 line_color = ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_Button]);
        const float button_size = GetFrameHeight();
        const ImGuiID id = win->GetID(label);
        const float bb_width = CalcItemWidth();

        bool changed = false;
        SetNextItemWidth(ImMax(1.0f, bb_width));
        ImVec2 AnimPos = CalcWidgetsAnimVert(id);

        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const ImRect frame_bb(AnimPos, ImVec2(bb_width + AnimPos.x, label_size.y + style.FramePadding.y * 2.0f + AnimPos.y));
        const ImRect total_bb(frame_bb.Min, ImVec2(frame_bb.Max.x + (label_size.x > 0.0f ? (style.ItemInnerSpacing.x + label_size.x) : 0.0f), frame_bb.Max.y));
        const float bb_height = frame_bb.Max.y - frame_bb.Min.y;

        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id, &frame_bb, 0))
            return false;

        const float scale = (max / bb_width);

        static std::map <ImGuiID, RangeSliderScalar_element> anim;
        auto it_anim = anim.find(id);
        if (it_anim == anim.end())
        {
            anim.insert({ id, { ColorConvertU32ToFloat4(GetColorU32(ImGuiCol_SliderGrab)) } });
            it_anim = anim.find(id);
        }
        float speed = 0.1f * (1.0f - GetIO().DeltaTime);

        ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
        ImVec4 tB_vec = ImGui::ColorConvertU32ToFloat4(frame_col);
        it_anim->second.B = ImLerp(it_anim->second.B, tB_vec, speed);
        frame_col = ColorConvertFloat4ToU32(it_anim->second.B);

        win->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, frame_col, g.Style.FrameRounding);
        win->DrawList->AddRectFilled({ AnimPos.x + low / scale + 13, AnimPos.y + 10 }, { AnimPos.x + high / scale + 2, AnimPos.y + 16 }, ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_SliderGrab])); //bar between the 2 grabbers
       
        if (drawGrabber(label, AnimPos, low, min , max, bb_width, bb_height, format, 123))
            changed = true;
        if (drawGrabber(label, AnimPos, high, min, max, bb_width, bb_height, format, 456))
            changed = true;

        char value_buf_a[64];
        char value_buf_b[64];
        const char* value_buf_end_a = value_buf_a + DataTypeFormatString(value_buf_a, IM_COUNTOF(value_buf_a), data_type, (void*)&low, format);
        const char* value_buf_end_b = value_buf_b + DataTypeFormatString(value_buf_b, IM_COUNTOF(value_buf_b), data_type, (void*)&high, format);
        char combined[128];
        int len = ImFormatString(combined, IM_ARRAYSIZE(combined), "%s - %s", value_buf_a, value_buf_b);
        const char* text_end = combined + len;
        if (g.LogEnabled)
            LogSetNextTextDecoration("{", "}");
        RenderTextClipped(frame_bb.Min, frame_bb.Max, combined, text_end, NULL, ImVec2(0.5f, 0.5f));
        if (label_size.x > 0.0f)
            RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

        if (low >= high)
            std::swap(low, high);
        return changed;
    }
    
    inline bool RangeSliderInt(const char* id, const int& min, const int& max, int& low, int& high, const char* format = "%d")
    {
        return RangeSlider<int>(id, ImGuiDataType_S32, min, max, low, high, format, 0);
    }

    inline bool RangeSliderFloat(const char* id, const float& min, const float& max, float& low, float& high, const char* format = "%.3f")
    {
        return RangeSlider<float>(id, ImGuiDataType_Float, min, max, low, high, format, 0);
    }
    /*
    bool RangeSliderScalar(const char* label, ImGuiDataType data_type, void* p_low, void* p_high, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float w = CalcItemWidth();
        const ImU32 color_marker = (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasColorMarker) ? g.NextItemData.ColorMarker : 0;

        ImVec2 AnimPos = CalcWidgetsAnimVert(id);

        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const ImRect frame_bb(AnimPos, ImVec2(w + AnimPos.x, label_size.y + style.FramePadding.y * 2.0f + AnimPos.y));
        const ImRect total_bb(frame_bb.Min, ImVec2((label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f) + frame_bb.Max.x, frame_bb.Max.y));

        const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
            return false;

        // Default format string when passing NULL
        if (format == NULL)
            format = DataTypeGetInfo(data_type)->PrintFmt;

        const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
        bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
        if (!temp_input_is_active)
        {
            // Tabbing or Ctrl+Click on Slider turns it into an input box
            const bool is_active = g.NavActivateId == id;
            const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
            const bool make_active = (clicked || is_active);
            if (make_active && clicked)
                SetKeyOwner(ImGuiKey_MouseLeft, id);
            if (make_active && temp_input_allowed)
                if ((clicked && g.IO.KeyCtrl) || (is_active && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
                    temp_input_is_active = true;

            // Store initial value (not used by main lib but available as a convenience but some mods e.g. to revert)
            if (make_active)
            {
                size_t size = DataTypeGetInfo(data_type)->Size;
                memcpy(&g.ActiveIdValueOnActivation, p_low, size);
                memcpy(&g.ActiveIdValueOnActivation, p_high, size);
            }

            if (make_active && !temp_input_is_active)
            {
                SetActiveID(id, window);
                SetFocusID(id, window);
                FocusWindow(window);
                g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            }
        }

        if (temp_input_is_active)
        {
            // Only clamp Ctrl+Click input when ImGuiSliderFlags_ClampOnInput is set (generally via ImGuiSliderFlags_AlwaysClamp)
            const bool clamp_enabled = (flags & ImGuiSliderFlags_ClampOnInput) != 0;
            PushID(1);
            const bool a = TempInputScalar(frame_bb, id, label, data_type, p_low, format, clamp_enabled ? p_min : NULL, clamp_enabled ? p_max : NULL);
            PopID();
            PushID(2);
            const bool b = TempInputScalar(frame_bb, id, label, data_type, p_high, format, clamp_enabled ? p_min : NULL, clamp_enabled ? p_max : NULL);
            PopID();
        }

        static std::map <ImGuiID, RangeSliderScalar_element> anim;
        auto it_anim = anim.find(id);
        if (it_anim == anim.end())
        {
            anim.insert({ id, { ColorConvertU32ToFloat4(GetColorU32(ImGuiCol_FrameBg)) } });
            it_anim = anim.find(id);
        }
        float speed = 0.1f * (1.0f - ImGui::GetIO().DeltaTime);

        // Draw frame
        ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
        ImVec4 tB_vec = ImGui::ColorConvertU32ToFloat4(frame_col);
        it_anim->second.B = ImLerp(it_anim->second.B, tB_vec, speed);
        frame_col = ColorConvertFloat4ToU32(it_anim->second.B);

        RenderNavCursor(frame_bb, id);
        RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, false, style.FrameRounding);
        if (color_marker != 0 && style.ColorMarkerSize > 0.0f)
            RenderColorComponentMarker(frame_bb, GetColorU32(color_marker), style.FrameRounding);
        RenderFrameBorder(frame_bb.Min, frame_bb.Max, g.Style.FrameRounding);

        // Slider behavior
        ImVec2 mouse_pos(GetMousePos().x - GetWindowPos().x, GetMousePos().y - GetWindowPos().y);


        const float center_x = frame_bb.Min.x + (frame_bb.Max.x - frame_bb.Min.x) * 0.5f;
        bool value_changed = false;

        std::map<ImGuiID, float> last_grab_pos;
        
        ImRect grab_bb_a;
        PushID(1);
        {
            auto it_rect = last_grab_pos.find(id);
            if (it_rect == last_grab_pos.end())
            {
                last_grab_pos.insert({ id, center_x });
                it_rect = last_grab_pos.find(id);
            }
            ImRect frame(frame_bb.Min, ImVec2(it_rect->second, frame_bb.Max.y));
            if (SliderBehavior(ImRect(0, 0, 0, 0), id, data_type, p_low, p_min, p_max, format, flags, &grab_bb_a))
            {
                MarkItemEdited(id);
                value_changed = true;
            }
        }
        ImU32 col_a = GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
        PopID();
        ImRect grab_bb_b;
        PushID(2);
        if (!value_changed)
        {
            auto it_rect = last_grab_pos.find(id);
            if (it_rect == last_grab_pos.end())
            {
                last_grab_pos.insert({ id, center_x });
                it_rect = last_grab_pos.find(id);
            }
            ImRect frame(ImVec2(it_rect->second, frame_bb.Min.y), frame_bb.Max);
            if (SliderBehavior(ImRect(0, 0, 0, 0), id, data_type, p_high, p_min, p_max, format, flags, &grab_bb_b))
            {
                MarkItemEdited(id);
                value_changed = true;
            }
        }
        ImU32 col_b = GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
        PopID();

        // Render grab
        if (grab_bb_a.Max.x > grab_bb_a.Min.x)
        {
            float size_x = grab_bb_a.Max.x - grab_bb_a.Min.x;
            window->DrawList->AddRectFilled(grab_bb_a.Min, ImVec2(grab_bb_a.Min.x + size_x, grab_bb_a.Max.y), col_a, style.GrabRounding);
        }
        if (grab_bb_b.Max.x > grab_bb_b.Min.x)
        {
            float size_x = grab_bb_b.Max.x - grab_bb_b.Min.x;
            window->DrawList->AddRectFilled(grab_bb_b.Min, ImVec2(grab_bb_b.Min.x + size_x, grab_bb_b.Max.y), col_b, style.GrabRounding);
        }

        // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
        char value_buf_a[64];
        char value_buf_b[64];
        const char* value_buf_end_a = value_buf_a + DataTypeFormatString(value_buf_a, IM_COUNTOF(value_buf_a), data_type, p_low, format);
        const char* value_buf_end_b = value_buf_b + DataTypeFormatString(value_buf_b, IM_COUNTOF(value_buf_b), data_type, p_high, format);
        char combined[128];
        int len = ImFormatString(combined, IM_ARRAYSIZE(combined), "%s - %s", value_buf_a, value_buf_b);
        const char* text_end = combined + len;
        if (g.LogEnabled)
            LogSetNextTextDecoration("{", "}");
        RenderTextClipped(frame_bb.Min, frame_bb.Max, combined, text_end, NULL, ImVec2(0.5f, 0.5f));

        if (label_size.x > 0.0f)
            RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (temp_input_allowed ? ImGuiItemStatusFlags_Inputable : 0));
        return value_changed;
    }
    bool RangeSliderInt(const char* label, int* v_low, int* v_high, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        return RangeSliderScalar(label, ImGuiDataType_S32, v_low, v_high, &v_min, &v_max, format, flags);
    }

    bool RangeSliderFloat(const char* label, float* v_low, float* v_high, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        return RangeSliderScalar(label, ImGuiDataType_Float, v_low, v_high, &v_min, &v_max, format, flags);
    }*/
}
