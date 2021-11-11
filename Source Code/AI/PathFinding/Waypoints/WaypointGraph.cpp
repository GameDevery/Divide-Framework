#include "stdafx.h"

#include "Headers/WaypointGraph.h"

#include "Utility/Headers/Localization.h"

namespace Divide::Navigation {

void WaypointGraph::addWaypoint(Waypoint* wp) {
    if (_waypoints.find(wp->ID()) != std::end(_waypoints)) {
        return;
    }

    insert(_waypoints, wp->ID(), wp);
    updateGraph();
}

void WaypointGraph::removeWaypoint(Waypoint* wp) {
    if (_waypoints.find(wp->ID()) != std::end(_waypoints)) {
        _waypoints.erase(wp->ID());
        updateGraph();
    } else {
        Console::printfn(Locale::Get(_ID("WARN_WAYPOINT_NOT_FOUND")), wp->ID(),
                         getID());
    }
}

void WaypointGraph::updateGraph() {
    _positions.resize(0);
    _rotations.resize(0);
    _times.resize(0);
    for (auto& waypoint : _waypoints) {
        _positions.push_back(waypoint.second->position());
        _rotations.push_back(waypoint.second->orientation());
        _times.push_back(waypoint.second->time());
    }
}

}  // namespace Divide::Navigation
