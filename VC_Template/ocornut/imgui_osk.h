#pragma once

#include <ctype.h>
#include <string.h>

namespace ImGuiOSK
{
    constexpr float RepeatDelay = 0.40f;
    constexpr float RepeatRate = 0.050f;
    constexpr float RepeatRateFast = 0.025f;
    constexpr float RepeatFastAfter = 1.50f;

    struct Event
    {
        ImGuiKey key;
        unsigned int ch;
    };
    struct State
    {
        enum { MaxEvents = 16 };
        ImGuiID      target     = 0;
        ImGuiWindow* targetWin  = nullptr;
        ImRect       rect       = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
        bool         hold       = false;
        bool         dismissed  = false;
        bool         shift      = false;
        bool         caps       = false;
        int          page       = 0; // 0 = Letters, 1 = Symbols, 2 = More Symbols
        ImVec2       pos        = ImVec2(0.0f, 0.0f);
        bool         posInit    = false;
        bool         dragging   = false;
        float        keySize    = 0.0f;
        ImGuiID      pressId    = 0;
        float        nextRepeat = 0.0f;
        Event        events[MaxEvents];
        int          eventCount = 0;
        float        lastShiftTap = -1.0f;
    };
    struct OSKChar
    {
        const char* label;
        unsigned int code;
    };
            
