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

#ifndef _CEGUI_INPUT_H_
#define _CEGUI_INPUT_H_
#include "Platform/Input/Headers/AutoKeyRepeat.h"
#include "Platform/Input/Headers/InputAggregatorInterface.h"

namespace Divide {

class GUI;

/// This class defines AutoRepeatKey::repeatKey(...) as CEGUI key inputs
class CEGUIInput : public Input::InputAggregatorInterface,
                   public Input::AutoRepeatKey {
   public:
    explicit CEGUIInput(GUI& parent);
    /// Key pressed: return true if input was consumed
    bool onKeyDown(const Input::KeyEvent& key);
    /// Key released: return true if input was consumed
    bool onKeyUp(const Input::KeyEvent& key);
    /// Joystick axis change: return true if input was consumed
    bool joystickAxisMoved(const Input::JoystickEvent& arg, I8 axis);
    /// Joystick direction change: return true if input was consumed
    bool joystickPovMoved(const Input::JoystickEvent& arg, I8 pov);
    /// Joystick button pressed: return true if input was consumed
    bool joystickButtonPressed(const Input::JoystickEvent& arg,
                               Input::JoystickButton button);
    /// Joystick button released: return true if input was consumed
    bool joystickButtonReleased(const Input::JoystickEvent& arg,
                                Input::JoystickButton button);
    bool joystickSliderMoved(const Input::JoystickEvent& arg, I8 index);
    bool joystickvector3Moved(const Input::JoystickEvent& arg, I8 index);
    /// Mouse moved: return true if input was consumed
    bool mouseMoved(const Input::MouseEvent& arg);
    /// Mouse button pressed: return true if input was consumed
    bool mouseButtonPressed(const Input::MouseEvent& arg,
                            Input::MouseButton button);
    /// Mouse button released: return true if input was consumed
    bool mouseButtonReleased(const Input::MouseEvent& arg,
                             Input::MouseButton button);

   protected:
    GUI& _parent;
    /// Called on key events: return true if the input was consumed
    bool injectOISKey(bool pressed, const Input::KeyEvent& inKey);
    void repeatKey(I32 inKey, U32 Char);
};

};  // namespace Divide
#endif
