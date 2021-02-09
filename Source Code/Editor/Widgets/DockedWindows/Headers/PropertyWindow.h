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
#ifndef _EDITOR_PROPERTY_WINDOW_H_
#define _EDITOR_PROPERTY_WINDOW_H_

#include "Editor/Widgets/Headers/DockedWindow.h"
#include "Core/Headers/PlatformContextComponent.h"
#include "Headers/Editor.h"

namespace Divide {

class Camera;
class Texture;
class Material;
class SceneGraphNode;
class TransformComponent;

struct Selections;
struct EditorComponentField;
class PropertyWindow final : public DockedWindow, public PlatformContextComponent {
    public:
        PropertyWindow(Editor& parent, PlatformContext& context, const Descriptor& descriptor);
        ~PropertyWindow() = default;

        void drawInternal() override;
        void backgroundUpdateInternal() override;

        [[nodiscard]] const char* name() const noexcept override;
    protected:

        [[nodiscard]] bool drawCamera(Camera* cam);

        [[nodiscard]] const Selections& selections() const;
        [[nodiscard]] SceneGraphNode* node(I64 guid) const;

        //return true if the field has been modified
        [[nodiscard]] bool processField(EditorComponentField& field);
        [[nodiscard]] bool processBasicField(EditorComponentField& field) const;
        [[nodiscard]] bool processTransform(TransformComponent* transform, bool readOnly, bool hex) const;
        [[nodiscard]] bool processMaterial(Material* material, bool readOnly, bool hex);

    private:
        Texture* _previewTexture = nullptr;
};

} //namespace Divide

#endif //_EDITOR_PROPERTY_WINDOW_H_