#pragma once

#include <SDL.h>
#include <cstdint>
#include <mutex>
#include <queue>

extern "C" {
#include <libavutil/avutil.h>
}

struct Sample;

class AudioPlayerManager
{
public:
  AudioPlayerManager();
  ~AudioPlayerManager();

  void push_buffer(uint8_t *buffer, int size);
  void clear_buffers();
  void audio_callback(Uint8 *stream, int len);
  void play_audio_sample(const Sample &sample);

private:
  std::queue<uint8_t *> m_audio_buffer_queue;
  std::queue<int> m_audio_buffer_size_queue;
  std::mutex m_audio_mutex;
};
