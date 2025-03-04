/*
   Copyright (c) 2018 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software
   and associated documentation files (the "Software"), to deal in the Software
   without restriction,
   including without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once
#ifndef DVD_GUI_MESSAGE_BOX_H_
#define DVD_GUI_MESSAGE_BOX_H_

#include "GUIElement.h"
#include "GUIText.h"

namespace CEGUI {
class Window;
class Font;
class EventArgs;
};

namespace Divide {

class GUIMessageBox final : public GUIElementBase<GUIType::GUI_MESSAGE_BOX> {
    friend class GUI;
    friend class GUIInterface;
    friend class SceneGUIElements;

   public:
    enum class MessageType : U8 {
        MESSAGE_INFO = 0,
        MESSAGE_WARNING = 1,
        MESSAGE_ERROR = 2
    };
    virtual ~GUIMessageBox() override;

    bool onConfirm(const CEGUI::EventArgs& /*e*/) noexcept;
    void setTitle(std::string_view titleText);
    void setMessage(std::string_view message);
    void setOffset(int2 offsetFromCentre);
    void setMessageType(MessageType type);

    void show() noexcept {
        active(true);
        visible(true);
    }

    GUIMessageBox(std::string_view name,
                  std::string_view title,
                  std::string_view message,
                  int2 offsetFromCentre = int2(0),
                  CEGUI::Window* parent = nullptr);

   protected:
    void visible(bool visible) noexcept override;
    void active(bool active) noexcept override;

   protected:
    int2 _offsetFromCentre;
    CEGUI::Window* _msgBoxWindow;
    CEGUI::Event::Connection _confirmEvent;
};

};  // namespace Divide

#endif //DVD_GUI_MESSAGE_BOX_H_
