#pragma once

#include "types.hpp"

#include <string>
#include <vector>

bool init_results_db(const std::string& db_path, std::string& error);
bool save_route_to_db(const std::string& db_path, const SavedRoute& route, std::string& error);
std::vector<SavedRoute> load_recent_routes(const std::string& db_path, int limit);
