
#include "carla/trafficmanager/TrackTraffic.h"

#define BUFFER_STEP_THROUGH 10

namespace carla
{
namespace traffic_manager
{

TrackTraffic::TrackTraffic() {}

void TrackTraffic::UpdateUnregisteredGridPosition(const ActorId actor_id,
                                                  const std::vector<SimpleWaypointPtr> waypoints)
{

    DeleteActor(actor_id);

    actor_to_grids.insert({actor_id, {}});
    std::unordered_set<GeoGridId> &current_grids = actor_to_grids.at(actor_id);
    // Step through waypoints and update grid list for actor and actor list for grids.
    for (auto &waypoint : waypoints)
    {
        UpdatePassingVehicle(waypoint->GetId(), actor_id);

        GeoGridId ggid = waypoint->GetGeodesicGridId();
        current_grids.insert(ggid);

        if (grid_to_actors.find(ggid) != grid_to_actors.end())
        {
            ActorIdSet &actor_ids = grid_to_actors.at(ggid);
            if (actor_ids.find(actor_id) == actor_ids.end())
            {
                actor_ids.insert(actor_id);
            }
        }
        else
        {
            grid_to_actors.insert({ggid, {actor_id}});
        }
        
    }
}

void TrackTraffic::UpdateGridPosition(const ActorId actor_id, const Buffer &buffer)
{
    if (!buffer.empty())
    {
        // Add actor entry, if not present.
        if (actor_to_grids.find(actor_id) == actor_to_grids.end())
        {
            actor_to_grids.insert({actor_id, {}});
        }

        std::unordered_set<GeoGridId> &current_grids = actor_to_grids.at(actor_id);

        // Clear current actor from all grids containing itself.
        for (auto &grid_id : current_grids)
        {
            if (grid_to_actors.find(grid_id) != grid_to_actors.end())
            {
                ActorIdSet &actor_ids = grid_to_actors.at(grid_id);
                if (actor_ids.find(actor_id) != actor_ids.end())
                {
                    actor_ids.erase(actor_id);
                }
            }
        }

        // Clear all grids the current actor is tracking.
        current_grids.clear();

        // Step through buffer and update grid list for actor and actor list for grids.
        uint64_t buffer_size = buffer.size();
        uint64_t step_size = static_cast<uint64_t>(std::floor(buffer_size / BUFFER_STEP_THROUGH));
        for (uint64_t i = 0u; i <= BUFFER_STEP_THROUGH; ++i)
        {
            GeoGridId ggid = buffer.at(std::min(i * step_size, buffer_size - 1u))->GetGeodesicGridId();
            current_grids.insert(ggid);

            // Add grid entry if not present.
            if (grid_to_actors.find(ggid) == grid_to_actors.end())
            {
                grid_to_actors.insert({ggid, {}});
            }

            ActorIdSet &actor_ids = grid_to_actors.at(ggid);
            if (actor_ids.find(actor_id) == actor_ids.end())
            {
                actor_ids.insert(actor_id);
            }
        }
    }
}

ActorIdSet TrackTraffic::GetOverlappingVehicles(ActorId actor_id) const
{
    ActorIdSet actor_id_set;

    if (actor_to_grids.find(actor_id) != actor_to_grids.end())
    {
        const std::unordered_set<GeoGridId> &grid_ids = actor_to_grids.at(actor_id);
        for (auto &grid_id : grid_ids)
        {
            if (grid_to_actors.find(grid_id) != grid_to_actors.end())
            {
                const ActorIdSet &actor_ids = grid_to_actors.at(grid_id);
                actor_id_set.insert(actor_ids.begin(), actor_ids.end());
            }
        }
    }

    return actor_id_set;
}

void TrackTraffic::DeleteActor(ActorId actor_id)
{
    if (actor_to_grids.find(actor_id) != actor_to_grids.end())
    {
        std::unordered_set<GeoGridId> &grid_ids = actor_to_grids.at(actor_id);
        for (auto &grid_id : grid_ids)
        {
            if (grid_to_actors.find(grid_id) != grid_to_actors.end())
            {
                ActorIdSet &actor_ids = grid_to_actors.at(grid_id);
                if (actor_ids.find(actor_id) != actor_ids.end())
                {
                    actor_ids.erase(actor_id);
                }
            }
        }
        actor_to_grids.erase(actor_id);
    }

    if (waypoint_occupied.find(actor_id) != waypoint_occupied.end())
    {
        WaypointIdSet waypoint_id_set = waypoint_occupied.at(actor_id);
        for (const uint64_t &waypoint_id : waypoint_id_set)
        {
            RemovePassingVehicle(waypoint_id, actor_id);
        }
    }
}

std::unordered_set<GeoGridId> TrackTraffic::GetGridIds(ActorId actor_id) const
{
    std::unordered_set<GeoGridId> grid_ids;

    if (actor_to_grids.find(actor_id) != actor_to_grids.end())
    {
        grid_ids = actor_to_grids.at(actor_id);
    }

    return grid_ids;
}

std::unordered_map<GeoGridId, ActorIdSet> TrackTraffic::GetGridActors() const
{
    return grid_to_actors;
}

void TrackTraffic::UpdatePassingVehicle(uint64_t waypoint_id, ActorId actor_id)
{
    if (waypoint_overlap_tracker.find(waypoint_id) != waypoint_overlap_tracker.end())
    {
        ActorIdSet &actor_id_set = waypoint_overlap_tracker.at(waypoint_id);
        if (actor_id_set.find(actor_id) == actor_id_set.end())
        {
            actor_id_set.insert(actor_id);
        }
    }
    else
    {
        waypoint_overlap_tracker.insert({waypoint_id, {actor_id}});
    }

    if (waypoint_occupied.find(actor_id) != waypoint_occupied.end())
    {
        WaypointIdSet &waypoint_id_set = waypoint_occupied.at(actor_id);
        if (waypoint_id_set.find(waypoint_id) == waypoint_id_set.end())
        {
            waypoint_id_set.insert(waypoint_id);
        }
    }
    else
    {
        waypoint_occupied.insert({actor_id, {waypoint_id}});
    }
}

void TrackTraffic::RemovePassingVehicle(uint64_t waypoint_id, ActorId actor_id)
{
    if (waypoint_overlap_tracker.find(waypoint_id) != waypoint_overlap_tracker.end())
    {
        ActorIdSet &actor_id_set = waypoint_overlap_tracker.at(waypoint_id);
        if (actor_id_set.find(actor_id) != actor_id_set.end())
        {
            actor_id_set.erase(actor_id);
        }

        if (actor_id_set.size() == 0)
        {
            waypoint_overlap_tracker.erase(waypoint_id);
        }
    }

    if (waypoint_occupied.find(actor_id) != waypoint_occupied.end())
    {
        WaypointIdSet &waypoint_id_set = waypoint_occupied.at(actor_id);
        if (waypoint_id_set.find(waypoint_id) != waypoint_id_set.end())
        {
            waypoint_id_set.erase(waypoint_id);
        }

        if (waypoint_id_set.size() == 0)
        {
            waypoint_occupied.erase(actor_id);
        }
    }
}

ActorIdSet TrackTraffic::GetPassingVehicles(uint64_t waypoint_id) const
{

    if (waypoint_overlap_tracker.find(waypoint_id) != waypoint_overlap_tracker.end())
    {
        return waypoint_overlap_tracker.at(waypoint_id);
    }
    else
    {
        return ActorIdSet();
    }
}

void TrackTraffic::Clear()
{
    waypoint_overlap_tracker.clear();
    waypoint_occupied.clear();
    actor_to_grids.clear();
    grid_to_actors.clear();
}

} // namespace traffic_manager
} // namespace carla
