#include "ui.hpp"

#include "board_db.hpp"
#include "json_util.hpp"
#include "results_db.hpp"
#include "route.hpp"
#include "util.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

void set_status(AppState* state, const std::string& text) {
  state->status = text;
  gtk_label_set_text(GTK_LABEL(state->status_label), state->status.c_str());
}

void draw_board(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data) {
  (void)area;
  auto* state = static_cast<AppState*>(user_data);

  cairo_set_source_rgb(cr, 0.07, 0.09, 0.12);
  cairo_paint(cr);

  if (state->holes.empty()) {
    cairo_set_source_rgb(cr, 0.9, 0.2, 0.2);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, "No holes loaded");
    return;
  }

  double min_x = state->holes.begin()->second.x;
  double max_x = min_x;
  double min_y = state->holes.begin()->second.y;
  double max_y = min_y;
  const double pad = 24.0;

  cairo_set_source_rgb(cr, 0.16, 0.19, 0.25);
  cairo_rectangle(cr, pad * 0.5, pad * 0.5, width - pad, height - pad);
  cairo_fill(cr);

  for (const auto& kv : state->holes) {
    min_x = std::min(min_x, kv.second.x);
    max_x = std::max(max_x, kv.second.x);
    min_y = std::min(min_y, kv.second.y);
    max_y = std::max(max_y, kv.second.y);
  }

  const double span_x = std::max(1.0, max_x - min_x);
  const double span_y = std::max(1.0, max_y - min_y);
  const double avail_w = std::max(1.0, width - pad * 2.0);
  const double avail_h = std::max(1.0, height - pad * 2.0);
  const double sx = avail_w / span_x;
  const double sy = avail_h / span_y;
  const double scale = std::min(sx, sy);
  const double content_w = span_x * scale;
  const double content_h = span_y * scale;
  const double x_offset = (width - content_w) * 0.5;
  const double y_offset = (height - content_h) * 0.5;

  const double inactive_r = std::clamp(scale * 0.95, 1.8, 3.2);
  const double active_r = std::clamp(scale * 2.6, 5.0, 8.4);

  for (const auto& kv : state->holes) {
    const auto& h = kv.second;
    const double px = x_offset + (h.x - min_x) * scale;
    const double py = y_offset + (max_y - h.y) * scale;
    const bool active = state->route_holes.find(h.id) != state->route_holes.end();

    if (active) {
      int role = ROLE_MIDDLE;
      auto it = state->route_role_by_hole.find(h.id);
      if (it != state->route_role_by_hole.end()) {
        role = it->second;
      }
      if (role == ROLE_START) {
        cairo_set_source_rgb(cr, 0.15, 0.90, 0.25);
      } else if (role == ROLE_FINISH) {
        cairo_set_source_rgb(cr, 0.85, 0.15, 0.95);
      } else if (role == ROLE_FOOT) {
        cairo_set_source_rgb(cr, 1.00, 0.65, 0.10);
      } else {
        cairo_set_source_rgb(cr, 0.10, 0.85, 0.95);
      }
      cairo_arc(cr, px, py, active_r, 0.0, 2.0 * G_PI);
      cairo_fill(cr);
      cairo_set_source_rgb(cr, 0.98, 0.98, 0.98);
      cairo_set_line_width(cr, 1.4);
      cairo_arc(cr, px, py, active_r, 0.0, 2.0 * G_PI);
      cairo_stroke(cr);
    } else {
      cairo_set_source_rgb(cr, 0.72, 0.72, 0.72);
      cairo_arc(cr, px, py, inactive_r, 0.0, 2.0 * G_PI);
      cairo_fill(cr);
    }
  }
}

