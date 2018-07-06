/*
   Copyright (c) 2014 DIVIDE-Studio
   Copyright (c) 2009 Ionut Cava

   This file is part of DIVIDE Framework.

   Permission is hereby granted, free of charge, to any person obtaining a copy of this software
   and associated documentation files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef _FRAME_LISTENER_H_
#define _FRAME_LISTENER_H_

#include "Platform/DataTypes/Headers/PlatformDefines.h"

/// As you might of guessed it, it's the same system used in Ogre3D
/// (http://www.ogre3d.org/docs/api/html/OgreFrameListener_8h_source.html)
/// I decided to use something that people already know and are comfortable with
///-Ionut

namespace Divide {

enum FrameEventType {
    FRAME_EVENT_ANY,
    FRAME_EVENT_STARTED,
    FRAME_PRERENDER_START,
    FRAME_PRERENDER_END,
    FRAME_POSTRENDER_START,
    FRAME_POSTRENDER_END,
    FRAME_EVENT_PROCESS,
    FRAME_EVENT_ENDED,
};

struct FrameEvent {
    D32 _timeSinceLastEvent;
    D32 _timeSinceLastFrame;
    D32 _currentTime;
    FrameEventType _type;
};

/// FrameListener class.
/// Has 3 events, associated with the start of rendering a frame, 
/// the end of rendering and the end of buffer swapping after frames
/// All events have timers associated with them for update timing
class FrameListener{
public:
    ///Either give it a name
    FrameListener(const stringImpl& name) : _callOrder(0) { _listenerName = name; }
    ///Or the frame listenr manager will assing it an ID
    FrameListener() : _callOrder(0) {}
    virtual ~FrameListener(){}
    inline const stringImpl& getName() const {return _listenerName;}

    bool operator<(FrameListener& that){
        return this->_callOrder < that._callOrder;
    }

protected:
    friend class FrameListenerManager;
    inline void  setName(const stringImpl& name) { _listenerName = name; }
    inline void  setCallOrder(U32 order)          { _callOrder = order; }
    ///Adapter patern instead of pure interface for the same reason as the Ogre boys pointed out:
    ///Implement what you need without filling classes with dummy functions
    ///frameStarted is calld at the beggining of a new frame before processing the logic aspect of a scene
    virtual bool frameStarted(const FrameEvent& evt) {return true;}
    ///framePreRenderStarted is called when we need to start processing the visual aspect of a scene
    virtual bool framePreRenderStarted(const FrameEvent& evt) {return true;}
    ///framePreRenderEnded is called after all the prerendering has finished and rendering should start
    virtual bool framePreRenderEnded(const FrameEvent& evt) {return true;}
    ///frameRendering Queued is called after all the frame setup/rendering but before the call to SwapBuffers
    virtual bool frameRenderingQueued(const FrameEvent& evt) {return true;}
    ///framePostRenderStarted is called after the main rendering calls are finished (e.g. use this for debug calls)
    virtual bool framePostRenderStarted(const FrameEvent& evt) {return true;}
    ///framePostRenderEnded is called after all the postrendering has finished
    virtual bool framePostRenderEnded(const FrameEvent& evt) { return true; }
    ///frameEnded is called after the buffers have been swapped
    virtual bool frameEnded(const FrameEvent& evt) {return true;}
    
private:
    ///not _name so that it doesn't conflict with Resource base class
    stringImpl _listenerName;
    ///if multiple frame listeners are handling the same event, this call order variable is used for sorting
    U32 _callOrder;
};

}; //namespace Divide
#endif