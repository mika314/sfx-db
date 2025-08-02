#pragma once

#include <cstdint>
#include <mutex>
#include <queue>
#include <sdlpp/sdlpp.hpp>

struct Sample;

class AudioPlayer
{
public:
  AudioPlayer();
  ~AudioPlayer();

  void play_audio_sample(const Sample &sample);
  static Sample extract_meta_data(const char *filepath);

private:
  SDL_AudioSpec wanted_spec, audio_spec;
  sdl::Audio audio_device;
  std::queue<uint8_t *> m_audio_buffer_queue;
  std::queue<int> m_audio_buffer_size_queue;
  std::mutex m_audio_mutex;
  int current_buffer_read_offset = 0;

  void push_buffer(uint8_t *buffer, int size);
  void clear_buffers();
  void audio_callback(Uint8 *stream, int len);
};
