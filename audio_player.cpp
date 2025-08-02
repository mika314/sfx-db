#include "audio_player.h"
#include <SDL.h>
#include <algorithm>

std::queue<uint8_t *> audio_buffer_queue;
std::queue<int> audio_buffer_size_queue;
std::mutex audio_mutex;

void audio_callback(uint8_t *stream, int len)
{
  std::lock_guard<std::mutex> lock(audio_mutex);
  static int current_buffer_read_offset = 0;
  int stream_fill_pos = 0;

  while (stream_fill_pos < len)
  {
    if (audio_buffer_queue.empty())
    {
      // No more audio to play, fill remaining `stream` with zeros
      SDL_memset(stream + stream_fill_pos, 0, len - stream_fill_pos);
      break; // All of `stream` is filled
    }

    uint8_t *original_buffer_ptr = audio_buffer_queue.front();
    int original_buffer_total_size = audio_buffer_size_queue.front();

    // Calculate remaining data in the current buffer from the read offset
    int remaining_in_current_buffer = original_buffer_total_size - current_buffer_read_offset;

    // Calculate how much data to copy in this iteration
    uint32_t bytes_to_copy = std::min((uint32_t)(len - stream_fill_pos), (uint32_t)remaining_in_current_buffer);

    // Copy data from the current read offset of the buffer to the SDL stream
    SDL_memcpy(stream + stream_fill_pos, original_buffer_ptr + current_buffer_read_offset, bytes_to_copy);

    stream_fill_pos += bytes_to_copy; // Advance position in `stream`
    current_buffer_read_offset += bytes_to_copy; // Advance read offset in current buffer

    if (current_buffer_read_offset == original_buffer_total_size)
    {
      // Current buffer fully consumed, free the original pointer and pop from queues
      av_freep(&original_buffer_ptr); // Free the original pointer
      audio_buffer_queue.pop();
      audio_buffer_size_queue.pop();
      current_buffer_read_offset = 0; // Reset offset for the next buffer
    }
  }
}
