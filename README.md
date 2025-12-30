# VitaTPBPlayer

![Status](https://img.shields.io/badge/status-in%20development-yellow)
![Platform](https://img.shields.io/badge/platform-PS%20Vita-blue)
![Language](https://img.shields.io/badge/language-C-orange)

A functional torrent client for the PlayStation Vita that uses The Pirate Bay and Real-Debrid APIs to search for and prepare files for download.

**Note:** This project is currently in a transitional phase. The graphical interface has been modernized, but background download implementation is still in progress.

### Overview

VitaTPBPlayer allows you to search for torrents directly on your PS Vita, use Real-Debrid’s power to get a direct download link, and then download the file to your console.

The project was recently refactored to use the `libvita2d` graphics library, resulting in a much more stable and visually clean interface compared to earlier versions.

## What does it do?

The idea behind VitaTPBPlayer is to create a simple flow for watching content on the Vita:

1. **Search**: The user types a query using an on-screen virtual keyboard.
2. **Results List**: The app searches torrents on The Pirate Bay and displays a list of results with name, seeders, and leechers.
3. **Selection**: The user selects an item from the list.
4. **"Debriding"**: The torrent’s magnet link is sent to the Real-Debrid API.
5. **Download**: The app waits for Real-Debrid to prepare the file, then downloads the direct link to the memory card (`ux0:data/movie.mp4`).
6. **Playback**: After the download finishes, it attempts to launch an external video player to play the downloaded file.

## Current (and Planned) Features

* [x] Search via The Pirate Bay API.
* [x] Integration with Real-Debrid API (add magnet, check status, get link).
* [x] File download via `libcurl`.
* [x] Simple text-based interface using a custom `debugScreen` library.
* [x] On-screen virtual keyboard for text input.
* [x] Pagination for search results.
* [x] Save settings (such as the Real-Debrid token) in a file.
* [ ] Improve the user interface (UI).
* [ ] Manage multiple downloads/files.
* [ ] Integrated video player (a long-term goal).

## How It Works

The application is written in C and compiled using **VitaSDK**.
All networking logic is fully based on `libcurl` to perform HTTP requests to:

* **`apibay.org`**: For anonymous torrent searches.
* **`api.real-debrid.com`**: For all torrent-to-direct-link operations.

The interface is rendered directly to the PS Vita framebuffer, providing a lightweight experience without relying on complex UI libraries.

## Build Requirements

* A PlayStation Vita with [HENkaku](https://henkaku.xyz/) or h-encore.
* [VitaSDK](https://vitasdk.org/) properly installed and configured in your development environment.
* A [Real-Debrid](https://real-debrid.com) account with an active subscription.

## How to Build

1. Clone this repository:

   ```bash
   git clone https://github.com/mzzvxm/VitaTPBPlayer.git
   cd VitaTPBPlayer
   ```

2. Build the project:

   ```bash
   make
   ```

3. This will generate a `VitaTPBPlayer.vpk` file in the project root. Transfer this file to your PS Vita and install it using VitaShell.

## Legal Disclaimer

* This project is provided for educational purposes only.
* The user is fully responsible for any content accessed through this application.
* This project is not affiliated with The Pirate Bay, Real-Debrid, or any other mentioned entity.
* Always respect your country’s copyright laws.