static void on_generate_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  auto* state = static_cast<AppState*>(user_data);

  const char* layout_id_txt = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->layout_combo));
  const char* grade_id_txt = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->grade_combo));
  if (!layout_id_txt || !grade_id_txt) {
    set_status(state, "Select board and grade tokens.");
    return;
  }
  const int layout_id = std::stoi(layout_id_txt);
  const int grade_id = std::stoi(grade_id_txt);

  std::string selected_checkpoint;
  if (state->checkpoint_combo != nullptr) {
    const char* sel = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->checkpoint_combo));
    if (sel != nullptr) {
      selected_checkpoint = sel;
    }
  }
  if (selected_checkpoint.empty() && state->checkpoint_entry != nullptr) {
    selected_checkpoint = gtk_editable_get_text(GTK_EDITABLE(state->checkpoint_entry));
  }
  if (selected_checkpoint.empty()) {
    selected_checkpoint = state->checkpoint_path;
  }
  state->checkpoint_path = selected_checkpoint;

  const int product_size_id = product_size_id_from_layout_token(layout_id);
  if (product_size_id < 0) {
    set_status(state, "Unsupported layout token id for board rendering.");
    return;
  }
  std::string db_err;
  if (!load_holes_from_db(state->db_path, product_size_id, state->holes, db_err)) {
    set_status(state, "DB reload failed: " + db_err);
    return;
  }

  if (!state->generator.running()) {
    set_status(state, "Starting generator process (loading Python + torch)...");
    if (!state->generator.start(state->python_path,
                                state->generate_server_script_path,
                                state->token_map_path,
                                state->id_map_path,
                                state->repo_root)) {
      set_status(state, "Could not start generator process: " + state->generator.last_error());
      return;
    }
  }

  const char* temperature_txt = gtk_editable_get_text(GTK_EDITABLE(state->temperature_entry));
  const char* topk_txt = gtk_editable_get_text(GTK_EDITABLE(state->topk_entry));
  const char* topp_txt = gtk_editable_get_text(GTK_EDITABLE(state->topp_entry));
  const char* rep_txt = gtk_editable_get_text(GTK_EDITABLE(state->repetition_penalty_entry));
  const bool greedy = gtk_check_button_get_active(GTK_CHECK_BUTTON(state->greedy_check));

  std::stringstream req;
  req << "{\"cmd\":\"generate\","
      << "\"checkpoint\":\"" << state->checkpoint_path << "\","
      << "\"layout_id\":" << layout_id << ","
      << "\"grade_id\":" << grade_id << ","
      << "\"max_len\":128,"
      << "\"greedy\":" << (greedy ? "true" : "false");
  if (!greedy) {
    req << ",\"temperature\":" << trim(temperature_txt)
        << ",\"top_k\":" << trim(topk_txt)
        << ",\"top_p\":" << trim(topp_txt)
        << ",\"repetition_penalty\":" << trim(rep_txt);
  }
  req << "}";

  std::string response;
  if (!state->generator.request(req.str(), response)) {
    set_status(state, "Generation IPC failed: " + state->generator.last_error());
    return;
  }

  std::string status_field;
  json_get_string(response, "status", status_field);
  if (status_field != "ok") {
    std::string message;
    json_get_string(response, "message", message);
    set_status(state, "Generator error: " + (message.empty() ? response : message));
    return;
  }

  std::string tokens_arr;
  std::string hole_ids_arr;
  const bool have_tokens = json_get_array(response, "tokens", tokens_arr);
  const bool have_hole_ids = json_get_array(response, "hole_ids", hole_ids_arr);
  if (!have_tokens && !have_hole_ids) {
    set_status(state, "Could not parse generator response.");
    return;
  }

  std::vector<std::string> token_list;
  if (have_tokens) {
    token_list = parse_string_token_list(tokens_arr);
    apply_route_with_roles(state, parse_route_tokens_with_roles(tokens_arr));
  } else {
    apply_route_hole_ids(state, parse_python_list_ints_ordered(hole_ids_arr));
  }
  set_current_route_metadata(state, layout_id, grade_id, "model", token_list);

  set_status(state,
             "Route generated: " + std::to_string(state->route_holes.size()) +
                 " holds (" + format_role_counts(state) +
                 "), board: " + std::to_string(state->holes.size()) + " holes.");
  gtk_widget_queue_draw(state->drawing_area);
}

