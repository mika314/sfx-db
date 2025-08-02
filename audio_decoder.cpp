#include "audio_decoder.h"
#include "audio_player.h"
#include "sample.h"
#include <SDL.h>
#include <log/log.hpp>

void play_audio_sample(const Sample &sample)
{
  // Clear any existing audio in the queue
  std::lock_guard<std::mutex> lock(audio_mutex);
  while (!audio_buffer_queue.empty())
  {
    av_freep(&audio_buffer_queue.front());
    audio_buffer_queue.pop();
    audio_buffer_size_queue.pop();
  }

  AVFormatContext *pFormatCtx = NULL;
  AVCodecContext *pCodecCtx = NULL;
  AVCodec *pCodec = NULL;
  AVPacket packet;
  AVFrame *pFrame = NULL;
  SwrContext *swr_ctx = NULL;

  const char *filepath = sample.filepath.c_str();

  if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0)
  {
    LOG("Couldn't open input stream.");
  }
  else if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
  {
    LOG("Couldn't find stream information.");
  }
  else
  {
    int audioStream = -1;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
    {
      if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        audioStream = i;
        break;
      }
    }
    if (audioStream == -1)
    {
      LOG("Didn't find a audio stream.");
    }
    else
    {
      pCodec = avcodec_find_decoder(pFormatCtx->streams[audioStream]->codecpar->codec_id);
      if (!pCodec)
      {
        LOG("Unsupported codec!");
      }
      else
      {
        pCodecCtx = avcodec_alloc_context3(pCodec);
        if (!pCodecCtx)
        {
          LOG("Could not allocate audio codec context.");
        }
        else if (avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioStream]->codecpar) <
                 0)
        {
          LOG("Could not copy codec parameters to context.");
        }
        else
        {
          LOG("After avcodec_parameters_to_context: channel_layout=",
              pCodecCtx->channel_layout,
              ", sample_fmt=",
              pCodecCtx->sample_fmt,
              ", sample_rate=",
              pCodecCtx->sample_rate,
              ", channels=",
              pCodecCtx->channels);
          // Set default channel layout if not already set
          if (pCodecCtx->channel_layout == 0)
          {
            pCodecCtx->channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
          }

          if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
          {
            LOG("Could not open codec.");
          }
          else
          {
            LOG("After avcodec_open2: channel_layout=",
                pCodecCtx->channel_layout,
                ", sample_fmt=",
                pCodecCtx->sample_fmt,
                ", sample_rate=",
                pCodecCtx->sample_rate,
                ", channels=",
                pCodecCtx->channels);
            pFrame = av_frame_alloc();
            if (!pFrame)
            {
              LOG("Could not allocate audio frame.");
            }
            else
            {
              swr_ctx = swr_alloc();
              if (!swr_ctx)
              {
                LOG("Could not allocate SwrContext.");
              }
              else
              {
                // Set up resampler to output S16_LE, 2 channels, 44100 Hz
                uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
                AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
                int out_sample_rate = 44100;

                // Convert planar to packed if necessary for input sample format
                AVSampleFormat in_sample_fmt = pCodecCtx->sample_fmt;
                if (av_sample_fmt_is_planar(in_sample_fmt))
                {
                  in_sample_fmt = av_get_packed_sample_fmt(in_sample_fmt);
                }

                LOG("Input: channel_layout=",
                    pCodecCtx->channel_layout,
                    ", sample_fmt=",
                    pCodecCtx->sample_fmt,
                    ", sample_rate=",
                    pCodecCtx->sample_rate);
                LOG("Output: channel_layout=",
                    out_channel_layout,
                    ", sample_fmt=",
                    out_sample_fmt,
                    ", sample_rate=",
                    out_sample_rate);

                swr_ctx =
                  swr_alloc_set_opts(swr_ctx,
                                     out_channel_layout,
                                     out_sample_fmt,
                                     out_sample_rate,
                                     pCodecCtx->channel_layout,
                                     in_sample_fmt, // Use the potentially converted packed format
                                     pCodecCtx->sample_rate,
                                     0,
                                     NULL);
                if (swr_init(swr_ctx) < 0)
                {
                  LOG("Failed to initialize SwrContext.");
                }
                else
                {
                  while (av_read_frame(pFormatCtx, &packet) >= 0)
                  {
                    if (packet.stream_index == audioStream)
                    {
                      if (avcodec_send_packet(pCodecCtx, &packet) >= 0)
                      {
                        while (avcodec_receive_frame(pCodecCtx, pFrame) >= 0)
                        {
                          uint8_t *out_buffer = nullptr;
                          int out_samples;

                          if (av_samples_alloc(&out_buffer,
                                               nullptr,
                                               pFrame->channels,
                                               pFrame->nb_samples * 2,
                                               AV_SAMPLE_FMT_S16,
                                               1) < 0)
                          {
                            LOG("Could not allocate output buffer.");
                            av_packet_unref(&packet);
                            continue;
                          }

                          out_samples = swr_convert(swr_ctx,
                                                    &out_buffer,
                                                    pFrame->nb_samples * 2, // Max output samples
                                                    (const uint8_t **)pFrame->data,
                                                    pFrame->nb_samples);

                          if (out_samples < 0)
                          {
                            LOG("Error while resampling.");
                            av_freep(&out_buffer);
                            break;
                          }

                          int out_buffer_size =
                            av_samples_get_buffer_size(NULL, 2, out_samples, AV_SAMPLE_FMT_S16, 1);
                          uint8_t *new_buffer = (uint8_t *)av_malloc(out_buffer_size);
                          SDL_memcpy(new_buffer, out_buffer, out_buffer_size);
                          audio_buffer_queue.push(new_buffer);
                          audio_buffer_size_queue.push(out_buffer_size);
                          av_freep(&out_buffer);
                        }
                      }
                    }
                    av_packet_unref(&packet);
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (pFormatCtx)
    avformat_close_input(&pFormatCtx);
  if (pCodecCtx)
    avcodec_free_context(&pCodecCtx);
  if (pFrame)
    av_frame_free(&pFrame);
  if (swr_ctx)
    swr_free(&swr_ctx);
}
