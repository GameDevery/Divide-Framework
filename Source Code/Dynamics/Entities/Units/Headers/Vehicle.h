/*
   Copyright (c) 2015 DIVIDE-Studio
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

#ifndef _VEHICLE_H_
#define _VEHICLE_H_

#include "Unit.h"

namespace Divide {

/// Basic vehicle class
class Vehicle : public Unit {
   public:
    /// Currently supported vehicle types
    enum VehicleType {
        /// Ground based vehicles
        VEHICLE_TYPE_GROUND = toBit(1),
        /// Flying/Space vehicles
        VEHICLE_TYPE_AIR = toBit(2),
        /// Water based vehicles (ships, boats, etc)
        VEHICLE_TYPE_WATER = toBit(3),
        /// Underwater vehicles
        VEHICLE_TYPE_UNDERWATER = toBit(4),
        /// For Future expansion
        VEHICLE_TYPE_PLACEHOLDER = toBit(10)
    };

    Vehicle(SceneGraphNode& node);
    ~Vehicle();

    /// A vehicle can be of multiple types at once
    void setVehicleTypeMask(U8 mask);
    /// Check if current vehicle fits the desired type
    bool checkVehicleMask(VehicleType type) const;

   public:
    /// Is this vehicle controlled by the player or the AI?
    bool _playerControlled;
    U8 _vehicleTypeMask;
};

};  // namespace Divide

#endif