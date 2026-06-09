#pragma once

#include "types.hpp"

#include <string>
#include <utility>
#include <vector>

RouteRole role_from_token_suffix(int suffix);
std::vector<std::pair<int, RouteRole>> parse_route_tokens_with_roles(const std::string& list_text);
std::vector<std::string> parse_string_token_list(const std::string& list_text);

void apply_route_with_roles(AppState* state,
                            const std::vector<std::pair<int, RouteRole>>& holes_with_roles);
void apply_route_hole_ids(AppState* state, const std::vector<int>& hole_ids);
void set_current_route_metadata(AppState* state, int layout_id, int grade_id,
                                const std::string& source,
                                const std::vector<std::string>& tokens);
std::string format_role_counts(const AppState* state);