static void on_random_dataset_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  auto* state = static_cast<AppState*>(user_data);

  const char* layout_id_txt = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->layout_combo));
  const char* grade_id_txt = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->grade_combo));
  if (!layout_id_txt || !grade_id_txt) {
    set_status(state, "Select board and grade tokens.");
    return;
  }
  const int layout_id = std::stoi(layout_id_txt);
  const int grade_id = std::stoi(grade_id_txt);

  const int product_size_id = product_size_id_from_layout_token(layout_id);
  if (product_size_id < 0) {
    set_status(state, "Unsupported layout token id for board rendering.");
    return;
  }
  std::string db_err;
  if (!load_holes_from_db(state->db_path, product_size_id, state->holes, db_err)) {
    set_status(state, "DB reload failed: " + db_err);
    return;
  }

  std::stringstream cmd;
  cmd << "\"" << state->python_path << "\" "
      << "\"" << state->dataset_sample_script_path << "\" "
      << "--dataset \"" << state->dataset_path << "\" "
      << "--id-map \"" << state->id_map_path << "\" "
      << "--layout-id " << layout_id << " "
      << "--grade-id " << grade_id;

  int exit_code = 0;
  const std::string output = run_command_capture_stdout(cmd.str(), exit_code);
  if (exit_code != 0) {
    set_status(state, "Random dataset sampling failed.");
    return;
  }

  std::stringstream ss(output);
  std::string line;
  std::string tokens_line;
  std::string hole_ids_line;
  while (std::getline(ss, line)) {
    if (line.find("Dataset route tokens:") != std::string::npos) {
      std::getline(ss, tokens_line);
      continue;
    }
    if (line.find("Dataset hole ids:") != std::string::npos) {
      std::getline(ss, hole_ids_line);
      break;
    }
  }
  if (tokens_line.empty() && hole_ids_line.empty()) {
    set_status(state, "Could not parse random dataset route.");
    return;
  }

  std::vector<std::string> token_list;
  if (!tokens_line.empty()) {
    token_list = parse_string_token_list(tokens_line);
    apply_route_with_roles(state, parse_route_tokens_with_roles(tokens_line));
  } else {
    apply_route_hole_ids(state, parse_python_list_ints_ordered(hole_ids_line));
  }
  set_current_route_metadata(state, layout_id, grade_id, "dataset", token_list);
  set_status(state,
             "Dataset route loaded: " + std::to_string(state->route_holes.size()) +
                 " holds (" + format_role_counts(state) + ").");
  gtk_widget_queue_draw(state->drawing_area);
}

void refresh_history(AppState* state) {
  if (state->history_label == nullptr) {
    return;
  }
  const std::vector<SavedRoute> rows = load_recent_routes(state->results_db_path, 10);
  if (rows.empty()) {
    gtk_label_set_text(GTK_LABEL(state->history_label),
                       "No saved routes yet. Generate one, rate it, and save.");
    return;
  }
  std::stringstream ss;
  for (const SavedRoute& r : rows) {
    ss << "#" << r.id << "  " << r.created_at
       << "  L" << r.layout_id << "/G" << r.grade_id
       << "  [" << r.source << "]"
       << "  " << r.hole_count << " holds"
       << "  rating " << r.rating << "/5\n";
  }
  std::string text = ss.str();
  if (!text.empty() && text.back() == '\n') {
    text.pop_back();
  }
  gtk_label_set_text(GTK_LABEL(state->history_label), text.c_str());
}

