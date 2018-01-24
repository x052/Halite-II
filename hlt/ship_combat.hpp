namespace hlt {
    namespace combat {
        int get_closest_corner(
            Map& map,
            Ship& ship
        ) {
            Location closest_point;

            int x_min = 0;
            int y_min = 0;
            int x_max = map.map_width;
            int y_max = map.map_height;

            if (ship.location.pos_x >= (float)((float)x_max / (float)2)) {
                closest_point.pos_x = x_max;
            } else {
                closest_point.pos_x = x_min;
            }

            if (ship.location.pos_y >= (float)((float)y_max / (float)2)) {
                closest_point.pos_y = y_max;
            } else {
                closest_point.pos_y = y_min;
            }

            return ship.location.orient_towards_in_deg(closest_point);
        }

        int get_target_danger(
            Map& map,
            Ship& ship,
            Location target
        ) {
            const double target_radius = constants::MAX_SPEED + ship.radius +
                ship.radius + constants::WEAPON_RADIUS;
            int danger = 0;

            if (collision::out_of_bounds(map, target)) {
                return 9999;
            }

            for (const auto& player_ship : map.ships) {
                for (const Ship& ship2 : player_ship.second) {
                    if (ship.owner_id == ship2.owner_id) {
                        continue;
                    }
                    if (ship2.docking_status != ShipDockingStatus::Undocked) {
                        continue;
                    }

                    const double distance = target.distance(ship2.location);
                    if (distance <= target_radius) {
                        danger++;
                    }
                }
            }

            return danger;
        }

        static void handle_abandonment(
            Map& map,
            std::vector<hlt::Move>& moves,
            Ship& ship,
            int max_corrections
        ) {
            if (ship.docking_status == hlt::ShipDockingStatus::Docked) {
                moves.push_back(Move::undock(ship.entity_id));
                return;
            }

            if (ship.docking_status != hlt::ShipDockingStatus::Undocked) {
                moves.push_back(Move::noop());
                return;
            }

            // the best course of action is to run when we have no docked ships left
            int working_angle = get_closest_corner(map, ship);

            Location best_target = { 0, 0 };
            int lowest_danger_level = 9999;

            for (int angle_deg = 0; angle_deg < max_corrections; angle_deg++) {
                working_angle = hlt::navigation::clip_angle(working_angle + 1);

                const double new_target_dx = 7 * std::cos(working_angle * M_PI / 180.0);
                const double new_target_dy = 7 * std::sin(working_angle * M_PI / 180.0);
                const Location new_target = { ship.location.pos_x + new_target_dx, ship.location.pos_y + new_target_dy };

                int danger = get_target_danger(map, ship, new_target);
                if (danger < lowest_danger_level) {
                    lowest_danger_level = danger;
                    best_target = new_target;
                }
            }

            hlt::possibly<hlt::Move> move =
                hlt::navigation::navigate_ship_towards_target(
                    map,
                    ship,
                    best_target,
                    hlt::constants::MAX_SPEED,
                    true, 360, M_PI / 180.0);
            if (move.second) {
                moves.push_back(move.first);
            }
        }

        static void handle_rush(
            Map& map,
            std::vector<hlt::Move>& moves,
            PlayerId rush_target,
            bool& should_rush_at_the_start,
            Ship& ship
        ) {
            // testing on seed 2497728512
            // ./halite "./MyBot" "./MyBot" -i ../replays -s 2497728512
            auto targets_left_count = map.ships.at(rush_target).size();

            if (targets_left_count == 0) {
                hlt::Log::log("rush over, switching back");
                should_rush_at_the_start = false;
                return;
            }

            ship.sort_nearby_entitys();

            for (auto entity : ship.nearby_enemy_ships) {
                if (entity.owner_id != rush_target) {
                    // we only need to rush our target
                    continue;
                }

                hlt::Ship closest_enemy_ship = map.get_ship(entity.owner_id, entity.entity_id);

                // if the ship(s) are docked, we can kill them without worry
                if (closest_enemy_ship.docking_status != ShipDockingStatus::Undocked) {
                    const double target_radius = hlt::constants::WEAPON_RADIUS - ship.radius - closest_enemy_ship.radius;
                    const hlt::Location& target_location = ship.location.get_closest_point(closest_enemy_ship.location,
                        target_radius);
                    
                    hlt::possibly<hlt::Move> move =
                        hlt::navigation::navigate_ship_towards_target(
                            map,
                            ship,
                            target_location,
                            hlt::constants::MAX_SPEED,
                            true, 90, M_PI / 180.0);
                    if (move.second) {
                        map.add_moving_towards(closest_enemy_ship, ship);
                        moves.push_back(move.first);
                        break;
                    }
                }

                // if the ship is undocked (can attack us) we need to be clever about it
                if (closest_enemy_ship.docking_status == ShipDockingStatus::Undocked) {
                    const double target_radius = constants::MAX_SPEED + ship.radius +
                        ship.radius + constants::WEAPON_RADIUS;;
                    const double distance = ship.location.distance(closest_enemy_ship.location);

                    if (distance < target_radius) {
                        // we need to run away from the target
                        // because they can attack us if they move towards us

                        // we might want to move towards our ships to group them together
                        double angle_away_from_target = ship.location.orient_towards_in_deg(closest_enemy_ship.location) + 180;
                        angle_away_from_target = angle_away_from_target * 180.0 / M_PI;

                        const hlt::Location& target_location = { 
                            ship.location.pos_x + ((target_radius - distance) * cos(angle_away_from_target)),
                            ship.location.pos_y + ((target_radius - distance) * sin(angle_away_from_target))
                         };

                        hlt::possibly<hlt::Move> move =
                            hlt::navigation::navigate_ship_towards_target(
                                map,
                                ship,
                                target_location,
                                hlt::constants::MAX_SPEED,
                                true, 90, M_PI / 180.0);
                        if (move.second) {
                            map.add_moving_towards(closest_enemy_ship, ship);
                            moves.push_back(move.first);
                            break;
                        }
                    }

                    // we can move towards the target ships
                    // because its safe to do so as long as we keep our distance
                    if (distance > target_radius) {
                        const hlt::Location& target_location = ship.location.get_closest_point(closest_enemy_ship.location,
                            target_radius);
                        
                        hlt::possibly<hlt::Move> move =
                            hlt::navigation::navigate_ship_towards_target(
                                map,
                                ship,
                                target_location,
                                hlt::constants::MAX_SPEED,
                                true, 90, M_PI / 180.0);
                        if (move.second) {
                            map.add_moving_towards(closest_enemy_ship, ship);
                            moves.push_back(move.first);
                            break;
                        }
                    }
                }
            }
        }

        Location get_safe_location(
            Map& map,
            Ship& ship,
            Location target_location
        ) {
            int working_angle = ship.location.orient_towards_in_deg(target_location);

            Location best_target = { 0, 0 };
            int lowest_danger_level = 9999;

            for (int angle_deg = 0; angle_deg < 360; angle_deg++) {
                working_angle = hlt::navigation::clip_angle(working_angle + 1);

                const double new_target_dx = 7 * std::cos(working_angle * M_PI / 180.0);
                const double new_target_dy = 7 * std::sin(working_angle * M_PI / 180.0);
                const Location new_target = { ship.location.pos_x + new_target_dx, ship.location.pos_y + new_target_dy };

                int danger = get_target_danger(map, ship, new_target);
                if (danger < lowest_danger_level) {
                    lowest_danger_level = danger;
                    best_target = new_target;
                }
            }

            return best_target;
        }

        Ship get_closest_ship(
            Map& map,
            Ship& ship,
            std::vector<NearbyEntity> entity_list
        ) {
            int best_distance = 9999;
            Ship closest_ship;

            for (auto &entity : entity_list) {
                Ship& ship2 = map.get_ship(entity.owner_id, entity.entity_id);
                int distance = ship.location.distance(ship2.location);

                if (distance < best_distance) {
                    closest_ship = ship2;
                    best_distance = distance;
                }
            }

            return closest_ship;
        }

        static bool handle_ship(
            Map& map,
            PlayerId player_id,
            std::vector<hlt::Move>& moves,
            Ship& ship,
            NearbyEntity& entity
        ) {
            // planet docking and navigation logic
            if (!entity.is_ship) {
                // get the planet from the map
                hlt::Planet& planet = map.get_planet(entity.entity_id);
                
                if (planet.is_owned && !planet.is_owned_by(player_id)) {
                    // we don't want to move to a planet we can't dock at
                    // for example we cant dock at a planet when an enemy has already taken it
                    // to get that planet we need to attack the enemys docked ships
                    return false;
                }

                const double target_radius = constants::MAX_SPEED + ship.radius +
                    ship.radius + constants::WEAPON_RADIUS;

                ship.sort_nearby_entitys();
                auto closest_enemy_entity = ship.nearby_enemy_ships[0];
                if (closest_enemy_entity.distance <= target_radius) {
                    return false;
                }

                // check if the planet is full
                const int already_moving_towards = planet.ships_moving_towards.size();
                if (planet.is_owned_by(player_id) && (planet.docked_ships.size() + already_moving_towards) >= planet.docking_spots) {
                    // planet full!
                    return false;
                }

                if (ship.can_dock(planet)) {
                    // before docking we should check if its safe to do so
                    map.add_moving_towards(planet, ship);
                    moves.push_back(hlt::Move::dock(ship.entity_id, planet.entity_id));
                    return true;
                }

                hlt::possibly<hlt::Move> move =
                        hlt::navigation::navigate_ship_to_dock(map, ship, planet, hlt::constants::MAX_SPEED);
                if (move.second) {
                    map.add_moving_towards(planet, ship);
                    moves.push_back(move.first);
                    return true;
                }
            }

            // attacking other players logic
            if (entity.is_ship) {
                // get the ship from the map
                hlt::Ship& target_ship = map.get_ship(entity.owner_id, entity.entity_id);

                // if we own the ship
                // we could add something about grouping other ships which would improve meta combat
                // defense needs to go here
                if (entity.owner_id == player_id) {
                    return false;
                }

                // we only want to attack ships that cant attack us back
                // attacking enemy docked ships
                if (target_ship.docking_status != hlt::ShipDockingStatus::Undocked) {
                    const double target_radius = ship.radius + target_ship.radius;
                    const hlt::Location& target_location = ship.location.get_closest_point(target_ship.location,
                        target_radius);

                    Location safe_target = get_safe_location(map, ship, target_location);
                    
                    hlt::possibly<hlt::Move> move =
                        hlt::navigation::navigate_ship_towards_target(
                            map,
                            ship,
                            safe_target,
                            hlt::constants::MAX_SPEED,
                            true, 90, M_PI / 180.0);
                    if (move.second) {
                        map.add_moving_towards(target_ship, ship);
                        moves.push_back(move.first);
                        return true;
                    }
                }
                
                int max_distance = constants::MAX_SPEED + ship.radius +
                    ship.radius + constants::WEAPON_RADIUS;
                if (entity.distance < max_distance && ship.nearby_owned_and_docked_enemy_ships.size() > 0) {
                    // if the entitys distance is below x then we need to check for docked ships
                    ship.sort_nearby_entitys();
                    auto nearby_docked = ship.nearby_owned_and_docked_enemy_ships[0];

                    if (nearby_docked.distance < max_distance) {
                        hlt::Log::log("DOCKED AND ATTACKING");
                        hlt::Ship& nearby_docked_ship = map.get_ship(nearby_docked.owner_id, nearby_docked.entity_id);
                        
                        const double target_radius = ship.radius + target_ship.radius;
                        const hlt::Location& target_location = ship.location.get_closest_point(nearby_docked_ship.location,
                            target_radius);

                        Location safe_target = get_safe_location(map, ship, target_location);
                        
                        hlt::possibly<hlt::Move> move =
                            hlt::navigation::navigate_ship_towards_target(
                                map,
                                ship,
                                safe_target,
                                hlt::constants::MAX_SPEED,
                                true, 90, M_PI / 180.0);
                        if (move.second) {
                            map.add_moving_towards(target_ship, ship);
                            moves.push_back(move.first);
                            return true;
                        }
                    }
                }

                // now if all else fails, we may want to attack them
                // or we can go for their docked nearest ship
                const double target_radius = hlt::constants::WEAPON_RADIUS - ship.radius - target_ship.radius;
                hlt::Location target_location = ship.location.get_closest_point(target_ship.location,
                    target_radius);

                if (entity.distance < max_distance && ship.nearby_owned_and_docked_ships.size() > 0) {
                    auto nearby_docked = ship.nearby_owned_and_docked_ships[0];
                    hlt::Ship& nearby_docked_ship = map.get_ship(nearby_docked.owner_id, nearby_docked.entity_id);

                    const double defense_target_radius = hlt::constants::WEAPON_RADIUS - 1;
                    const double angle_towards_enemy = ship.location.orient_towards_in_deg(target_ship.location);

                    const double new_target_dx = defense_target_radius * std::cos(angle_towards_enemy * M_PI / 180.0);
                    const double new_target_dy = defense_target_radius * std::sin(angle_towards_enemy * M_PI / 180.0);
                    Location defense_target_location = { nearby_docked_ship.location.pos_x + new_target_dx,
                        nearby_docked_ship.location.pos_y + new_target_dy };

                    if (nearby_docked.distance < (max_distance * 1.5)) {
                        hlt::Log::log("DOCKED AND DEFENDING");

                        hlt::possibly<hlt::Move> move =
                            hlt::navigation::navigate_ship_towards_target(
                                map,
                                ship,
                                defense_target_location,
                                hlt::constants::MAX_SPEED,
                                true, 90, M_PI / 180.0);
                        if (move.second) {
                            map.add_moving_towards(target_ship, ship);
                            moves.push_back(move.first);
                            return true;
                        }
                    }
                }

                if (entity.distance < max_distance) {
                    // target_location = get_safe_location(map, ship, target_location);
                }
                
                hlt::possibly<hlt::Move> move =
                    hlt::navigation::navigate_ship_towards_target(
                        map,
                        ship,
                        target_location,
                        hlt::constants::MAX_SPEED,
                        true, 90, M_PI / 180.0);
                if (move.second) {
                    map.add_moving_towards(target_ship, ship);
                    moves.push_back(move.first);
                    return true;
                }
            }
            return false;
        }
    }
}