#include "results_db.hpp"

#include <sqlite3.h>

#include <filesystem>

namespace fs = std::filesystem;

bool init_results_db(const std::string& db_path, std::string& error) {
  std::error_code ec;
  fs::create_directories(fs::path(db_path).parent_path(), ec);

  sqlite3* db = nullptr;
  if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
    error = "Could not open results DB: " + std::string(sqlite3_errmsg(db));
    if (db) sqlite3_close(db);
    return false;
  }
  const char* sql =
      "CREATE TABLE IF NOT EXISTS saved_routes ("
      " id INTEGER PRIMARY KEY AUTOINCREMENT,"
      " created_at TEXT NOT NULL,"
      " layout_id INTEGER NOT NULL,"
      " grade_id INTEGER NOT NULL,"
      " source TEXT NOT NULL,"
      " rating INTEGER NOT NULL,"
      " hole_count INTEGER NOT NULL,"
      " tokens TEXT NOT NULL,"
      " hole_ids TEXT NOT NULL"
      ");";
  char* err_msg = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    error = "Could not create table: " + std::string(err_msg ? err_msg : "?");
    sqlite3_free(err_msg);
    sqlite3_close(db);
    return false;
  }
  sqlite3_close(db);
  return true;
}

bool save_route_to_db(const std::string& db_path, const SavedRoute& route, std::string& error) {
  sqlite3* db = nullptr;
  if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
    error = "Could not open results DB: " + std::string(sqlite3_errmsg(db));
    if (db) sqlite3_close(db);
    return false;
  }
  const char* sql =
      "INSERT INTO saved_routes"
      " (created_at, layout_id, grade_id, source, rating, hole_count, tokens, hole_ids)"
      " VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    error = "Prepare insert failed: " + std::string(sqlite3_errmsg(db));
    sqlite3_close(db);
    return false;
  }
  sqlite3_bind_text(stmt, 1, route.created_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, route.layout_id);
  sqlite3_bind_int(stmt, 3, route.grade_id);
  sqlite3_bind_text(stmt, 4, route.source.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, route.rating);
  sqlite3_bind_int(stmt, 6, route.hole_count);
  sqlite3_bind_text(stmt, 7, route.tokens.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, route.hole_ids.c_str(), -1, SQLITE_TRANSIENT);

  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok) {
    error = "Insert failed: " + std::string(sqlite3_errmsg(db));
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok;
}

std::vector<SavedRoute> load_recent_routes(const std::string& db_path, int limit) {
  std::vector<SavedRoute> out;
  sqlite3* db = nullptr;
  if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return out;
  }
  const char* sql =
      "SELECT id, created_at, layout_id, grade_id, source, rating, hole_count"
      " FROM saved_routes ORDER BY id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return out;
  }
  sqlite3_bind_int(stmt, 1, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SavedRoute r;
    r.id = sqlite3_column_int(stmt, 0);
    const unsigned char* ts = sqlite3_column_text(stmt, 1);
    r.created_at = ts ? reinterpret_cast<const char*>(ts) : "";
    r.layout_id = sqlite3_column_int(stmt, 2);
    r.grade_id = sqlite3_column_int(stmt, 3);
    const unsigned char* src = sqlite3_column_text(stmt, 4);
    r.source = src ? reinterpret_cast<const char*>(src) : "";
    r.rating = sqlite3_column_int(stmt, 5);
    r.hole_count = sqlite3_column_int(stmt, 6);
    out.push_back(std::move(r));
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return out;
}