static void persist_current_route(AppState* state) {
  SavedRoute route;
  route.created_at = now_timestamp();
  route.layout_id = state->current_layout_id;
  route.grade_id = state->current_grade_id;
  route.source = state->current_source.empty() ? "model" : state->current_source;
  route.rating = static_cast<int>(
      gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state->rating_spin)));
  route.hole_count = static_cast<int>(state->route_holes.size());
  route.tokens = join_strings(state->route_tokens, ',');
  route.hole_ids = join_ints(state->route_sequence, ',');

  std::string err;
  if (!save_route_to_db(state->results_db_path, route, err)) {
    set_status(state, "Save failed: " + err);
    return;
  }
  set_status(state, "Saved route to local DB (rating " + std::to_string(route.rating) +
                        "/5, " + std::to_string(route.hole_count) + " holds).");
  refresh_history(state);
}

static void on_save_confirm_response(GObject* source, GAsyncResult* result, gpointer user_data) {
  auto* state = static_cast<AppState*>(user_data);
  GError* error = nullptr;
  const int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, &error);
  if (error != nullptr) {
    g_error_free(error);
    return;
  }
  if (choice == 1) {
    persist_current_route(state);
  } else {
    set_status(state, "Save cancelled.");
  }
}

static void on_save_clicked(GtkButton* button, gpointer user_data) {
  (void)button;
  auto* state = static_cast<AppState*>(user_data);

  if (!state->has_route || state->route_sequence.empty()) {
    set_status(state, "No route to save. Generate or load a route first.");
    return;
  }

  const int rating = static_cast<int>(
      gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state->rating_spin)));

  std::stringstream detail;
  detail << "Save this route to the local database?\n\n"
         << "Board (layout): " << state->current_layout_id << "\n"
         << "Grade: " << state->current_grade_id << "\n"
         << "Source: " << (state->current_source.empty() ? "model" : state->current_source) << "\n"
         << "Holds: " << state->route_holes.size() << "\n"
         << "Your rating: " << rating << "/5";

  GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", "Confirm save");
  gtk_alert_dialog_set_detail(dialog, detail.str().c_str());
  const char* buttons[] = {"Cancel", "Save", nullptr};
  gtk_alert_dialog_set_buttons(dialog, buttons);
  gtk_alert_dialog_set_cancel_button(dialog, 0);
  gtk_alert_dialog_set_default_button(dialog, 1);
  gtk_alert_dialog_choose(dialog, GTK_WINDOW(state->window), nullptr,
                          on_save_confirm_response, state);
  g_object_unref(dialog);
}

static GtkWidget* labeled_entry(const char* label, GtkWidget** out_entry, const char* default_text) {
  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget* lbl = gtk_label_new(label);
  GtkWidget* entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(entry), default_text);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_box_append(GTK_BOX(row), lbl);
  gtk_box_append(GTK_BOX(row), entry);
  *out_entry = entry;
  return row;
}

static GtkWidget* labeled_combo(
    const char* label,
    GtkWidget** out_combo,
    const std::vector<std::pair<int, std::string>>& options,
    int default_id) {
  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget* lbl = gtk_label_new(label);
  GtkWidget* combo = gtk_combo_box_text_new();
  gtk_widget_set_hexpand(combo, TRUE);
  for (const auto& kv : options) {
    const std::string id_str = std::to_string(kv.first);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), id_str.c_str(), kv.second.c_str());
  }
  gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), std::to_string(default_id).c_str());
  gtk_box_append(GTK_BOX(row), lbl);
  gtk_box_append(GTK_BOX(row), combo);
  *out_combo = combo;
  return row;
}

static GtkWidget* legend_item(const char* text, const char* color_hex) {
  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget* dot = gtk_label_new("●");
  GtkWidget* lbl = gtk_label_new(text);
  std::string markup = std::string("<span foreground=\"") + color_hex + "\">●</span>";
  gtk_label_set_use_markup(GTK_LABEL(dot), TRUE);
  gtk_label_set_markup(GTK_LABEL(dot), markup.c_str());
  gtk_box_append(GTK_BOX(row), dot);
  gtk_box_append(GTK_BOX(row), lbl);
  return row;
}

