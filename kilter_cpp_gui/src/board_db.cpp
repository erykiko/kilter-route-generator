#include "board_db.hpp"

#include <sqlite3.h>

int product_size_id_from_layout_token(int layout_token_id) {
  switch (layout_token_id) {
    case 1263: return 8;
    case 1264: return 14;
    case 1265: return 17;
    case 1266: return 18;
    case 1267: return 19;
    case 1268: return 21;
    case 1269: return 22;
    case 1270: return 7;
    case 1271: return 10;
    case 1272: return 27;
    case 1273: return 28;
    case 1274: return 26;
    case 1275: return 25;
    case 1276: return 24;
    case 1277: return 23;
    case 1278: return 29;
    default: return -1;
  }
}

bool load_holes_from_db(const std::string& db_path, int product_size_id,
                        std::map<int, Hole>& out_holes, std::string& error) {
  sqlite3* db = nullptr;
  if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
    error = "Could not open DB: " + std::string(sqlite3_errmsg(db));
    if (db) {
      sqlite3_close(db);
    }
    return false;
  }

  const char* sql =
      "SELECT DISTINCT h.id, h.x, h.y "
      "FROM product_sizes_layouts_sets psls "
      "JOIN placements p ON p.layout_id = psls.layout_id AND p.set_id = psls.set_id "
      "JOIN holes h ON h.id = p.hole_id "
      "WHERE psls.product_size_id = ?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    error = "Could not query holes: " + std::string(sqlite3_errmsg(db));
    sqlite3_close(db);
    return false;
  }

  if (sqlite3_bind_int(stmt, 1, product_size_id) != SQLITE_OK) {
    error = "Could not bind product size id";
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return false;
  }

  out_holes.clear();
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Hole h;
    h.id = sqlite3_column_int(stmt, 0);
    h.x = static_cast<double>(sqlite3_column_int(stmt, 1));
    h.y = static_cast<double>(sqlite3_column_int(stmt, 2));
    out_holes[h.id] = h;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return true;
}
