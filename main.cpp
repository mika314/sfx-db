#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "imgui-impl-opengl3.h"
#include "imgui-impl-sdl.h"
#include <imgui/imgui.h>
#include <log/log.hpp>
#include <sdlpp/sdlpp.hpp>
#include <sqlite3.h>
#include "tinyfiledialogs.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

struct Sample {
    std::string filepath;
    std::string filename;
    long long size;
    double duration;
    int sample_rate;
    int bit_depth;
    int channels;
    std::string tags;
};

int main(int /*argc*/, char ** /*argv*/)
{
  try
  {
    sqlite3* db;
    int rc = sqlite3_open("sfx.db", &db);
    if (rc)
    {
        LOG("Can't open database: {}", sqlite3_errmsg(db));
        return 1;
    }
    else
    {
        LOG("Opened database successfully");
    }

    char* zErrMsg = 0;
    const char* sql = "CREATE TABLE IF NOT EXISTS samples ("  \
                      "ID INTEGER PRIMARY KEY AUTOINCREMENT," \
                      "filepath TEXT NOT NULL," \
                      "filename TEXT NOT NULL," \
                      "size INT NOT NULL," \
                      "duration REAL NOT NULL," \
                      "samplerate INT NOT NULL," \
                      "bitdepth INT NOT NULL," \
                      "channels INT NOT NULL," \
                      "tags TEXT);";

    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK)
    {
        LOG("SQL error: {}", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else
    {
        LOG("Table created successfully");
    }

    std::vector<Sample> samples_data;

    auto load_samples = [&](sqlite3* db_ptr) {
        samples_data.clear();
        const char* select_sql = "SELECT filepath, filename, size, duration, samplerate, bitdepth, channels, tags FROM samples;";
        sqlite3_stmt *stmt;
        int rc_select = sqlite3_prepare_v2(db_ptr, select_sql, -1, &stmt, 0);
        if (rc_select != SQLITE_OK) {
            LOG("SQL error preparing select: {}", sqlite3_errmsg(db_ptr));
        } else {
            while ((rc_select = sqlite3_step(stmt)) == SQLITE_ROW) {
                Sample s;
                s.filepath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                s.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                s.size = sqlite3_column_int64(stmt, 2);
                s.duration = sqlite3_column_double(stmt, 3);
                s.sample_rate = sqlite3_column_int(stmt, 4);
                s.bit_depth = sqlite3_column_int(stmt, 5);
                s.channels = sqlite3_column_int(stmt, 6);
                s.tags = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
                samples_data.push_back(s);
            }
            if (rc_select != SQLITE_DONE) {
                LOG("SQL error selecting data: {}", sqlite3_errmsg(db_ptr));
            }
            sqlite3_finalize(stmt);
        }
    };

    load_samples(db);

    sdl::Init sdl(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    sdl::Window window(
      "sfx-db", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL);

    auto gl_context = SDL_GL_CreateContext(window.get());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window.get(), gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    bool running = true;
    while (running)
    {
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
          running = false;
        }
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();

      if (ImGui::BeginMainMenuBar())
      {
        if (ImGui::BeginMenu("File"))
        {
          if (ImGui::MenuItem("Add Sample"))
          {
            char const * lTheOpenFileName = tinyfd_openFileDialog(
                "Select a sound file",
                "",
                0,
                NULL,
                NULL,
                0);
            if (lTheOpenFileName)
            {
                LOG("Selected file: {}", lTheOpenFileName);

                std::string filepath_str = lTheOpenFileName;
                std::filesystem::path p(filepath_str);
                std::string filename_str = p.filename().string();
                long long file_size = 0;
                try {
                    file_size = std::filesystem::file_size(p);
                } catch (const std::filesystem::filesystem_error& e) {
                    LOG("Error getting file size: {}", e.what());
                }

                AVFormatContext *pFormatCtx = NULL;
                double duration = 0.0;
                int sample_rate = 0;
                int channels = 0;
                int bit_depth = 0;
                int audio_stream_idx = -1;

                if (avformat_open_input(&pFormatCtx, lTheOpenFileName, NULL, NULL) != 0)
                {
                    LOG("Couldn't open input stream.");
                }
                else
                {
                    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
                    {
                        LOG("Couldn't find stream information.");
                    }
                    else
                    {
                        duration = (double)pFormatCtx->duration / AV_TIME_BASE;
                        LOG("Duration: {}", duration);
                        for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
                        {
                            if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                            {
                                audio_stream_idx = i;
                                sample_rate = pFormatCtx->streams[i]->codecpar->sample_rate;
                                channels = pFormatCtx->streams[i]->codecpar->channels;
                                bit_depth = av_get_bits_per_sample(pFormatCtx->streams[i]->codecpar->codec_id);
                                LOG("Sample Rate: {}", sample_rate);
                                LOG("Channels: {}", channels);
                                LOG("Bit Depth: {}", bit_depth);
                                break;
                            }
                        }
                    }
                    avformat_close_input(&pFormatCtx);
                }

                if (audio_stream_idx != -1) { // Only insert if audio stream found
                    std::string insert_sql = "INSERT INTO samples (filepath, filename, size, duration, samplerate, bitdepth, channels, tags) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
                    sqlite3_stmt *stmt;
                    rc = sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &stmt, 0);
                    if (rc != SQLITE_OK) {
                        LOG("SQL error preparing insert: {}", sqlite3_errmsg(db));
                    } else {
                        sqlite3_bind_text(stmt, 1, filepath_str.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 2, filename_str.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_int64(stmt, 3, file_size);
                        sqlite3_bind_double(stmt, 4, duration);
                        sqlite3_bind_int(stmt, 5, sample_rate);
                        sqlite3_bind_int(stmt, 6, bit_depth);
                        sqlite3_bind_int(stmt, 7, channels);
                        sqlite3_bind_text(stmt, 8, "", -1, SQLITE_STATIC); // Empty tags for now

                        rc = sqlite3_step(stmt);
                        if (rc != SQLITE_DONE) {
                            LOG("SQL error inserting data: {}", sqlite3_errmsg(db));
                        } else {
                            LOG("Sample inserted successfully.");
                            load_samples(db); // Refresh data after insertion
                        }
                        sqlite3_finalize(stmt);
                    }
                } else {
                    LOG("No audio stream found in selected file. Not inserting into database.");
                }
            }
          }
          if (ImGui::MenuItem("Exit"))
          {
            running = false;
          }
          ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
      }

      ImGui::Begin("Sound Samples");

      if (ImGui::BeginTable("samples", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
      {
          ImGui::TableSetupColumn("Filepath");
          ImGui::TableSetupColumn("Filename");
          ImGui::TableSetupColumn("Size");
          ImGui::TableSetupColumn("Duration");
          ImGui::TableSetupColumn("Sample Rate");
          ImGui::TableSetupColumn("Bit Depth");
          ImGui::TableSetupColumn("Channels");
          ImGui::TableSetupColumn("Tags");
          ImGui::TableHeadersRow();

          for (const auto& s : samples_data)
          {
              ImGui::TableNextRow();
              ImGui::TableSetColumnIndex(0);
              ImGui::Text("%s", s.filepath.c_str());
              ImGui::TableSetColumnIndex(1);
              ImGui::Text("%s", s.filename.c_str());
              ImGui::TableSetColumnIndex(2);
              ImGui::Text("%lld KB", s.size / 1024);
              ImGui::TableSetColumnIndex(3);
              ImGui::Text("%.2f s", s.duration);
              ImGui::TableSetColumnIndex(4);
              ImGui::Text("%d Hz", s.sample_rate);
              ImGui::TableSetColumnIndex(5);
              ImGui::Text("%d bit", s.bit_depth);
              ImGui::TableSetColumnIndex(6);
              ImGui::Text("%d channels", s.channels);
              ImGui::TableSetColumnIndex(7);
              ImGui::Text("%s", s.tags.c_str());
          }
          ImGui::EndTable();
      }

      ImGui::End();

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      window.glSwap();
    }

    sqlite3_close(db);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
  }
  catch (const std::exception &e)
  {
    LOG("{}", e.what());
    return 1;
  }

  return 0;
}