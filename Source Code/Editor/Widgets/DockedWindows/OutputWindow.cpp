#include "stdafx.h"

#include "Headers/OutputWindow.h"
#include "Core/Headers/StringHelper.h"
#include "Editor/Headers/Editor.h"

namespace Divide {
    constexpr U16 g_maxLogEntries = 512;

    std::atomic_size_t g_writeIndex = 0;
    std::array<Console::OutputEntry, g_maxLogEntries> g_log;

    OutputWindow::OutputWindow(Editor& parent, const Descriptor& descriptor)
        : DockedWindow(parent, descriptor),
         _scrollToBottom(true)
    {
        memset(_inputBuf, 0, sizeof _inputBuf);

        std::atomic_init(&g_writeIndex, 0);
        _consoleCallbackIndex = Console::bindConsoleOutput([this](const Console::OutputEntry& entry) {
            printText(entry);
        });
    }

    OutputWindow::~OutputWindow()
    {
        Console::unbindConsoleOutput(_consoleCallbackIndex);
        clearLog();
    }


    void OutputWindow::clearLog() {
        g_log.fill({ "", Console::EntryType::INFO });
        g_writeIndex.store(0);
        _scrollToBottom = true;
    }

    void OutputWindow::drawInternal() {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

        if (ImGui::SmallButton("Clear")) {
            clearLog();
        }
        ImGui::SameLine();

        const bool copy_to_clipboard = ImGui::SmallButton("Copy");
        ImGui::SameLine();
        if (ImGui::Checkbox("Scroll to bottom", &_scrollToBottom)) {
            
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        _filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
        ImGui::PopStyleVar();
        ImGui::Separator();

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable("Clear")) {
                clearLog();
            }
            ImGui::EndPopup();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

        if (copy_to_clipboard) {
            ImGui::LogToClipboard();
        }

        static ImVec4 colours[] = {
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
            ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            ImVec4(0.0f, 0.0f, 1.0f, 1.0f)
        };

        const size_t start = g_writeIndex.load();
        for (U16 i = 0; i < g_maxLogEntries;  ++i) {
            const size_t index = (start + 1 + i) % g_maxLogEntries;
            const Console::OutputEntry& message = g_log[index];
            if (!_filter.PassFilter(message._text.c_str())) {
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, colours[to_U8(message._type)]);
            ImGui::TextUnformatted(message._text.c_str());
            ImGui::PopStyleColor();
        }
        
        if (copy_to_clipboard) {
            ImGui::LogFinish();
        }
        if (_scrollToBottom) {
            ImGui::SetScrollHere();
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::Separator();

        if (ImGui::InputText("Input",
                             _inputBuf,
                             IM_ARRAYSIZE(_inputBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                             [](ImGuiTextEditCallbackData* data){
                                OutputWindow* console = static_cast<OutputWindow*>(data->UserData);
                                return console->textEditCallback(data);
                             },
                             (void*)this))
        {
            char* input_end = _inputBuf + strlen(_inputBuf);
            while (input_end > _inputBuf && input_end[-1] == ' ') {
                input_end--;
            }
            *input_end = 0;

            if (_inputBuf[0]) {
                executeCommand(_inputBuf);
            }
            strcpy(_inputBuf, "");
        }

        // Demonstrate keeping auto focus on the input box
        if (ImGui::IsItemHovered() || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
        }
        ImGui::PopStyleColor();
    }

    void OutputWindow::printText(const Console::OutputEntry& entry) {
        g_log[g_writeIndex.fetch_add(1) % g_maxLogEntries] = entry;
    }

    void OutputWindow::executeCommand(const char* command_line) {
        printText(
            {
                Util::StringFormat("# %s\n", command_line),
                Console::EntryType::COMMAND
            }
        );
    }

    I32 OutputWindow::textEditCallback(ImGuiTextEditCallbackData* data) {
        switch (data->EventFlag)
        {
            case ImGuiInputTextFlags_CallbackCompletion:
            case ImGuiInputTextFlags_CallbackHistory:
            default: break;
        }

        return -1;
    }
}