#include "database.h"
#include <filesystem>
#include <log/log.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

Database::Database(const std::string &db_path)
{
  int rc = sqlite3_open(db_path.c_str(), &db_);
  if (rc)
  {
    LOG("Can't open database: {}", sqlite3_errmsg(db_));
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
    LOG("SQL error: {}", zErrMsg);
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

void Database::load_samples(std::vector<Sample> &samples_data)
{
  samples_data.clear();
  const char *select_sql =
    "SELECT filepath, filename, size, duration, samplerate, bitdepth, channels, tags FROM samples;";
  sqlite3_stmt *stmt;
  int rc_select = sqlite3_prepare_v2(db_, select_sql, -1, &stmt, 0);
  if (rc_select != SQLITE_OK)
  {
    LOG("SQL error preparing select: {}", sqlite3_errmsg(db_));
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
      LOG("SQL error selecting data: {}", sqlite3_errmsg(db_));
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
    LOG("SQL error preparing insert: {}", sqlite3_errmsg(db_));
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
      LOG("SQL error inserting data: {}", sqlite3_errmsg(db_));
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
  LOG("Scanning directory: {}", directory_path);
  for (const auto &entry : std::filesystem::recursive_directory_iterator(directory_path))
  {
    if (entry.is_regular_file())
    {
      std::string filepath = entry.path().string();
      LOG("Found file: {}", filepath);

      Sample new_sample;
      new_sample.filepath = filepath;
      new_sample.filename = entry.path().filename().string();

      try
      {
        new_sample.size = std::filesystem::file_size(entry.path());
      }
      catch (const std::filesystem::filesystem_error &e)
      {
        LOG("Error getting file size: {}", e.what());
        new_sample.size = 0;
      }

      AVFormatContext *pFormatCtx = NULL;
      double duration = 0.0;
      int sample_rate = 0;
      int channels = 0;
      int bit_depth = 0;
      int audio_stream_idx = -1;

      if (avformat_open_input(&pFormatCtx, filepath.c_str(), NULL, NULL) != 0)
      {
        LOG("Couldn't open input stream for {}.", filepath);
      }
      else
      {
        if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        {
          LOG("Couldn't find stream information for {}.", filepath);
        }
        else
        {
          duration = (double)pFormatCtx->duration / AV_TIME_BASE;
          for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
          {
            if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
              audio_stream_idx = i;
              sample_rate = pFormatCtx->streams[i]->codecpar->sample_rate;
              channels = pFormatCtx->streams[i]->codecpar->channels;
              bit_depth = av_get_bits_per_sample(pFormatCtx->streams[i]->codecpar->codec_id);
              break;
            }
          }
        }
        avformat_close_input(&pFormatCtx);
      }

      if (audio_stream_idx != -1)
      { // Only insert if audio stream found
        new_sample.duration = duration;
        new_sample.sample_rate = sample_rate;
        new_sample.bit_depth = bit_depth;
        new_sample.channels = channels;
        new_sample.tags = ""; // Empty tags for now
        insert_sample(new_sample);
      }
      else
      {
        LOG("No audio stream found in {}. Not inserting into database.", filepath);
      }
    }
  }
}
