#pragma once

#include <gtk/gtk.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "generator_process.hpp"

struct Hole {
  int id{};
  double x{};
  double y{};
};

struct SavedRoute {
  int id{};
  std::string created_at;
  int layout_id{};
  int grade_id{};
  std::string source;
  int rating{};
  int hole_count{};
  std::string tokens;
  std::string hole_ids;
};

enum RouteRole {
  ROLE_START = 0,
  ROLE_MIDDLE = 1,
  ROLE_FINISH = 2,
  ROLE_FOOT = 3
};

struct AppState {
  std::map<int, Hole> holes;
  std::vector<int> route_sequence;
  std::set<int> route_holes;
  std::map<int, int> route_role_by_hole;
  std::vector<std::string> route_tokens;
  std::string status;
  std::string repo_root;
  std::string db_path;
  std::string results_db_path;
  std::string generate_server_script_path;
  std::string python_path;
  std::string generate_script_path;
  std::string dataset_sample_script_path;
  std::string checkpoint_path;
  std::string checkpoint_dir;
  std::vector<std::string> available_checkpoints;
  std::string token_map_path;
  std::string id_map_path;
  std::string dataset_path;
  std::vector<std::pair<int, std::string>> layout_tokens;
  std::vector<std::pair<int, std::string>> grade_tokens;

  bool has_route{false};
  int current_layout_id{-1};
  int current_grade_id{-1};
  std::string current_source;

  GeneratorProcess generator;

  GtkWidget* window{};
  GtkWidget* drawing_area{};
  GtkWidget* status_label{};
  GtkWidget* layout_combo{};
  GtkWidget* grade_combo{};
  GtkWidget* checkpoint_combo{};
  GtkWidget* checkpoint_entry{};
  GtkWidget* temperature_entry{};
  GtkWidget* topk_entry{};
  GtkWidget* topp_entry{};
  GtkWidget* repetition_penalty_entry{};
  GtkWidget* greedy_check{};
  GtkWidget* rating_spin{};
  GtkWidget* save_btn{};
  GtkWidget* history_label{};
};
