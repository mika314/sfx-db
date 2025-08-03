#include "audio_player.h"
#include "sample.h"
#include <SDL.h>
#include <algorithm>
#include <log/log.hpp>

#include "miniaudio.h"

AudioPlayer::AudioPlayer()
  : wanted_spec{.freq = 44100, .format = AUDIO_S16SYS, .channels = 2, .samples = 4096},
    audio_device(sdl::Audio{NULL, 0, &wanted_spec, &audio_spec, 0, [&](Uint8 *stream, int len) {
                              audio_callback(stream, len);
                            }})

{
  audio_device.pause(0); // Start audio playback
}

AudioPlayer::~AudioPlayer()
{
  // Clear any remaining buffers in the queue
  std::lock_guard<std::mutex> lock(m_audio_mutex);
  while (!m_audio_buffer_queue.empty())
  {
    uint8_t *buffer = m_audio_buffer_queue.front();
    ma_free(buffer, NULL);
    m_audio_buffer_queue.pop();
    m_audio_buffer_size_queue.pop();
  }
}

void AudioPlayer::push_buffer(uint8_t *buffer, int size)
{
  std::lock_guard<std::mutex> lock(m_audio_mutex);
  m_audio_buffer_queue.push(buffer);
  m_audio_buffer_size_queue.push(size);
}

void AudioPlayer::clear_buffers()
{
  std::lock_guard<std::mutex> lock(m_audio_mutex);
  while (!m_audio_buffer_queue.empty())
  {
    uint8_t *buffer = m_audio_buffer_queue.front();
    ma_free(buffer, NULL);
    m_audio_buffer_queue.pop();
    m_audio_buffer_size_queue.pop();
  }
  current_buffer_read_offset = 0;
}

void AudioPlayer::audio_callback(uint8_t *stream, int len)
{
  std::lock_guard<std::mutex> lock(m_audio_mutex);
  int stream_fill_pos = 0;

  while (stream_fill_pos < len)
  {
    if (m_audio_buffer_queue.empty())
    {
      // No more audio to play, fill remaining `stream` with zeros
      SDL_memset(stream + stream_fill_pos, 0, len - stream_fill_pos);
      break; // All of `stream` is filled
    }

    uint8_t *original_buffer_ptr = m_audio_buffer_queue.front();
    int original_buffer_total_size = m_audio_buffer_size_queue.front();

    // Calculate remaining data in the current buffer from the read offset
    int remaining_in_current_buffer = original_buffer_total_size - current_buffer_read_offset;

    // Calculate how much data to copy in this iteration
    uint32_t bytes_to_copy =
      std::min((uint32_t)(len - stream_fill_pos), (uint32_t)remaining_in_current_buffer);

    // Copy data from the current read offset of the buffer to the SDL stream
    SDL_memcpy(
      stream + stream_fill_pos, original_buffer_ptr + current_buffer_read_offset, bytes_to_copy);

    stream_fill_pos += bytes_to_copy;            // Advance position in `stream`
    current_buffer_read_offset += bytes_to_copy; // Advance read offset in current buffer

    if (current_buffer_read_offset == original_buffer_total_size)
    {
      // Current buffer fully consumed, free the original pointer and pop from queues
      ma_free(original_buffer_ptr, NULL); // Free the original pointer
      m_audio_buffer_queue.pop();
      m_audio_buffer_size_queue.pop();
      current_buffer_read_offset = 0; // Reset offset for the next buffer
    }
  }
}

void AudioPlayer::play_audio_sample(const Sample &sample)
{
  // Clear any existing audio in the queue
  clear_buffers();

  ma_result result;
  ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 2, 44100);
  ma_decoder decoder;

  result = ma_decoder_init_file(sample.filepath.c_str(), &config, &decoder);
  if (result != MA_SUCCESS)
  {
    LOG("Failed to open and decode audio file: ", sample.filepath.c_str());
    return;
  }

  ma_uint64 frameCount;
  ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);

  ma_uint64 bufferSize =
    frameCount * ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
  uint8_t *pcmData = (uint8_t *)ma_malloc(bufferSize, NULL);

  if (pcmData == NULL)
  {
    LOG("Failed to allocate memory for decoded audio.");
    ma_decoder_uninit(&decoder);
    return;
  }

  ma_uint64 framesRead;
  ma_decoder_read_pcm_frames(&decoder, pcmData, frameCount, &framesRead);
  if (framesRead < frameCount)
  {
    LOG("Warning: Could not read all frames from the file.");
  }

  push_buffer(pcmData,
              framesRead * ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels));

  ma_decoder_uninit(&decoder);
}

Sample AudioPlayer::extract_meta_data(const char *filepath)
{
  Sample new_sample;
  new_sample.filepath = filepath;
  std::filesystem::path p(new_sample.filepath);
  

  try
  {
    new_sample.size = std::filesystem::file_size(p);
  }
  catch (const std::filesystem::filesystem_error &e)
  {
    LOG("Error getting file size: ", e.what());
    new_sample.size = 0;
    return {};
  }

  ma_decoder decoder;
  ma_result result = ma_decoder_init_file(filepath, NULL, &decoder);
  if (result != MA_SUCCESS)
  {
    LOG("Failed to open audio file: ", filepath);
    return {};
  }

  ma_uint64 frame_count;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count) != MA_SUCCESS)
  {
    LOG("Failed to get duration for: ", filepath);
    ma_decoder_uninit(&decoder);
    return {};
  }

  new_sample.duration = (double)frame_count / decoder.outputSampleRate;
  new_sample.sample_rate = decoder.outputSampleRate;
  new_sample.channels = decoder.outputChannels;
  new_sample.bit_depth = ma_get_bytes_per_sample(decoder.outputFormat) * 8;
  new_sample.tags = ""; // Empty tags for now

  ma_decoder_uninit(&decoder);
  return new_sample;
}
