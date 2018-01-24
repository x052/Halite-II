#pragma once

#include "map.hpp"
#include "types.hpp"
#include "ship.hpp"
#include "planet.hpp"

namespace hlt {
    class Map {
    public:
        int map_width, map_height;

        std::unordered_map<PlayerId, std::vector<Ship>> ships;
        std::unordered_map<PlayerId, entity_map<unsigned int>> ship_map;

        std::vector<Planet> planets;
        entity_map<unsigned int> planet_map;

        Map(int width, int height);

        Ship& get_ship(const PlayerId player_id, const EntityId ship_id) {
            return ships.at(player_id).at(ship_map.at(player_id).at(ship_id));
        }

        Planet& get_planet(const EntityId planet_id) {
            return planets.at(planet_map.at(planet_id));
        }

        void add_moving_towards (Planet& planet, Ship& ship) {
            NearbyEntity ship_entity;
            ship_entity.is_ship = true;
            ship_entity.entity_id = ship.entity_id;
            ship_entity.owner_id = ship.owner_id;
            
            planet.ships_moving_towards.push_back(ship_entity);
        }

        void add_moving_towards (Ship& end_ship, Ship& start_ship) {
            NearbyEntity ship_entity;
            ship_entity.is_ship = true;
            ship_entity.entity_id = end_ship.entity_id;
            ship_entity.owner_id = end_ship.owner_id;

            start_ship.ships_moving_towards.push_back(ship_entity);
        }
    };
}
