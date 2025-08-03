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

Ui::Ui(sdl::Window &window,
       SDL_GLContext gl_context,
       Database &db,
       std::vector<Sample> &samples_data,
       const std::string &initial_filter,
       int initial_selected_sample_idx)
  : m_window(window),
    m_gl_context(gl_context),
    m_db(db),
    m_samples_data(samples_data),
    m_running(true),
    m_selected_sample_idx(initial_selected_sample_idx),
    filter(initial_filter)
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable; // Disable multi-viewports
  io.KeyRepeatDelay = 0.15f;                           // Shorter delay before key repeat starts
  io.KeyRepeatRate = 0.02f;                            // Faster key repeat rate
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(m_window.get(), m_gl_context);
  ImGui_ImplOpenGL3_Init("#version 130");
  m_db.load_samples(m_samples_data, filter);
  if (m_selected_sample_idx >= 0 && static_cast<size_t>(m_selected_sample_idx) < m_samples_data.size())
  {
    m_scroll_to_selected = true;
  }
  if (m_selected_sample_idx >= 0 && static_cast<size_t>(m_selected_sample_idx) < m_samples_data.size())
  {
    m_scroll_to_selected = true;
  }
  if (m_selected_sample_idx >= 0 && static_cast<size_t>(m_selected_sample_idx) < m_samples_data.size())
  {
    m_scroll_to_selected = true;
  }
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
    m_db.load_samples(m_samples_data, filter);
  }
  ImGui::SetKeyboardFocusHere(-1); // Keep focus on the input text after pressing Enter

  // Calculate remaining height for the child window
  float footer_height_to_reserve =
    ImGui::GetStyle().ItemSpacing.y; // Adjust as needed for other elements below
  ImVec2 child_size = ImVec2(0, -footer_height_to_reserve);

  ImGui::BeginChild(
    "SampleListChild", child_size, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);

  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
  {
    if (m_selected_sample_idx > 0)
    {
      m_selected_sample_idx--;
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
      ImGui::SetClipboardText(m_samples_data[m_selected_sample_idx].filepath.c_str());
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
  {
    if (m_selected_sample_idx < (int)m_samples_data.size() - 1)
    {
      m_selected_sample_idx++;
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
      ImGui::SetClipboardText(m_samples_data[m_selected_sample_idx].filepath.c_str());
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
  {
    if (m_selected_sample_idx > 0)
    {
      m_selected_sample_idx = std::max(0, m_selected_sample_idx - 10);
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
      ImGui::SetClipboardText(m_samples_data[m_selected_sample_idx].filepath.c_str());
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
  {
    if (m_selected_sample_idx < (int)m_samples_data.size() - 1)
    {
      m_selected_sample_idx = std::min((int)m_samples_data.size() - 1, m_selected_sample_idx + 10);
      m_scroll_to_selected = true;
      m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
      ImGui::SetClipboardText(m_samples_data[m_selected_sample_idx].filepath.c_str());
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

    ImGuiListClipper clipper;
    clipper.Begin(m_samples_data.size());
    while (clipper.Step())
    {
      for (int row_num = clipper.DisplayStart; row_num < clipper.DisplayEnd; row_num++)
      {
        ImGui::PushID(row_num); // Push unique ID for each row
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Selectable(m_samples_data[row_num].filepath.c_str(),
                              m_selected_sample_idx == row_num,
                              ImGuiSelectableFlags_SpanAllColumns))
        {
          m_selected_sample_idx = row_num;
          m_scroll_to_selected = true;
          m_audio_player.play_audio_sample(m_samples_data[m_selected_sample_idx]);
          ImGui::SetClipboardText(m_samples_data[m_selected_sample_idx].filepath.c_str());
        }
        if (m_scroll_to_selected && m_selected_sample_idx == row_num)
        {
          ImGui::SetScrollHereY();
          m_scroll_to_selected = false;
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", m_samples_data[row_num].filename.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%lld KB", m_samples_data[row_num].size / 1024);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2f s", m_samples_data[row_num].duration);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%d Hz", m_samples_data[row_num].sample_rate);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%d bit", m_samples_data[row_num].bit_depth);
        ImGui::TableSetColumnIndex(6);
        ImGui::Text("%d channels", m_samples_data[row_num].channels);
        ImGui::TableSetColumnIndex(7);
        ImGui::Text("%s", m_samples_data[row_num].tags.c_str());
        ImGui::PopID(); // Pop the ID
      }
    }
    clipper.End();
    ImGui::EndTable();
  }
  ImGui::EndChild(); // End of the child window for the sample list

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
