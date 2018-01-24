#pragma once

#include <queue>
#include <algorithm>

#include "location.hpp"
#include "types.hpp"

namespace hlt {
    struct vel {
        long double vel_x = 0.000001, vel_y = 0.000001;

        void accelerate_by(double magnitude, double angle) {
            vel_x = vel_x + magnitude * std::cos(angle);
            vel_y = vel_y + magnitude * std::sin(angle);

            const auto max_speed = constants::MAX_SPEED;
            if (this->magnitude() > max_speed) {
                double scale = max_speed / this->magnitude();
                vel_x *= scale;
                vel_y *= scale;
            }
        }

        const long double magnitude() {
            return sqrt(vel_x * vel_x + vel_y * vel_y);
        }
    };

    struct NearbyEntity {
        long double distance;

        bool is_ship;
        EntityId entity_id;
        PlayerId owner_id;

        bool operator<(const NearbyEntity& other_entity) const {
            return distance > other_entity.distance;
        }

        // sorts entitys by distance
        static bool sort_by_distance (
            const NearbyEntity& entity1,
            const NearbyEntity& entity2
        ) {
            return (entity1.distance < entity2.distance);
        }
    };

    struct Entity {
        bool has_sorted_already = false;

        vel velocity;

        EntityId entity_id;
        PlayerId owner_id;
        Location location;
        int health;
        double radius;

        std::priority_queue<NearbyEntity> priority_targets;

        std::vector<NearbyEntity> nearby_owned_ships;
        std::vector<NearbyEntity> nearby_owned_and_docked_ships;

        std::vector<NearbyEntity> nearby_enemy_ships;
        std::vector<NearbyEntity> nearby_owned_and_docked_enemy_ships;


        std::vector<NearbyEntity> nearby_planets;

        void sort_nearby_entitys () {
            // sorts all of our NearbyEntity's by distance
            // only should be ran when being used to save on cpu
            if (!has_sorted_already) {
                std::sort(nearby_owned_ships.begin(), nearby_owned_ships.end(), NearbyEntity::sort_by_distance);
                std::sort(nearby_owned_and_docked_ships.begin(), nearby_owned_and_docked_ships.end(), NearbyEntity::sort_by_distance);
                std::sort(nearby_enemy_ships.begin(), nearby_enemy_ships.end(), NearbyEntity::sort_by_distance);
                std::sort(nearby_owned_and_docked_enemy_ships.begin(), nearby_owned_and_docked_enemy_ships.end(), NearbyEntity::sort_by_distance);
                std::sort(nearby_planets.begin(), nearby_planets.end(), NearbyEntity::sort_by_distance);
            }
            has_sorted_already = true;
        }

        bool is_alive() const {
            return health > 0;
        }
    };
}
