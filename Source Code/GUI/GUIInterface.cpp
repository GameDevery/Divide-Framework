#include "stdafx.h"

#include "Headers/GUIInterface.h"

#include "Headers/GUI.h"
#include "Headers/GUIFlash.h"
#include "Headers/GUIText.h"
#include "Headers/GUIButton.h"
#include "Headers/GUIConsole.h"
#include "Headers/GUIMessageBox.h"

#include "Scenes/Headers/Scene.h"

#include "Core/Headers/Kernel.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Platform/Headers/PlatformRuntime.h"

namespace Divide {

GUIInterface::GUIInterface(GUI& context)
    : _context(&context)
{
    Locale::setChangeLanguageCallback([this](const std::string_view newLanguage) { 
        onLanguageChange(newLanguage);
    });
}

GUIInterface::~GUIInterface()
{
    for (U8 i = 0; i < to_base(GUIType::COUNT); ++i) {
        for (auto [nameHash, entry] : _guiElements[i]) {
            MemoryManager::DELETE(entry.first);
        }
    }
}

void GUIInterface::onLanguageChange(std::string_view newLanguage) {
    ACKNOWLEDGE_UNUSED(newLanguage);
}

void GUIInterface::addElement(const U64 id, GUIElement* element) {
    assert(Runtime::isMainThread());

    const U8 typeIndex = to_U8(element->type());
    GUIMap& targetMap = _guiElements[typeIndex];

    const GUIMap::iterator it = targetMap.find(id);
    if (it != std::end(targetMap)) {
        MemoryManager::SAFE_UPDATE(it->second.first, element);
        it->second.second = element ? element->visible() : false;
    } else {
        insert(targetMap, id, std::make_pair(element, element ? element->visible() : false));
    }
}

GUIElement* GUIInterface::getGUIElementImpl(const U64 elementName, const GUIType type) const {
    GUIElement* ret = nullptr;
    if (type == GUIType::COUNT) {
        for (U8 i = 0; i < to_base(GUIType::COUNT); ++i) {
            const GUIMap::const_iterator it = _guiElements[i].find(elementName);
            if (it != std::cend(_guiElements[i])) {
                ret = it->second.first;
                break;
            }
        }
    } else {
        const GUIMap::const_iterator it = _guiElements[to_U32(type)].find(elementName);
        if (it != std::cend(_guiElements[to_U32(type)])) {
            ret = it->second.first;
        }
    }
    return ret;
}

GUIElement* GUIInterface::getGUIElementImpl(const I64 elementID, const GUIType type) const {
    GUIElement* ret = nullptr;
    GUIElement* element;
    if (type == GUIType::COUNT) {
        for (U8 i = 0; i < to_base(GUIType::COUNT); ++i) {
            for (const GUIMap::value_type& guiStackIterator : _guiElements[i]) {
                element = guiStackIterator.second.first;
                if (element != nullptr && element->getGUID() == elementID) {
                    ret = element;
                    break;
                }
            }
            if (ret != nullptr) {
                break;
            }
        }
    } else {
        for (const GUIMap::value_type& guiStackIterator : _guiElements[to_U32(type)]) {
            element = guiStackIterator.second.first;
            if (element != nullptr && element->getGUID() == elementID) {
                ret = element;
                break;
            }
        }
    }

    return ret;
}

GUIButton* GUIInterface::addButton(const char* name,
                                   const stringImpl& text,
                                   const RelativePosition2D& offset,
                                   const RelativeScale2D& size,
                                   const stringImpl& rootSheetID) {
    const U64 guiID = _ID(name);

    assert(getGUIElement<GUIButton>(guiID) == nullptr);

    CEGUI::Window* parent = _context->getCEGUIContext().getRootWindow();
    if (!rootSheetID.empty()) {
        parent = parent->getChild(rootSheetID.c_str());
    }

    ResourceDescriptor beepSound("buttonClick");
    beepSound.assetName(ResourcePath("beep.wav"));
    beepSound.assetLocation(Paths::g_assetsLocation + Paths::g_soundsLocation);
    const AudioDescriptor_ptr onClickSound = CreateResource<AudioDescriptor>(_context->parent().resourceCache(), beepSound);

    GUIButton* btn = MemoryManager_NEW GUIButton(name,
                                                 text,
                                                 _context->guiScheme(),
                                                 offset,
                                                 size,
                                                 parent);

    btn->setEventSound(GUIButton::Event::MouseClick, onClickSound);

    addElement(guiID, btn);

    return btn;
}

GUIMessageBox* GUIInterface::addMsgBox(const char* name,
                                       const stringImpl& title,
                                       const stringImpl& message,
                                       const vec2<I32>& offsetFromCentre) {
    const U64 guiID = _ID(name);

    assert(getGUIElement<GUIMessageBox>(guiID) == nullptr);

    GUIMessageBox* box = MemoryManager_NEW GUIMessageBox(name,
                                                         title,
                                                         message,
                                                         offsetFromCentre,
                                                         _context->rootSheet());
    addElement(guiID, box);

    return box;
}

GUIText* GUIInterface::addText(const char* name,
                               const RelativePosition2D& position,
                               const stringImpl& font,
                               const UColour4& colour,
                               const stringImpl& text,
                               const bool multiLine,
                               const U8 fontSize) {
    const U64 guiID = _ID(name);

    assert(getGUIElement<GUIText>(guiID) == nullptr);

    GUIText* t = MemoryManager_NEW GUIText(name,
                                           text,
                                           multiLine,
                                           position,
                                           font,
                                           colour,
                                           _context->rootSheet(),
                                           fontSize);
    addElement(guiID, t);

    return t;
}

GUIFlash* GUIInterface::addFlash(const char* name,
                                 const RelativePosition2D& position,
                                 const RelativeScale2D& size) {
    ACKNOWLEDGE_UNUSED(position);
    ACKNOWLEDGE_UNUSED(size);

    const U64 guiID = _ID(name);
    assert(getGUIElement<GUIFlash>(guiID) == nullptr);
    
    GUIFlash* flash = MemoryManager_NEW GUIFlash(name, _context->rootSheet());
    addElement(guiID, flash);

    return flash;
}

GUIText* GUIInterface::modifyText(const char* name, const stringImpl& text, const bool multiLine) {
    const U64 guiID = _ID(name);

    const GUIMap::iterator it = _guiElements[to_base(GUIType::GUI_TEXT)].find(guiID);

    if (it == std::cend(_guiElements[to_base(GUIType::GUI_TEXT)])) {
        return nullptr;
    }

    GUIText* textElement = static_cast<GUIText*>(it->second.first);
    assert(textElement != nullptr);

    textElement->text(text.c_str(), multiLine);

    return textElement;
}

CEGUI::Window* GUIInterface::createWindow(const CEGUI::String& type, const CEGUI::String& name) {
    CEGUI::Window* window = CEGUI::WindowManager::getSingleton().createWindow(type, name);
    if (window != nullptr) {
        _context->rootSheet()->addChild(window);
    }

    return window;
}

CEGUI::Window* GUIInterface::loadWindowFromLayoutFile(const char* layoutFileName) {
    CEGUI::Window* window = CEGUI::WindowManager::getSingleton().loadLayoutFromFile(layoutFileName);
    if (window != nullptr) {
        _context->rootSheet()->addChild(window);
    }

    return window;
}

bool GUIInterface::unloadWindow(CEGUI::Window*& window) {
    if (_context->rootSheet()->isChild(window)) {
        _context->rootSheet()->destroyChild(window);
        window = nullptr;
        return true;
    }

    return false;
}
}; //namespace Divide