#include "stdafx.h"

#include "Headers/FileUpdateMonitor.h"
#include "Platform/File/Headers/FileManagement.h"

namespace Divide {

UpdateListener::UpdateListener(FileUpdateCbk&& cbk)
    : _cbk(std::move(cbk))
{
}

void UpdateListener::addIgnoredExtension(const char* extension) {
    _ignoredExtensions.emplace_back(extension);
}

void UpdateListener::addIgnoredEndCharacter(char character) {
    _ignoredEndingCharacters.emplace_back(character);
}

void UpdateListener::handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename, const FW::Action action)
{
    ACKNOWLEDGE_UNUSED(watchid);
    ACKNOWLEDGE_UNUSED(dir);

    // We can ignore files that end in a specific character. Many text editors, for example, append a '~' at the end of temp files
    if (!_ignoredEndingCharacters.empty()) {
        if (eastl::find_if(eastl::cbegin(_ignoredEndingCharacters),
            eastl::cend(_ignoredEndingCharacters),
            [filename](const char character) noexcept {
            return std::tolower(filename.back()) == std::tolower(character);
        }) != eastl::cend(_ignoredEndingCharacters)) {
            return;
        }
    }

    // We can specify a list of extensions to ignore for a specific listener to avoid, for example, parsing temporary OS files
    if (!_ignoredExtensions.empty()) {
        if (eastl::find_if(eastl::cbegin(_ignoredExtensions),
            eastl::cend(_ignoredExtensions),
            [filename](const Str8& extension) {
            return hasExtension(filename.c_str(), extension);
        }) != eastl::cend(_ignoredExtensions)) {
            return;
        }
    }
    if (_cbk) {
        FileUpdateEvent evt = FileUpdateEvent::COUNT;
        switch (action)
        {
        case FW::Actions::Add: evt = FileUpdateEvent::ADD; break;
        case FW::Actions::Delete: evt = FileUpdateEvent::DELETE; break;
        case FW::Actions::Modified: evt = FileUpdateEvent::MODIFY; break;
        default: DIVIDE_UNEXPECTED_CALL("Unknown file event!");
        }

        _cbk(filename.c_str(), evt);
    }
}

}; //namespace Divide;