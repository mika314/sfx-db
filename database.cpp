#include "database.h"
#include "audio_player.h"
#include <filesystem>
#include <log/log.hpp>

Database::Database(const std::string &db_path)
{
  int rc = sqlite3_open(db_path.c_str(), &db_);
  if (rc)
  {
    LOG("Can't open database:", sqlite3_errmsg(db_));
    throw std::runtime_error("Failed to open database");
  }
  else
  {
    LOG("Opened database successfully");
  }

  char *zErrMsg = 0;
  const char *sql = "CREATE TABLE IF NOT EXISTS samples ("
                    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "filepath TEXT NOT NULL,"
                    "filename TEXT NOT NULL,"
                    "size INT NOT NULL,"
                    "duration REAL NOT NULL,"
                    "samplerate INT NOT NULL,"
                    "bitdepth INT NOT NULL,"
                    "channels INT NOT NULL,"
                    "tags TEXT);";

  rc = sqlite3_exec(db_, sql, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK)
  {
    LOG("SQL error:", zErrMsg);
    sqlite3_free(zErrMsg);
    throw std::runtime_error("Failed to create table");
  }
  else
  {
    LOG("Table created successfully");
  }
}

Database::~Database()
{
  sqlite3_close(db_);
}

void Database::load_samples(std::vector<Sample> &samples_data, std::string where)
{
  samples_data.clear();
  const std::string select_sql =
    "SELECT filepath, filename, size, duration, samplerate, bitdepth, channels, tags FROM samples" +
    (!where.empty() ? (" WHERE " + where) : std::string{}) + ";";
  sqlite3_stmt *stmt;
  int rc_select = sqlite3_prepare_v2(db_, select_sql.c_str(), -1, &stmt, 0);
  if (rc_select != SQLITE_OK)
  {
    LOG("SQL error preparing select:", sqlite3_errmsg(db_));
  }
  else
  {
    while ((rc_select = sqlite3_step(stmt)) == SQLITE_ROW)
    {
      Sample s;
      s.filepath = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      s.filename = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      s.size = sqlite3_column_int64(stmt, 2);
      s.duration = sqlite3_column_double(stmt, 3);
      s.sample_rate = sqlite3_column_int(stmt, 4);
      s.bit_depth = sqlite3_column_int(stmt, 5);
      s.channels = sqlite3_column_int(stmt, 6);
      s.tags = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
      samples_data.push_back(s);
    }
    if (rc_select != SQLITE_DONE)
    {
      LOG("SQL error selecting data:", sqlite3_errmsg(db_));
    }
    sqlite3_finalize(stmt);
  }
}

void Database::insert_sample(const Sample &sample)
{
  std::string insert_sql = "INSERT INTO samples (filepath, filename, size, duration, samplerate, "
                           "bitdepth, channels, tags) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, insert_sql.c_str(), -1, &stmt, 0);
  if (rc != SQLITE_OK)
  {
    LOG("SQL error preparing insert:", sqlite3_errmsg(db_));
  }
  else
  {
    sqlite3_bind_text(stmt, 1, sample.filepath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, sample.filename.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, sample.size);
    sqlite3_bind_double(stmt, 4, sample.duration);
    sqlite3_bind_int(stmt, 5, sample.sample_rate);
    sqlite3_bind_int(stmt, 6, sample.bit_depth);
    sqlite3_bind_int(stmt, 7, sample.channels);
    sqlite3_bind_text(stmt, 8, sample.tags.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
      LOG("SQL error inserting data:", sqlite3_errmsg(db_));
    }
    else
    {
      LOG("Sample inserted successfully.");
    }
    sqlite3_finalize(stmt);
  }
}

void Database::scan_directory(const std::string &directory_path)
{
  LOG("Scanning directory:", directory_path);
  for (const auto &entry : std::filesystem::recursive_directory_iterator(directory_path))
  {
    if (!entry.is_regular_file())
      continue;
    std::string filepath = entry.path().string();
    LOG("Found file:", filepath);
    auto new_sample = AudioPlayer::extract_meta_data(filepath.c_str());
    if (new_sample.filepath.empty())
    {
      LOG("No audio stream found in:", filepath, "Not inserting into database.");
      continue;
    }
    insert_sample(new_sample);
  }
}
