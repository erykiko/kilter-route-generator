#include "route.hpp"

#include "util.hpp"

#include <sstream>

RouteRole role_from_token_suffix(int suffix) {
  switch (suffix) {
    case 12: return ROLE_START;
    case 13: return ROLE_MIDDLE;
    case 14: return ROLE_FINISH;
    case 15: return ROLE_FOOT;
    default: return ROLE_MIDDLE;
  }
}

std::vector<std::pair<int, RouteRole>> parse_route_tokens_with_roles(
    const std::string& list_text) {
  std::vector<std::pair<int, RouteRole>> out;
  std::string cleaned;
  cleaned.reserve(list_text.size());
  for (char ch : list_text) {
    if (ch == '[' || ch == ']' || ch == '\'' || ch == '"') {
      continue;
    }
    cleaned.push_back(ch);
  }
  std::stringstream ss(cleaned);
  std::string token;
  std::vector<std::string> tokens;
  while (std::getline(ss, token, ',')) {
    token = trim(token);
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& tok = tokens[i];
    if (tok.size() < 2 || tok[0] != 'p') {
      continue;
    }
    int hole_id = 0;
    try {
      hole_id = std::stoi(tok.substr(1));
    } catch (...) {
      continue;
    }
    RouteRole role = ROLE_MIDDLE;
    if (i + 1 < tokens.size() && tokens[i + 1].size() >= 2 && tokens[i + 1][0] == 'r') {
      try {
        role = role_from_token_suffix(std::stoi(tokens[i + 1].substr(1)));
      } catch (...) {
      }
    }
    out.emplace_back(hole_id, role);
  }
  return out;
}

std::vector<std::string> parse_string_token_list(const std::string& list_text) {
  std::vector<std::string> out;
  std::string cleaned;
  cleaned.reserve(list_text.size());
  for (char ch : list_text) {
    if (ch == '[' || ch == ']' || ch == '\'' || ch == '"') {
      continue;
    }
    cleaned.push_back(ch);
  }
  std::stringstream ss(cleaned);
  std::string token;
  while (std::getline(ss, token, ',')) {
    token = trim(token);
    if (!token.empty()) {
      out.push_back(token);
    }
  }
  return out;
}

void apply_route_with_roles(AppState* state,
                            const std::vector<std::pair<int, RouteRole>>& holes_with_roles) {
  state->route_sequence.clear();
  state->route_holes.clear();
  state->route_role_by_hole.clear();
  state->route_tokens.clear();
  state->route_sequence.reserve(holes_with_roles.size());
  for (const auto& kv : holes_with_roles) {
    state->route_sequence.push_back(kv.first);
    state->route_holes.insert(kv.first);
    state->route_role_by_hole[kv.first] = kv.second;
  }
}

void apply_route_hole_ids(AppState* state, const std::vector<int>& hole_ids) {
  state->route_sequence = hole_ids;
  state->route_holes.clear();
  state->route_tokens.clear();
  for (int id : hole_ids) {
    state->route_holes.insert(id);
  }
  state->route_role_by_hole.clear();
  if (!state->route_sequence.empty()) {
    state->route_role_by_hole[state->route_sequence.front()] = ROLE_START;
    state->route_role_by_hole[state->route_sequence.back()] = ROLE_FINISH;
  }
}

void set_current_route_metadata(AppState* state, int layout_id, int grade_id,
                                const std::string& source,
                                const std::vector<std::string>& tokens) {
  state->has_route = !state->route_sequence.empty();
  state->current_layout_id = layout_id;
  state->current_grade_id = grade_id;
  state->current_source = source;
  if (!tokens.empty()) {
    state->route_tokens = tokens;
  } else {
    state->route_tokens.clear();
    for (int id : state->route_sequence) {
      state->route_tokens.push_back("p" + std::to_string(id));
    }
  }
}

std::string format_role_counts(const AppState* state) {
  int starts = 0, middles = 0, finishes = 0, feet = 0;
  for (const auto& kv : state->route_role_by_hole) {
    switch (kv.second) {
      case ROLE_START: ++starts; break;
      case ROLE_MIDDLE: ++middles; break;
      case ROLE_FINISH: ++finishes; break;
      case ROLE_FOOT: ++feet; break;
    }
  }
  std::stringstream ss;
  ss << "start=" << starts << ", middle=" << middles
     << ", finish=" << finishes << ", foot=" << feet;
  return ss.str();
}