void on_activate(GtkApplication* app, gpointer user_data) {
  auto* state = static_cast<AppState*>(user_data);

  GtkWidget* window = gtk_application_window_new(app);
  state->window = window;
  gtk_window_set_title(GTK_WINDOW(window), "Kilter Route GUI (Offline)");
  gtk_window_set_default_size(GTK_WINDOW(window), 1100, 760);

  GtkWidget* root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_top(root, 12);
  gtk_widget_set_margin_bottom(root, 12);
  gtk_widget_set_margin_start(root, 12);
  gtk_widget_set_margin_end(root, 12);
  gtk_window_set_child(GTK_WINDOW(window), root);

  GtkCssProvider* provider = gtk_css_provider_new();
  const char* css =
      ".left-panel { background: #17243A; border-radius: 10px; padding: 16px; }"
      ".right-panel { background: #17243A; border-radius: 10px; padding: 14px; }"
      ".title-main { font-size: 42px; font-weight: 800; color: #a3f4d4; }"
      ".status-box { background: #2d3748; border-radius: 8px; padding: 8px; }";
  gtk_css_provider_load_from_string(provider, css);
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(),
      GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  GtkWidget* left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_add_css_class(left_panel, "left-panel");
  gtk_widget_set_size_request(left_panel, 420, -1);
  gtk_box_append(GTK_BOX(root), left_panel);

  GtkWidget* title = gtk_label_new("GENCLIMB");
  gtk_widget_add_css_class(title, "title-main");
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  gtk_box_append(GTK_BOX(left_panel), title);

  GtkWidget* subtitle = gtk_label_new("Offline local inference (DB + checkpoint)");
  gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0f);
  gtk_box_append(GTK_BOX(left_panel), subtitle);

  gtk_box_append(GTK_BOX(left_panel),
                 labeled_combo("Board", &state->layout_combo, state->layout_tokens, 1270));
  gtk_box_append(GTK_BOX(left_panel),
                 labeled_combo("Grade", &state->grade_combo, state->grade_tokens, 1239));

  if (!state->available_checkpoints.empty()) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* lbl = gtk_label_new("Checkpoint");
    GtkWidget* combo = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(combo, TRUE);
    for (const auto& path : state->available_checkpoints) {
      const std::string display = fs::path(path).filename().string();
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), path.c_str(), display.c_str());
    }
    if (!state->checkpoint_path.empty()) {
      gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), state->checkpoint_path.c_str());
    }
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(combo)) < 0) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    }
    gtk_box_append(GTK_BOX(row), lbl);
    gtk_box_append(GTK_BOX(row), combo);
    gtk_box_append(GTK_BOX(left_panel), row);
    state->checkpoint_combo = combo;
  } else {
    gtk_box_append(GTK_BOX(left_panel),
                   labeled_entry("Checkpoint", &state->checkpoint_entry,
                                 state->checkpoint_path.c_str()));
  }

  GtkWidget* generate_btn = gtk_button_new_with_label("Generate Route + Render");
  gtk_box_append(GTK_BOX(left_panel), generate_btn);
  g_signal_connect(generate_btn, "clicked", G_CALLBACK(on_generate_clicked), state);

  GtkWidget* random_btn = gtk_button_new_with_label("Load Random Route from Dataset");
  gtk_box_append(GTK_BOX(left_panel), random_btn);
  g_signal_connect(random_btn, "clicked", G_CALLBACK(on_random_dataset_clicked), state);

  GtkWidget* advanced_expander = gtk_expander_new("Advanced Settings");
  gtk_expander_set_expanded(GTK_EXPANDER(advanced_expander), FALSE);
  GtkWidget* advanced_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_append(GTK_BOX(advanced_box), labeled_entry("Temperature", &state->temperature_entry, "1.15"));
  gtk_box_append(GTK_BOX(advanced_box), labeled_entry("Top-k", &state->topk_entry, "40"));
  gtk_box_append(GTK_BOX(advanced_box), labeled_entry("Top-p", &state->topp_entry, "0.95"));
  gtk_box_append(GTK_BOX(advanced_box),
                 labeled_entry("Repetition penalty", &state->repetition_penalty_entry, "1.1"));
  state->greedy_check = gtk_check_button_new_with_label("Use greedy decoding (deterministic)");
  gtk_check_button_set_active(GTK_CHECK_BUTTON(state->greedy_check), FALSE);
  gtk_box_append(GTK_BOX(advanced_box), state->greedy_check);
  gtk_expander_set_child(GTK_EXPANDER(advanced_expander), advanced_box);
  gtk_box_append(GTK_BOX(left_panel), advanced_expander);

  GtkWidget* rating_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget* rating_lbl = gtk_label_new("Your rating (1-5)");
  state->rating_spin = gtk_spin_button_new_with_range(1, 5, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(state->rating_spin), 4);
  gtk_widget_set_hexpand(state->rating_spin, TRUE);
  gtk_box_append(GTK_BOX(rating_row), rating_lbl);
  gtk_box_append(GTK_BOX(rating_row), state->rating_spin);
  gtk_box_append(GTK_BOX(left_panel), rating_row);

  state->save_btn = gtk_button_new_with_label("Save route + rating to DB");
  gtk_box_append(GTK_BOX(left_panel), state->save_btn);
  g_signal_connect(state->save_btn, "clicked", G_CALLBACK(on_save_clicked), state);

  GtkWidget* history_title = gtk_label_new("Recently saved routes (local DB)");
  gtk_label_set_xalign(GTK_LABEL(history_title), 0.0f);
  gtk_widget_set_margin_top(history_title, 6);
  gtk_box_append(GTK_BOX(left_panel), history_title);

  GtkWidget* history_scroller = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(history_scroller, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroller),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  state->history_label = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(state->history_label), 0.0f);
  gtk_label_set_yalign(GTK_LABEL(state->history_label), 0.0f);
  gtk_label_set_wrap(GTK_LABEL(state->history_label), TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(history_scroller), state->history_label);
  gtk_box_append(GTK_BOX(left_panel), history_scroller);

  GtkWidget* right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_hexpand(right_panel, TRUE);
  gtk_widget_set_vexpand(right_panel, TRUE);
  gtk_widget_add_css_class(right_panel, "right-panel");
  gtk_box_append(GTK_BOX(root), right_panel);

  state->drawing_area = gtk_drawing_area_new();
  gtk_widget_set_vexpand(state->drawing_area, TRUE);
  gtk_widget_set_hexpand(state->drawing_area, TRUE);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(state->drawing_area), draw_board, state, nullptr);
  gtk_box_append(GTK_BOX(right_panel), state->drawing_area);

  GtkWidget* legend = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_box_append(GTK_BOX(legend), legend_item("Start (r12)", "#1ae935"));
  gtk_box_append(GTK_BOX(legend), legend_item("Middle (r13)", "#15d8f8"));
  gtk_box_append(GTK_BOX(legend), legend_item("Finish (r14)", "#d732ff"));
  gtk_box_append(GTK_BOX(legend), legend_item("Foot (r15)", "#ffa31a"));
  gtk_box_append(GTK_BOX(right_panel), legend);

  GtkWidget* status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_add_css_class(status_box, "status-box");
  gtk_box_append(GTK_BOX(right_panel), status_box);

  state->status_label = gtk_label_new(state->status.c_str());
  gtk_label_set_xalign(GTK_LABEL(state->status_label), 0.0f);
  gtk_box_append(GTK_BOX(status_box), state->status_label);

  refresh_history(state);
  gtk_window_present(GTK_WINDOW(window));
}
