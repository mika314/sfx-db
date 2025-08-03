#include "database.h"
#include "audio_player.h"
#include <filesystem>
#include <log/log.hpp>
#include <regex.h>

static void regexp(sqlite3_context *context, int /*argc*/, sqlite3_value **argv) {
    const char *pattern = (const char *)sqlite3_value_text(argv[0]);
    const char *text = (const char *)sqlite3_value_text(argv[1]);

    regex_t regex;
    int ret;

    if (pattern == NULL || text == NULL) {
        sqlite3_result_int(context, 0);
        return;
    }

    ret = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE | REG_NOSUB);
    if (ret != 0) {
        sqlite3_result_error(context, "Invalid regular expression", -1);
        return;
    }

    ret = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex);

    sqlite3_result_int(context, (ret == 0));
}

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

  // Register "REGEXP" function
  sqlite3_create_function(db_, "REGEXP", 2, SQLITE_UTF8, NULL, &regexp, NULL, NULL);

  char *zErrMsg = 0;
  const char *sql = "CREATE TABLE IF NOT EXISTS samples ("
                    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "filepath TEXT NOT NULL,"
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
    "SELECT filepath, size, duration, samplerate, bitdepth, channels, tags FROM samples" +
    (!where.empty() ? (" WHERE " + where) : std::string{}) + " ORDER BY filepath;";
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
      s.size = sqlite3_column_int64(stmt, 1);
      s.duration = sqlite3_column_double(stmt, 2);
      s.sample_rate = sqlite3_column_int(stmt, 3);
      s.bit_depth = sqlite3_column_int(stmt, 4);
      s.channels = sqlite3_column_int(stmt, 5);
      s.tags = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
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
  std::string insert_sql = "INSERT INTO samples (filepath, size, duration, samplerate, "
                           "bitdepth, channels, tags) VALUES (?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db_, insert_sql.c_str(), -1, &stmt, 0);
  if (rc != SQLITE_OK)
  {
    LOG("SQL error preparing insert:", sqlite3_errmsg(db_));
  }
  else
  {
    sqlite3_bind_text(stmt, 1, sample.filepath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, sample.size);
    sqlite3_bind_double(stmt, 3, sample.duration);
    sqlite3_bind_int(stmt, 4, sample.sample_rate);
    sqlite3_bind_int(stmt, 5, sample.bit_depth);
    sqlite3_bind_int(stmt, 6, sample.channels);
    sqlite3_bind_text(stmt, 7, sample.tags.c_str(), -1, SQLITE_STATIC);

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
