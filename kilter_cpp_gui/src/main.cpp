#include "types.hpp"

#include "board_db.hpp"
#include "results_db.hpp"
#include "token_loader.hpp"
#include "ui.hpp"
#include "util.hpp"

#include <algorithm>
#include <csignal>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  signal(SIGPIPE, SIG_IGN);

  AppState state;

  state.repo_root = fs::current_path().string();
  state.db_path = state.repo_root + "/boardlib_data/kilter.sqlite3";
  state.results_db_path = state.repo_root + "/kilter_cpp_gui/data/saved_routes.db";
  state.python_path = state.repo_root + "/kilter_dl/.venv/bin/python";
  state.generate_script_path = state.repo_root + "/kilter_dl/generate.py";
  state.generate_server_script_path = state.repo_root + "/kilter_dl/generate_server.py";
  state.dataset_sample_script_path = state.repo_root + "/kilter_dl/sample_dataset_route.py";
  state.checkpoint_dir = state.repo_root + "/kilter_dl/checkpoints";
  state.checkpoint_path = state.checkpoint_dir + "/kilter_gen.pt";
  state.available_checkpoints = list_checkpoints(state.checkpoint_dir);
  if (!state.available_checkpoints.empty()) {
    bool has_default = std::find(state.available_checkpoints.begin(),
                                 state.available_checkpoints.end(),
                                 state.checkpoint_path) != state.available_checkpoints.end();
    if (!has_default) {
      state.checkpoint_path = state.available_checkpoints.front();
    }
  }
  state.token_map_path = state.repo_root + "/dataset/token_to_id.json";
  state.id_map_path = state.repo_root + "/dataset/id_to_token.json";
  state.dataset_path = state.repo_root + "/dataset/dataset/single_board.json";

  std::string err;
  if (!load_condition_tokens(state.token_map_path, state.layout_tokens, state.grade_tokens, err)) {
    state.status = "Token load error: " + err;
  }
  if (!init_results_db(state.results_db_path, err) && state.status.empty()) {
    state.status = "Results DB init error: " + err;
  }
  if (!load_holes_from_db(state.db_path, 7, state.holes, err)) {
    if (state.status.empty()) {
      state.status = "DB load error: " + err;
    }
  } else if (state.status.empty()) {
    state.status = "Loaded holes: " + std::to_string(state.holes.size());
  }

  GtkApplication* app = gtk_application_new("com.sztuczna.kiltergui", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), &state);
  const int status = g_application_run(G_APPLICATION(app), 0, nullptr);
  g_object_unref(app);
  state.generator.stop();
  return status;
}
