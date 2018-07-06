/*�Copyright 2009-2013 DIVIDE-Studio�*/
/* This file is part of DIVIDE Framework.

   DIVIDE Framework is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   DIVIDE Framework is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with DIVIDE Framework.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GUI_BUTTON_H_
#define _GUI_BUTTON_H_

#include "GUIElement.h"
#include "GUIText.h"
#include <boost/function.hpp>

class GUIButton : public GUIElement {

typedef boost::function0<void> ButtonCallback;
friend class GUI;
public:
	GUIButton(const std::string& id,
			  const std::string& text,
			  const vec2<F32>& position,
			  const vec2<F32>& dimensions,
			  const vec3<F32>& color
			  /*, Texture2D& image*/,
			  ButtonCallback callback)  : GUIElement(GUI_BUTTON,position),
		_text(text),
		_dimensions(dimensions),
		_color(color),
		_callbackFunction(callback),
		_highlight(false),
		_pressed(false),
		_font(Font::DIVIDE_DEFAULT),
		_fontHeight(16),
		_guiText(NULL)
		/*_image(image)*/{
			setActive(true);
	}
	~GUIButton() {SAFE_DELETE(_guiText);}
	std::string _text;
	std::string _font;
	U8          _fontHeight;
	vec2<F32>	_dimensions;
	vec3<F32>	_color;
	bool		_pressed;
	bool		_highlight;
	ButtonCallback _callbackFunction;	/* A pointer to a function to call if the button is pressed */
	GUIText*    _guiText; ///<Cached text element to display over the button rectangle
	//Texture2D image;

	void onResize(const vec2<I32>& newSize){
		if(_dimensions.x - newSize.x/_dimensions.x > 0.075f &&
		   _dimensions.y - newSize.y/_dimensions.y > 0.05f){
			_position.x -= newSize.x;
			_position.y -= newSize.y;
			_dimensions.x -= newSize.x/_dimensions.x; 
			_dimensions.y -= newSize.y/_dimensions.y; 
		}
	}
    void onMouseMove(const GUIEvent &event);
    void onMouseUp(const GUIEvent &event);
    void onMouseDown(const GUIEvent &event);
/*  void onRightMouseUp(const GUIEvent &event);
    void onRightMouseDown(const GUIEvent &event);
    bool onKeyUp(const GUIEvent &event);
    bool onKeyDown(const GUIEvent &event);
*/
};
#endif