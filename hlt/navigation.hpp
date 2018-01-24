#pragma once

#include "collision.hpp"
#include "map.hpp"
#include "move.hpp"
#include "util.hpp"

namespace hlt {
    namespace navigation {
        static int clip_angle (int angle) {
            return (((angle % 360L) + 360L) % 360L);
        }

        static possibly<Move> navigate_ship_towards_target(
                const Map& map,
                Ship& ship,
                const Location& target,
                const int max_thrust,
                const bool avoid_obstacles,
                const int max_corrections,
                const double angular_step_rad)
        {
            if (max_corrections <= 0) {
                return { Move::noop(), false };
            }

            const double distance = ship.location.get_distance_to(target);
            const double angle_rad = ship.location.orient_towards_in_rad(target);

            unsigned short thrust;
            if (distance < max_thrust) {
                // Do not round up, since overshooting might cause collision.
                thrust = (unsigned short) distance;
            } else {
                thrust = (unsigned short) max_thrust;
            }

            const unsigned short angle_deg = util::angle_rad_to_deg_clipped(angle_rad);
            
            ship.velocity.vel_x = 0;
            ship.velocity.vel_y = 0;

            auto angle = angle_deg * M_PI / 180.0;
            ship.velocity.accelerate_by(thrust, angle);

            if (collision::will_collide(map, ship, target) == true) {
                const double new_target_dx = cos((angle_deg * M_PI / 180.0) + angular_step_rad) * distance;
                const double new_target_dy = sin((angle_deg * M_PI / 180.0) + angular_step_rad) * distance;
                const Location new_target = { ship.location.pos_x + new_target_dx, ship.location.pos_y + new_target_dy };

                return navigate_ship_towards_target(
                        map, ship, new_target, max_thrust, true, (max_corrections - 1), angular_step_rad);
            }

            return { Move::thrust(ship.entity_id, thrust, angle_deg), true };
        }

        static possibly<Move> navigate_ship_to_dock(
                const Map& map,
                Ship& ship,
                const Entity& dock_target,
                const int max_thrust)
        {
            const int max_corrections = constants::MAX_NAVIGATION_CORRECTIONS;
            const bool avoid_obstacles = true;
            const double angular_step_rad = M_PI / 180.0;
            const double max_dist = 0; //constants::DOCK_RADIUS + dock_target.radius - constants::SHIP_RADIUS - constants::SHIP_RADIUS - constants::SHIP_RADIUS;
            const Location& target = ship.location.get_closest_point(dock_target.location, max_dist);

            return navigate_ship_towards_target(
                    map, ship, target, max_thrust, avoid_obstacles, max_corrections, angular_step_rad);
        }
    }
}
