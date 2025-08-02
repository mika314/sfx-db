# Sound Sample Categorizer - Requirements

## 1. Overview

A C++ desktop application for organizing and categorizing sound samples. The application will provide a graphical user interface for managing a library of sound files.

## 2. Core Technologies

*   **Language:** C++
*   **GUI:** ImGUI
*   **Backend:** SDL2
*   **Graphics:** OpenGL
*   **Audio Decoding:** ffmpeg
*   **Database:** SQLite

## 3. Functional Requirements

### 3.1. Library Management

*   [ ] Users should be able to add sound samples to the library.
*   [ ] Users should be able to remove sound samples from the library.
*   [ ] The application should be able to scan a directory and its subdirectories for sound files to add to the library.
*   [ ] The application should store metadata for each sound sample in a SQLite database.

### 3.2. Metadata

For each sound sample, the following metadata should be stored:

*   [ ] Filepath
*   [ ] Filename
*   [ ] File size
*   [ ] Duration
*   [ ] Sample rate
*   [ ] Bit depth
*   [ ] Number of channels (mono, stereo, etc.)
*   [ ] User-defined tags/categories.

### 3.3. User Interface

*   [ ] The main window should display a list of all sound samples in the library.
*   [ ] The list should be searchable and sortable by different metadata fields.
*   [ ] Users should be able to select a sound sample to view its detailed metadata.
*   [ ] Users should be able to play back the selected sound sample.
*   [ ] Users should be able to add, edit, and remove tags for each sound sample.

### 3.4. Audio Playback

*   [ ] The application should be able to play back the decoded audio from the sound samples.
*   [ ] The UI should have basic playback controls (play, pause, stop, seek).

## 4. Non-Functional Requirements

*   **Platform:** The application should be cross-platform (Windows, macOS, Linux).
*   **Performance:** The application should be responsive, even with a large library of sound samples.
*   **Dependencies:** The application should have a clear way to manage its dependencies (SDL2, ImGUI, ffmpeg, SQLite).
