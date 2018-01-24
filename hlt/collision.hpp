#pragma once

#include <algorithm>

#include "entity.hpp"
#include "location.hpp"

constexpr auto EVENT_TIME_PRECISION = 10000;

namespace hlt {
    namespace collision {
        static double square(const double num) {
            return num * num;
        }

        /**
         * Test whether a given line segment intersects a circular area.
         *
         * @param start  The start of the segment.
         * @param end    The end of the segment.
         * @param circle The circle to test against.
         * @param fudge  An additional safety zone to leave when looking for collisions. Probably set it to ship radius.
         * @return true if the segment intersects, false otherwise
         */
        static bool segment_circle_intersect(
                const Location& start,
                const Location& end,
                const Entity& circle,
                const double fudge)
        {
            // Parameterize the segment as start + t * (end - start),
            // and substitute into the equation of a circle
            // Solve for t
            const double circle_radius = circle.radius;
            const double start_x = start.pos_x;
            const double start_y = start.pos_y;
            const double end_x = end.pos_x;
            const double end_y = end.pos_y;
            const double center_x = circle.location.pos_x;
            const double center_y = circle.location.pos_y;
            const double dx = end_x - start_x;
            const double dy = end_y - start_y;

            const double a = square(dx) + square(dy);

            const double b =
                    -2 * (square(start_x) - (start_x * end_x)
                    - (start_x * center_x) + (end_x * center_x)
                    + square(start_y) - (start_y * end_y)
                    - (start_y * center_y) + (end_y * center_y));

            if (a == 0.0) {
                // Start and end are the same point
                return start.get_distance_to(circle.location) <= circle_radius + fudge;
            }

            // Time along segment when closest to the circle (vertex of the quadratic)
            const double t = std::min(-b / (2 * a), 1.0);
            if (t < 0) {
                return false;
            }

            const double closest_x = start_x + dx * t;
            const double closest_y = start_y + dy * t;
            const double closest_distance = Location{ closest_x, closest_y }.get_distance_to(circle.location);

            return closest_distance <= circle_radius + fudge;
        }

        static void check_and_add_entity_between(
                std::vector<const Entity *>& entities_found,
                const Location& start,
                const Location& target,
                const Entity& entity_to_check)
        {
            const Location &location = entity_to_check.location;
            if (location == start || location == target) {
                return;
            }
            if (segment_circle_intersect(start, target, entity_to_check, constants::FORECAST_FUDGE_FACTOR)) {
                entities_found.push_back(&entity_to_check);
            }
        }

        static std::vector<const Entity *> objects_between(
                const Map& map,
                const Location& start,
                const Location& target,
                const bool include_ships,
                const bool include_planets
        ) {
            std::vector<const Entity *> entities_found;

            if (include_planets) {
                for (const Planet& planet : map.planets) {
                    check_and_add_entity_between(entities_found, start, target, planet);
                }
            }

            if (include_ships) {
                for (const auto& player_ship : map.ships) {
                    for (const Ship& ship : player_ship.second) {
                        check_and_add_entity_between(entities_found, start, target, ship);
                    }
                }
            }

            return entities_found;
        }

        static bool out_of_bounds (
                const Map& map,
                const Location& target
        ) {
          return !(target.pos_x >= 0 && target.pos_y >= 0 &&
            target.pos_x < map.map_width && target.pos_y < map.map_height);
        }

        auto ship_collision_time(
            long double r,
            hlt::Ship ship1,
            hlt::Ship ship2
        ) -> std::pair<bool, double> {
            // With credit to Ben Spector
            // Simplified derivation:
            // 1. Set up the distance between the two entities in terms of time,
            //    the difference between their velocities and the difference between
            //    their positions
            // 2. Equate the distance equal to the event radius (max possible distance
            //    they could be)
            // 3. Solve the resulting quadratic

            const auto dx = ship1.location.pos_x - ship2.location.pos_x;
            const auto dy = ship1.location.pos_y - ship2.location.pos_y;
            const auto dvx = ship1.velocity.vel_x - ship2.velocity.vel_x;
            const auto dvy = ship1.velocity.vel_y - ship2.velocity.vel_y;

            // Quadratic formula
            const auto a = std::pow(dvx, 2) + std::pow(dvy, 2);
            const auto b = 2 * (dx * dvx + dy * dvy);
            const auto c = std::pow(dx, 2) + std::pow(dy, 2) - std::pow(r, 2);

            const auto disc = std::pow(b, 2) - 4 * a * c;

            if (a == 0.0) {
                if (b == 0.0) {
                    if (c <= 0.0) {
                        // Implies r^2 >= dx^2 + dy^2 and the two are already colliding
                        return { true, 0.0 };
                    }
                    return { false, 0.0 };
                }
                const auto t = -c / b;
                if (t >= 0.0) {
                    return { true, t };
                }
                return { false, 0.0 };
            }
            else if (disc == 0.0) {
                // One solution
                const auto t = -b / (2 * a);
                return { true, t };
            }
            else if (disc > 0) {
                const auto t1 = -b + std::sqrt(disc);
                const auto t2 = -b - std::sqrt(disc);

                if (t1 >= 0.0 && t2 >= 0.0) {
                    return { true, std::min(t1, t2) / (2 * a) };
                } else if (t1 <= 0.0 && t2 <= 0.0) {
                    return { true, std::max(t1, t2) / (2 * a) };
                } else {
                    return { true, 0.0 };
                }
            }
            else {
                return { false, 0.0 };
            }
        }