    inline State& Get()
    {
        static State s;
        return s;
    }
    inline void PushKey(ImGuiKey key)
    {
        State& s = Get();
        if (s.eventCount < State::MaxEvents)
            s.events[s.eventCount++] = { key, 0 };
    }
    inline void PushChar(unsigned int c)
    {
        State& s = Get();
        if (s.eventCount < State::MaxEvents)
            s.events[s.eventCount++] = { ImGuiKey_None, c };
    }
    inline void ProcessImGuiKeyboard()
    {
        ImGuiContext& g = *ImGui::GetCurrentContext();
        ImGuiIO& io = ImGui::GetIO();
        State& s = Get();

        if (!io.MouseDown[0]) s.pressId = 0;
        const bool activeIsWindowMove = g.ActiveIdWindow && g.ActiveId == g.ActiveIdWindow->MoveId;
        if (io.MouseClicked[0] && !s.rect.Contains(io.MousePos))
        {
            s.hold   = false;
            s.target = 0;
        }
        else if (s.hold && s.target && (g.ActiveId == 0 || activeIsWindowMove))
        {
            ImGui::SetActiveID(s.target, s.targetWin);
        }
        else if (g.ActiveId && ImGui::GetInputTextState(g.ActiveId))
        {
            s.target    = g.ActiveId;
            s.targetWin = g.ActiveIdWindow;
        }
        
        // our flusher to flush out the history...
        for (int i = 0; i < s.eventCount; i++)
        {
            if (s.events[i].key != ImGuiKey_None)
            {
                io.AddKeyEvent(s.events[i].key, true);
                io.AddKeyEvent(s.events[i].key, false);
            }
            else
            {
                io.AddInputCharacter(s.events[i].ch);
            }
        }
        s.eventCount = 0;
        
        if (io.MouseClicked[0] && s.rect.Contains(io.MousePos))
            s.hold = true;

        bool visible;
        const bool wanted = io.WantTextInput || s.hold;
        if (!wanted) s.dismissed = false;
        visible = wanted && !s.dismissed;
        
        if (!visible)
        {
            s.rect = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
            return;
        }

        // constant variables
        const ImGuiWindowFlags keyboardFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 pad = style.WindowPadding;
        const float sp = style.ItemSpacing.x;
        const float spy = style.ItemSpacing.y;
        const float u = s.keySize > 0.0f ? s.keySize : ImMax(40.0f, ImGui::GetFontSize() * 2.6f);
        const float rh = u;
        const float titleH = ImGui::GetFrameHeight();
        const float W = u * 10.0f + sp * 9.0f;
        const float winW = W + pad.x * 2.0f;
        const float h = pad.y * 2.0f + titleH + rh * 4.0f + spy * 4.0f;

        // dragging system for keyboard
        if (!s.posInit)
        {
            s.pos = ImVec2((io.DisplaySize.x - winW) * 0.5f, (io.DisplaySize.y - h) * 0.5f);
            s.posInit = true;
        }
        const ImRect titleBB(s.rect.Min, ImVec2(s.rect.Max.x, s.rect.Min.y + pad.y + titleH));
        const ImRect prevClose(ImVec2(s.rect.Max.x - pad.x - titleH, s.rect.Min.y + pad.y),
                               ImVec2(s.rect.Max.x - pad.x, s.rect.Min.y + pad.y + titleH));
        if (io.MouseClicked[0] && titleBB.Contains(io.MousePos) && !prevClose.Contains(io.MousePos))
            s.dragging = true;
        
        if (s.dragging)
        {
            if (io.MouseDown[0])
            {
                s.pos.x += io.MouseDelta.x;
                s.pos.y += io.MouseDelta.y;
            }
            else
            {
                s.dragging = false;
            }
        }
        s.pos.x = ImClamp(s.pos.x, 0.0f, ImMax(0.0f, io.DisplaySize.x - winW));
        s.pos.y = ImClamp(s.pos.y, 0.0f, ImMax(0.0f, io.DisplaySize.y - h));

        // our little keyboard... yea it's like Gboard similarly
        ImGui::SetNextWindowPos(s.pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(winW, h));
        if (ImGui::Begin("##imgui_osk", nullptr, keyboardFlags))
        {
            ImGuiWindow* win = ImGui::GetCurrentWindow();
            ImGui::BringWindowToDisplayFront(win);
            const ImVec2 wp = ImGui::GetWindowPos();
            s.rect = ImRect(wp, ImVec2(wp.x + winW, wp.y + h));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImRect closeBB(ImVec2(wp.x + winW - pad.x - titleH, wp.y + pad.y), 
                                 ImVec2(wp.x + winW - pad.x, wp.y + pad.y + titleH));
            const bool closeHov = closeBB.Contains(io.MousePos) && g.HoveredWindow == win;
            dl->AddRectFilled(wp, ImVec2(wp.x + winW, wp.y + pad.y + titleH),
                              ImGui::GetColorU32(ImGuiCol_TitleBgActive), style.WindowRounding,
                              ImDrawFlags_RoundCornersTop);
            const float ty = wp.y + pad.y + (titleH - ImGui::GetFontSize()) * 0.5f;

            // title at center-top
            const char* title = "Virtual Keyboard";
            const float titleSize = 37.0f; // the title size
            const ImVec2 textSize = ImGui::CalcTextSize(title);
            dl->AddText(ImGui::GetFont(), titleSize, ImVec2(wp.x + (winW - textSize.x) * 0.5f, ty - 12.0f),
                        ImGui::GetColorU32(ImGuiCol_Text), title);
            
            if (closeHov)
                dl->AddRectFilled(closeBB.Min, closeBB.Max,
                                  ImGui::GetColorU32(io.MouseDown[0] ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered),
                                  style.FrameRounding);
                
            if (closeHov && io.MouseClicked[0])
            {
                ImGui::ClearActiveID();
                s.dismissed = true;
                s.hold = false;
                s.target = 0;
                s.eventCount = 0;
            }
            ImGui::Dummy(ImVec2(W, titleH));
            
            // keys helper
            auto key = [&](const char* label, float w, bool repeat, bool on) -> int
            {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const ImRect bb(p, ImVec2(p.x + w, p.y + rh));
                const ImGuiID id = ImGui::GetID(label);
                ImGui::ItemSize(bb);

                const bool hov = bb.Contains(io.MousePos);
                int fires = 0;
                if (hov && io.MouseClicked[0] && g.HoveredWindow == win)
                {
                    s.pressId = id;
                    s.nextRepeat = RepeatDelay;
                    fires = 1;
                }
                else if (s.pressId == id)
                {
                    if (!io.MouseDown[0] || !hov)
                    {
                        s.pressId = 0;
                    }
                    else if (repeat)
                    {
                        for (int guard = 0; guard < 3 && io.MouseDownDuration[0] >= s.nextRepeat; guard++)
                        {
                            s.nextRepeat += (io.MouseDownDuration[0] > RepeatFastAfter) ? RepeatRateFast : RepeatRate;
                            fires++;
                        }
                    }
                }

                const bool down = (s.pressId == id) && io.MouseDown[0];
                ImGuiCol col = ImGuiCol_Button;
                if (down || on) col = ImGuiCol_ButtonActive;
                else if (hov) col = ImGuiCol_ButtonHovered;
                dl->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(col), style.FrameRounding);
                const ImVec2 ts = ImGui::CalcTextSize(label);
                dl->AddText(ImVec2(bb.Min.x + (w - ts.x) * 0.5f, bb.Min.y + (rh - ts.y) * 0.5f),
                            ImGui::GetColorU32(ImGuiCol_Text), label);
                return fires;
            };
            
            // PAGE 1: LETTERS
            static const OSKChar letters1[] = {
                {"q", 'q'}, {"w", 'w'}, {"e", 'e'}, {"r", 'r'}, {"t", 't'},
                {"y", 'y'}, {"u", 'u'}, {"i", 'i'}, {"o", 'o'}, {"p", 'p'}
            };
            static const OSKChar letters2[] = {
                {"a", 'a'}, {"s", 's'}, {"d", 'd'}, {"f", 'f'}, {"g", 'g'},
                {"h", 'h'}, {"j", 'j'}, {"k", 'k'}, {"l", 'l'}
            };
            static const OSKChar letters3[] = {
                {"z", 'z'}, {"x", 'x'}, {"c", 'c'}, {"v", 'v'},
                {"b", 'b'}, {"n", 'n'}, {"m", 'm'}
            };

            // PAGE 2: NUMBERS AND SYMBOLS
            static const OSKChar symbols1[] = {
                {"1",'1'}, {"2",'2'}, {"3",'3'}, {"4",'4'}, {"5",'5'},
                {"6",'6'}, {"7",'7'}, {"8",'8'}, {"9",'9'}, {"0",'0'}
            };
            static const OSKChar symbols2[] = {
                {"@",'@'}, {"#",'#'}, {"$",'$'}, {"_",'_'}, {"&",'&'},
                {"-",'-'}, {"+",'+'}, {"(",'('}, {")",')'}, {"/",'/'}
            };
            static const OSKChar symbols3[] = {
                {"*",'*'}, {"\"",'\"'}, {"'",'\''}, {":",':'},
                {";",';'}, {"!",'!'}, {"?",'?'}
            };
            
            // PAGE 3: ADDITIONAL SYMBOLS (not all, some are broken... sad)
            static const OSKChar more1[] = {
                {"~",'~'}, {"`",'`'}, {"|",'|'}, {"•",0x2022},
                {"√",0x221A}, {"π",0x03C0}, {"÷",0x00F7},
                {"×",0x00D7}, {"§",0x00A7}, {"∆",0x2206}
            };
            static const OSKChar more2[] = {
                {"£",0x00A3}, {"¢",0x00A2}, {"€",0x20AC},
                {"¥",0x00A5}, {"^",'^'}, {"°",0x00B0},
                {"=",'='}, {"{",'{'}, {"}",'}'}, {"\\",'\\'}
            };
            static const OSKChar more3[] = {
                {"%",'%'}, {"©",0x00A9}, {"®",0x00AE},
                {"™",0x2122}, {"✓",0x2713}, {"[",'['}, {"]",']'}
            };
            
            // Draw a centered row of keys.
            auto typeRow = [&](const OSKChar* chars, int n, bool centered)
            {
                if (centered)
                    ImGui::SetCursorPosX(pad.x + (W - (n * u + (n - 1) * sp)) * 0.5f);
                
                for (int i = 0; i < n; i++)
                {
                    if (i) ImGui::SameLine(0.0f, sp);
                    char upper[2] = {0, 0};
                    const char* label = chars[i].label;
                    unsigned int code = chars[i].code;
            
                    if (s.page == 0 && (s.shift || s.caps))
                    {
                        upper[0] = (char)toupper((unsigned char)code);
                        label = upper;
                        code = (unsigned int)upper[0];
                    }
                    if (key(label, u, false, false))
                    {
                        PushChar(code);
            
                        // Only consume temporary Shift.
                        // Caps Lock remains enabled.
                        if (!s.caps)
                        {
                            s.shift = false;
                            s.lastShiftTap = -1.0f;
                        }
                    }
                }
            };

            // ROW 1
            if (s.page == 0)
                typeRow(letters1, IM_ARRAYSIZE(letters1), true);
            else if (s.page == 1)
                typeRow(symbols1, IM_ARRAYSIZE(symbols1), true);
            else
                typeRow(more1, IM_ARRAYSIZE(more1), true);
            
            // ROW 2
            if (s.page == 0)
                typeRow(letters2, IM_ARRAYSIZE(letters2), true);
            else if (s.page == 1)
                typeRow(symbols2, IM_ARRAYSIZE(symbols2), true);
            else
                typeRow(more2, IM_ARRAYSIZE(more2), true);
            
            // ROW 3
            const float side = (W - 7.0f * u - 8.0f * sp) * 0.5f;

            // LEFT SIDE: SHIFT / NEXT SYMBOLS / ABC
            ImGui::SetCursorPosX(pad.x);
            if (s.page == 0)
            {
                const char* shiftLabel = s.caps ? "SHIFT" : "Shift";
                if (key(shiftLabel, side, false, s.shift || s.caps))
                {
                    const float now = (float)ImGui::GetTime();
                    if (s.caps)
                    {
                        // Caps Lock on -> off
                        s.caps = false;
                        s.shift = false;
                        s.lastShiftTap = -1.0f;
                    }
                    else if (s.shift)
                    {
                        // 230ms delay double-tap to match my goon keyboard
                        if (s.lastShiftTap >= 0.0f && now - s.lastShiftTap <= 0.230f)
                        {
                            // Quick second tap -> Caps Lock on
                            s.caps = true;
                            s.shift = false;
                        }
                        else
                        {
                            // Slower second tap -> cancel the one-shot Shift
                            s.shift = false;
                        }
                        s.lastShiftTap = -1.0f;
                    }
                    else
                    {
                        // Off -> one-shot Shift
                        s.shift = true;
                        s.lastShiftTap = now;
                    }
                }
            }
            else if (s.page == 1)
            {
                // Symbols -> More Symbols
                if (key("=\\<", side, false, false))
                    s.page = 2;
            }
            else
            {
                // More Symbols -> Symbols
                if (key("?123", side, false, false))
                    s.page = 1;
            }
            
            // Seven keys in the third row.
            ImGui::SameLine(0.0f, sp);
            if (s.page == 0)
                typeRow(letters3, IM_ARRAYSIZE(letters3), false);
            else if (s.page == 1)
                typeRow(symbols3, IM_ARRAYSIZE(symbols3), false);
            else
                typeRow(more3, IM_ARRAYSIZE(more3), false);
            
            // BACKSPACE
            ImGui::SameLine(0.0f, sp);
            for (int n = key("Back", side, true, false); n > 0; n--) PushKey(ImGuiKey_Backspace);
            
            // BOTTOM ROW
            const float navW = u * 1.6f;
            const float commaW = u;
            const float dotW = u;
            const float enterW = u * 1.5f;
            const float spaceW = W - navW - commaW - dotW - enterW - sp * 4.0f;
            
            // PAGE NAVIGATION
            ImGui::SetCursorPosX(pad.x);
            if (s.page == 0)
            {
                if (key("?123", navW, false, false))
                {
                    s.page = 1;
                    s.shift = false;
                }
            }
            else
            {
                if (key("ABC", navW, false, false))
                {
                    s.page = 0;
                    s.shift = false;
                }
            }
            ImGui::SameLine(0.0f, sp);
            if (key(",", commaW, false, false)) PushChar(',');
            ImGui::SameLine(0.0f, sp);
            if (key("Space", spaceW, false, false)) PushChar(' ');
            ImGui::SameLine(0.0f, sp);
            if (key(".", dotW, false, false)) PushChar('.');
            ImGui::SameLine(0.0f, sp);
            if (key("Enter", enterW, false, false)) PushChar('\n');
        }
        ImGui::End();
    }
}
