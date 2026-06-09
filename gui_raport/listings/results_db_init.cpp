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
  // ...
}
