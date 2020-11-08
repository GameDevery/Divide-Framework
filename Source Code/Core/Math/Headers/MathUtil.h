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

/*Code references:
    Matrix inverse: http://www.devmaster.net/forums/showthread.php?t=14569
    Matrix multiply:
   http://fhtr.blogspot.com/2010/02/4x4-float-matrix-multiplication-using.html
    Square root: http://www.codeproject.com/KB/cpp/Sqrt_Prec_VS_Speed.aspx
*/

#pragma once
#ifndef _CORE_MATH_MATH_UTIL_H_
#define _CORE_MATH_MATH_UTIL_H_

#include "MathVectors.h"

namespace Divide {
namespace Util {

struct NOINITVTABLE GraphPlot {
    explicit GraphPlot(const stringImpl& name) : _plotName(name)
    {
    }
    virtual ~GraphPlot() = default;

    stringImpl _plotName;
    [[nodiscard]] virtual bool empty() const noexcept = 0;
};

struct GraphPlot2D final : GraphPlot {
    GraphPlot2D() : GraphPlot2D("UNNAMED_PLOT_2D")
    {
    }

    explicit GraphPlot2D(const stringImpl& name) : GraphPlot(name)
    {
    }

    vectorEASTL<vec2<F32>> _coords;
     
    [[nodiscard]] bool empty() const noexcept override {
        return _coords.empty();
    }
};

struct GraphPlot3D final : GraphPlot {
    GraphPlot3D() : GraphPlot3D("UNNAMED_PLOT_3D")
    {
    }

    explicit GraphPlot3D(const stringImpl& name) : GraphPlot(name)
    {
    }

    vectorEASTL<vec3<F32>> _coords;

    [[nodiscard]] bool empty() const noexcept override {
        return _coords.empty();
    }
};

};  // namespace Util
};  // namespace Divide
#endif