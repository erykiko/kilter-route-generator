#pragma once

#include <string>
#include <utility>
#include <vector>

bool load_condition_tokens(const std::string& token_map_path,
                           std::vector<std::pair<int, std::string>>& out_layouts,
                           std::vector<std::pair<int, std::string>>& out_grades,
                           std::string& error);
