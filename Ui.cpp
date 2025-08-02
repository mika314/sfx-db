#include "Ui.h"
#include "audio_player.h"
#include "imgui-impl-opengl3-loader.h"
#include "imgui-impl-opengl3.h"
#include "imgui-impl-sdl.h"
#include "tinyfiledialogs.h"
#include <filesystem>
#include <fstream>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <log/log.hpp>

#include "miniaudio.h"

Ui::Ui(sdl::Window &window, SDL_GLContext gl_context, Database &db, std::vector<Sample> &samples_data)
  : m_window(window),
    m_gl_context(gl_context),
    m_db(db),
    m_samples_data(samples_data),
    m_running(true),
    m_selected_sample_idx(-1)
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable; // Disable multi-viewports
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(m_window.get(), m_gl_context);
  ImGui_ImplOpenGL3_Init("#version 130");
}

Ui::~Ui()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

bool Ui::processEvent(SDL_Event &event)
{
  ImGui_ImplSDL2_ProcessEvent(&event);
  if (event.type == SDL_QUIT)
  {
    m_running = false;
    return true;
  }
  return false;
}

void Ui::render()
{
  // Clear the screen
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Create a full-window ImGui window to contain all content
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("MainUI", nullptr, window_flags);

  if (ImGui::BeginMainMenuBar())
  {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("Add Sample"))
      {
        char const *lTheOpenFileName =
          tinyfd_openFileDialog("Select a sound file", "", 0, NULL, NULL, 0);
        if (lTheOpenFileName)
        {
          LOG("Selected file: ", lTheOpenFileName);
          extract_metadata_and_insert(lTheOpenFileName);
        }
      }
      if (ImGui::MenuItem("Scan Directory"))
      {
        char const *lTheSelectedDirectory = tinyfd_selectFolderDialog("Select a directory to scan", "");
        if (lTheSelectedDirectory)
        {
          LOG("Selected directory: ", lTheSelectedDirectory);
          m_db.scan_directory(lTheSelectedDirectory);
          m_db.load_samples(m_samples_data, filter);
        }
      }
      if (ImGui::MenuItem("Exit"))
      {
        m_running = false;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  // Your table or main content
  ImGui::Text("Sound Samples");
  if (ImGui::InputText("Filter", &filter, ImGuiInputTextFlags_EnterReturnsTrue))
  {
    ImGui::SetKeyboardFocusHere();
    m_db.load_samples(m_samples_data, filter);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
  {
    if (m_selected_sample_idx > 0)
    {
      m_selected_sample_idx--;
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
  {
    if (m_selected_sample_idx < (int)m_samples_data.size() - 1)
    {
      m_selected_sample_idx++;
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
  {
    if (m_selected_sample_idx > 0)
    {
      m_selected_sample_idx = std::max(0, m_selected_sample_idx - 10);
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
  {
    if (m_selected_sample_idx < (int)m_samples_data.size() - 1)
    {
      m_selected_sample_idx = std::min((int)m_samples_data.size() - 1, m_selected_sample_idx + 10);
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
    }
  }

  if (ImGui::BeginTable("samples", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
  {
    ImGui::TableSetupColumn("Filepath", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Sample Rate", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Bit Depth", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Tags", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < m_samples_data.size(); ++i)
    {
      ImGui::PushID(i); // Push unique ID for each row
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      if (ImGui::Selectable(m_samples_data[i].filepath.c_str(),
                            m_selected_sample_idx == (int)i,
                            ImGuiSelectableFlags_SpanAllColumns))
      {
        m_selected_sample_idx = i;
        m_scroll_to_selected = true;
        m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
      }
      if (m_scroll_to_selected && m_selected_sample_idx == (int)i)
      {
        ImGui::SetScrollHereY();
        m_scroll_to_selected = false;
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", m_samples_data[i].filename.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%lld KB", m_samples_data[i].size / 1024);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%.2f s", m_samples_data[i].duration);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d Hz", m_samples_data[i].sample_rate);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%d bit", m_samples_data[i].bit_depth);
      ImGui::TableSetColumnIndex(6);
      ImGui::Text("%d channels", m_samples_data[i].channels);
      ImGui::TableSetColumnIndex(7);
      ImGui::Text("%s", m_samples_data[i].tags.c_str());
      ImGui::PopID(); // Pop the ID
    }
    ImGui::EndTable();
  }

  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  m_window.glSwap();
}

void Ui::extract_metadata_and_insert(const char *filepath)
{
  auto new_sample = AudioPlayer::extract_meta_data(filepath);
  if (new_sample.filepath.empty())
    return;
  m_db.insert_sample(new_sample);
  m_db.load_samples(m_samples_data, filter);
}
