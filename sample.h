#pragma once

#include <string>

struct Sample
{
  std::string filepath;
  long long size;
  double duration;
  int sample_rate;
  int bit_depth;
  int channels;
  std::string tags;
};
