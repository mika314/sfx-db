#pragma once

#include <cstdint>
#include <mutex>
#include <queue>

extern "C" {
#include <libavutil/avutil.h>
}

extern std::queue<uint8_t *> audio_buffer_queue;
extern std::queue<int> audio_buffer_size_queue;
extern std::mutex audio_mutex;

void audio_callback(uint8_t *stream, int len);
