#include "hlt/hlt.hpp"
#include "hlt/navigation.hpp"
#include "hlt/ship_combat.hpp"

int main() {
    hlt::Metadata metadata = hlt::initialize("bhaxbot v6 v14");
    hlt::PlayerId player_id = metadata.player_id;

    hlt::Map& initial_map = metadata.initial_map;

    // We now have 1 full minute to analyse the initial map.
    std::ostringstream initial_map_intelligence;
    initial_map_intelligence
            << "width: " << initial_map.map_width
            << "; height: " << initial_map.map_height
            << "; players: " << initial_map.ship_map.size()
            << "; my ships: " << initial_map.ship_map.at(player_id).size()
            << "; planets: " << initial_map.planets.size();
    hlt::Log::log(initial_map_intelligence.str());

    bool should_rush_at_the_start = false;
    bool has_decided_to_rush_at_the_start = false;
    hlt::PlayerId rush_target;

    bool has_decided_to_abandon = false;
    int game_turn = 0;

    std::vector<hlt::Move> moves;
    for (;;) {
        moves.clear();
        hlt::Map map = hlt::in::get_map();
        game_turn++;

        // build a list of nearby enemys and targets
        for (hlt::Ship &ship : map.ships.at(player_id)) {
            if (ship.docking_status == hlt::ShipDockingStatus::Docking) {
                auto planet = map.get_planet(ship.docked_planet);
                map.add_moving_towards(planet, ship);
            }

            if (ship.docking_status != hlt::ShipDockingStatus::Undocked) {     
                continue;
            }

            for (hlt::Planet planet : map.planets) {
                hlt::NearbyEntity planet_entity;
                planet_entity.is_ship = false;
                planet_entity.entity_id = planet.entity_id;
                planet_entity.distance = ship.location.distance(planet.location);

                ship.priority_targets.push(planet_entity);
                ship.nearby_planets.push_back(planet_entity);
            }

            for (const auto& player_ship : map.ships) {
                for (const hlt::Ship& ship2 : player_ship.second) {
                    hlt::NearbyEntity ship_entity;
                    ship_entity.is_ship = true;
                    ship_entity.entity_id = ship2.entity_id;
                    ship_entity.owner_id = player_ship.first;
                    ship_entity.distance = ship.location.distance(ship2.location);

                    ship.priority_targets.push(ship_entity);

                    if (player_ship.first == player_id) {
                        // if we own the ship
                        ship.nearby_owned_ships.push_back(ship_entity);

                        if (ship2.docking_status == hlt::ShipDockingStatus::Docked) {
                            ship.nearby_owned_and_docked_ships.push_back(ship_entity);
                        }
                    } else {
                        // if the ship is not owned
                        ship.nearby_enemy_ships.push_back(ship_entity);

                        if (ship2.docking_status == hlt::ShipDockingStatus::Docked) {
                            ship.nearby_owned_and_docked_enemy_ships.push_back(ship_entity);
                        }
                    }
                }
            }
        }

        // only run once at the start of the game
        if (has_decided_to_rush_at_the_start == false) {
            for (hlt::Ship& ship : map.ships.at(player_id)) {
                // we want to check weather we can rush or not
                ship.sort_nearby_entitys();
                hlt::NearbyEntity entity = ship.nearby_enemy_ships[0];
                hlt::Ship closest_enemy_ship = map.get_ship(entity.owner_id, entity.entity_id);

                int time_to_build_new_ship = 9; // it takes 9 turns for a ship to be produced (if all 3 ships decide to dock)
                int time_to_kill_enemy_ships = 0;
                bool conditions_met = false;

                auto distance_to_closest_planet = 99999;
                hlt::Planet closest_planet;
                for (hlt::Planet& planet : map.planets) {
                    auto planet_to_enemy_distance = closest_enemy_ship.location.distance(planet.location);
                    if (planet_to_enemy_distance < distance_to_closest_planet) {
                        distance_to_closest_planet = planet_to_enemy_distance;
                        closest_planet = planet;
                    }
                }
                
                // calculate how long it takes to reach a location our enemy can dock at
                const double max_dist_to_dock = hlt::constants::DOCK_RADIUS + closest_planet.radius - hlt::constants::SHIP_RADIUS;
                hlt::Location closest_location = closest_enemy_ship.location.get_closest_point(closest_planet.location, max_dist_to_dock);
                time_to_build_new_ship += closest_enemy_ship.location.distance(closest_location) / hlt::constants::MAX_SPEED;

                const double ship_health = 255;
                const double attack_damage = 192; // may need to lower to adjust to slower ships
                time_to_kill_enemy_ships += closest_enemy_ship.location.distance(ship.location) / hlt::constants::MAX_SPEED;
                time_to_kill_enemy_ships += (attack_damage / ship_health) * 1; // how long it takes to kill one ship
                // by the time one ship is dead, the delay to make another will increase, making it take longer to make another ship

                if (time_to_kill_enemy_ships < time_to_build_new_ship) {
                    conditions_met = true;
                }

                if (conditions_met) {
                    // at this distance they shouldn't be able to make another ship before we kill them
                    should_rush_at_the_start = true;
                    rush_target = entity.owner_id;
                    break;
                }
            }

            has_decided_to_rush_at_the_start = true;
        }

        // decide whether abandoning is the best option
        if (!should_rush_at_the_start && !has_decided_to_abandon) {
            if (initial_map.ship_map.size() > 2) {
                int total_ships = 0;
                int my_total_ships = 0;

                for (const auto& player_ship : map.ships) {
                    for (const hlt::Ship& ship : player_ship.second) {
                        if (player_ship.first == player_id) {
                            my_total_ships++;
                        }
                        total_ships++;
                    }
                }

                int MAX_OWNED_PERCENTAGE = 15.5;
                float owned_percentage = ((float)my_total_ships / (float)total_ships) * 100;
                if (owned_percentage < MAX_OWNED_PERCENTAGE) {
                    has_decided_to_abandon = true;
                }
            }
        }

        // now once we have our nearby entitys
        // we want to utilize them in some shape or form
        for (hlt::Ship &ship : map.ships.at(player_id)) {
            bool has_made_move = false;

            if (!has_made_move && has_decided_to_abandon) {
                hlt::combat::handle_abandonment(map, moves, ship, 360);
                has_made_move = true;
            }

            if (ship.priority_targets.empty()) {
                // ship has no targets, its probably docked or everyone could be dead
                continue;
            }

            if (!has_made_move && should_rush_at_the_start) {
                hlt::combat::handle_rush(map, moves, rush_target, should_rush_at_the_start, ship);
                has_made_move = true;
            }

            while (!has_made_move && !ship.priority_targets.empty()) {
                auto entity = ship.priority_targets.top();
                ship.priority_targets.pop();

                if (hlt::combat::handle_ship(map, player_id, moves, ship, entity)) {
                    has_made_move = true;
                    break;
                }
            }
        }

        // check for collisions
        for (hlt::Ship &ship : map.ships.at(player_id)) {
            hlt::Location temp_target = { 1, 1 };
            if (hlt::collision::will_collide(map, ship, temp_target)) {
                hlt::Log::log("late collision found ship: " + std::to_string(ship.entity_id));
            }
        }

        if (!hlt::out::send_moves(moves)) {
            hlt::Log::log("send_moves failed; exiting");
            break;
        }
    }
}
