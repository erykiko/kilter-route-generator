#pragma once

#include "types.hpp"

#include <gtk/gtk.h>

#include <string>
#include <utility>
#include <vector>

void set_status(AppState* state, const std::string& text);
void draw_board(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
void refresh_history(AppState* state);
void on_activate(GtkApplication* app, gpointer user_data);
