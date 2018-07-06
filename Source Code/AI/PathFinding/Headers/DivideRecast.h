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

/*
    OgreCrowd
    ---------

    Copyright (c) 2012 Jonas Hauquier

    Additional contributions by:

    - mkultra333
    - Paul Wilson

    Sincere thanks and to:

    - Mikko Mononen (developer of Recast navigation libraries)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

*/

#ifndef _NAVIGATION_PATH_H_
#define _NAVIGATION_PATH_H_

#include "core.h"
#include "../NavMeshes/Headers/NavMesh.h"

namespace AI {

namespace Navigation {

    DEFINE_SINGLETON( DivideRecast )
        public:
            
           /**
            * Find a path beween start point and end point and, if possible, generates a list of lines in a path.
            * It might fail if the start or end points aren't near any navmesh polygons, or if the path is too long,
            * or it can't make a path, or various other reasons.
            *
            * pathSlot: The index number for the slot in which the found path is to be stored.
            * target: Number identifying the target the path leads to. Recast does nothing with this, but you can give them
            *   meaning in your own application.
            *
            * Return codes:
            *  PATH_ERROR_NONE                    Found path
            *  PATH_ERROR_NO_NEAREST_POLY         Couldn't find polygon nearest to start point
            *  PATH_ERROR_NO_NEAREST_POLY_END     Couldn't find polygon nearest to end point
            *  PATH_ERROR_COULD_NOT_CREATE_PATH   Couldn't create a path
            *  PATH_ERROR_COULD_NOT_FIND_PATH     Couldn't find a path
            *  PATH_ERROR_NO_STRAIGHT_PATH_CREATE Couldn't create a straight path
            *  PATH_ERROR_NO_STRAIGHT_PATH_FIND   Couldn't find a straight path
           **/
            PathErrorCode FindPath(const NavigationMesh& navMesh,
                                   const vec3<F32>& startPos, 
                                   const vec3<F32>& endPos, 
                                   I32 pathSlot,
                                   I32 target);
            /**
            * Retrieve the path at specified slot defined as a line along an ordered set of 3D positions.
            * The path has a maximum length of MAX_PATHVERT, and is an empty list in case no path is
            * defined or an invalid pathSlot index is given.
            **/
            vectorImpl<vec3<F32> > getPath(I32 pathSlot);
            /**
            * The ID number identifying the target for the path at specified slot. Targets have
            * no meaning for OgreRecast but you can use them to give them their own meanings.
            * Returns 0 when a faulty pathSlot is given.
            **/
            I32 getTarget(I32 pathSlot);
            /**
            * Returns a random point on the navmesh.
            **/
            vec3<F32> getRandomNavMeshPoint(const NavigationMesh& navMesh);
            /**
            * Returns a random point on the navmesh contained withing the specified circle
            **/
            vec3<F32> getRandomPointAroundCircle(const NavigationMesh& navMesh, const vec3<F32>& centerPosition, F32 radius);
            /**
            * Find a point on the navmesh closest to the specified point position, within predefined
            * bounds.
            * Returns true if such a point is found (returned as resultPt), returns false
            * if no point is found. When false is returned, resultPt is not altered.
            **/
            bool findNearestPointOnNavmesh(const NavigationMesh& navMesh, const vec3<F32>& position, vec3<F32>& resultPt);
            bool findNearestPolyOnNavmesh(const NavigationMesh& navMesh, const vec3<F32>& position, vec3<F32>& resultPt, dtPolyRef &resultPoly);

        protected:
            DivideRecast();
            ~DivideRecast();

        protected:
            /// Stores all created paths
            PATHDATA _pathStore[MAX_PATHSLOT];
            /// The poly filter that will be used for all (random) point and nearest poly searches.
            dtQueryFilter* _filter;

    END_SINGLETON

};

};
#endif 