        auto planet_collision_time(
            long double r,
            hlt::Ship ship1,
            hlt::Planet planet2
        ) -> std::pair<bool, double> {
            // With credit to Ben Spector
            // Simplified derivation:
            // 1. Set up the distance between the two entities in terms of time,
            //    the difference between their velocities and the difference between
            //    their positions
            // 2. Equate the distance equal to the event radius (max possible distance
            //    they could be)
            // 3. Solve the resulting quadratic

            const auto dx = ship1.location.pos_x - planet2.location.pos_x;
            const auto dy = ship1.location.pos_y - planet2.location.pos_y;
            const auto dvx = ship1.velocity.vel_x - planet2.velocity.vel_x;
            const auto dvy = ship1.velocity.vel_y - planet2.velocity.vel_y;

            // Quadratic formula
            const auto a = std::pow(dvx, 2) + std::pow(dvy, 2);
            const auto b = 2 * (dx * dvx + dy * dvy);
            const auto c = std::pow(dx, 2) + std::pow(dy, 2) - std::pow(r, 2);

            const auto disc = std::pow(b, 2) - 4 * a * c;

            if (a == 0.0) {
                if (b == 0.0) {
                    if (c <= 0.0) {
                        // Implies r^2 >= dx^2 + dy^2 and the two are already colliding
                        return { true, 0.0 };
                    }
                    return { false, 0.0 };
                }
                const auto t = -c / b;
                if (t >= 0.0) {
                    return { true, t };
                }
                return { false, 0.0 };
            }
            else if (disc == 0.0) {
                // One solution
                const auto t = -b / (2 * a);
                return { true, t };
            }
            else if (disc > 0) {
                const auto t1 = -b + std::sqrt(disc);
                const auto t2 = -b - std::sqrt(disc);

                if (t1 >= 0.0 && t2 >= 0.0) {
                    return { true, std::min(t1, t2) / (2 * a) };
                } else if (t1 <= 0.0 && t2 <= 0.0) {
                    return { true, std::max(t1, t2) / (2 * a) };
                } else {
                    return { true, 0.0 };
                }
            }
            else {
                return { false, 0.0 };
            }
        }

        auto round_event_time(double t) -> double {
            return std::round(t * EVENT_TIME_PRECISION) / EVENT_TIME_PRECISION;
        }

        auto round_event_movment(double t) -> double {
            return std::round(t * EVENT_TIME_PRECISION) / EVENT_TIME_PRECISION;
        }

        auto might_collide(long double distance, const hlt::Ship& ship1, const hlt::Ship& ship2) -> bool {
            return distance <= // ship1.magnitude() + ship2.magnitude() +
                sqrt(7 * 7 + 7 * 7) + sqrt(7 * 7 + 7 * 7) +
                ship1.radius + ship2.radius + 0.01;
        }

        static bool will_collide (
                const Map& map,
                Ship& ship1,
                const Location& target)
        {
            if (out_of_bounds(map, target)) {
                return true;
            }

            for (auto& planet : map.planets) {
                const auto distance = ship1.location.distance(planet.location);
                if (distance <= ship1.velocity.magnitude() + ship1.radius + planet.radius) {
                    const auto collision_radius = ship1.radius + planet.radius + 0.01;
                    const auto t = planet_collision_time(collision_radius, ship1, planet);
                    if (t.first) {
                        if (round_event_time(t.second) >= 0 && round_event_time(t.second) <= 1) {
                            return true;
                        }
                    }
                }
            }

            for (const auto& player_ship : map.ships) {
                for (const Ship& ship2 : player_ship.second) {
                    if (ship1.entity_id == ship2.entity_id && ship1.owner_id == ship2.owner_id) {
                        // ignore our own ship
                        continue;
                    }

                    const auto distance = ship1.location.distance(ship2.location);
                    auto in_rage_to_collide = might_collide(distance, ship1, ship2);

                    if (in_rage_to_collide) {
                       const double r = constants::SHIP_RADIUS * 2 + 0.01;
                        auto t = ship_collision_time(r, ship1, ship2);

                        if (t.first && t.second >= 0 && t.second <= 1) {
                            return true;
                        }
                    }
                }
            }
            
            return false;
        }
    }
}
