#pragma once

#include "sample.h"
#include <sqlite3.h>
#include <string>
#include <vector>

class Database
{
public:
  Database(const std::string &db_path);
  ~Database();
  void load_samples(std::vector<Sample> &samples_data, std::string where = {});
  void insert_sample(const Sample &sample);
  void scan_directory(const std::string &directory_path);

private:
  sqlite3 *db_;
};
