#include "token_loader.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

bool load_condition_tokens(const std::string& token_map_path,
                           std::vector<std::pair<int, std::string>>& out_layouts,
                           std::vector<std::pair<int, std::string>>& out_grades,
                           std::string& error) {
  std::ifstream in(token_map_path);
  if (!in.is_open()) {
    error = "Could not open token map: " + token_map_path;
    return false;
  }

  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string text = buffer.str();
  std::regex pair_re("\"([^\"]+)\"\\s*:\\s*(\\d+)");
  auto begin = std::sregex_iterator(text.begin(), text.end(), pair_re);
  auto end = std::sregex_iterator();

  out_layouts.clear();
  out_grades.clear();
  for (auto it = begin; it != end; ++it) {
    const std::string token = (*it)[1].str();
    const int id = std::stoi((*it)[2].str());
    if (id >= 1224 && id <= 1262) {
      out_grades.push_back({id, token});
    } else if (id >= 1263 && id <= 1278) {
      out_layouts.push_back({id, token});
    }
  }

  std::sort(out_layouts.begin(), out_layouts.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  std::sort(out_grades.begin(), out_grades.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  if (out_layouts.empty() || out_grades.empty()) {
    error = "Could not extract layout/grade tokens from token map.";
    return false;
  }
  return true;
}
