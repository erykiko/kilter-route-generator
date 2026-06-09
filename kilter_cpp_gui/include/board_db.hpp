#pragma once

#include "types.hpp"

#include <map>
#include <string>

int product_size_id_from_layout_token(int layout_token_id);
bool load_holes_from_db(const std::string& db_path, int product_size_id,
                        std::map<int, Hole>& out_holes, std::string& error);
