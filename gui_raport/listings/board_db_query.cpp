bool load_holes_from_db(const std::string& db_path, int product_size_id,
                        std::map<int, Hole>& out_holes, std::string& error) {
  sqlite3* db = nullptr;
  if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
    error = "Could not open DB: " + std::string(sqlite3_errmsg(db));
    if (db) sqlite3_close(db);
    return false;
  }

  const char* sql =
      "SELECT DISTINCT h.id, h.x, h.y "
      "FROM product_sizes_layouts_sets psls "
      "JOIN placements p ON p.layout_id = psls.layout_id AND p.set_id = psls.set_id "
      "JOIN holes h ON h.id = p.hole_id "
      "WHERE psls.product_size_id = ?";
  // sqlite3_prepare_v2, bind, step ...
}
