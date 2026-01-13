# VitaTPBPlayer  
![Status](https://img.shields.io/badge/Status-In%20Development-yellow) ![Platform](https://img.shields.io/badge/Platform-PS%20Vita-blue) ![Language](https://img.shields.io/badge/Language-C-green)

![Project Preview](https://i.postimg.cc/qq8sNQrG/Vita-TPBPlayer.png)

> **Disclaimer:** This is a **homebrew** PlayStation Vita project created for educational purposes only.  
> It is **not affiliated** with The Pirate Bay or Real-Debrid. Usage is entirely the user's responsibility.

---

## About  
**VitaTPBPlayer** is a PlayStation Vita homebrew application that allows users to **search for torrents directly on the device**, send magnet links to **Real-Debrid** to generate direct download links, and **download content locally** for playback on the Vita.

The main goal of this project is to provide an **all-in-one torrent workflow** on the PS Vita:  
**Search → Resolve → Download → Play**, without relying on external devices.

---

##  Tech Stack  
- **Language:** C  
- **SDK:** VitaSDK  
- **Networking:** `libcurl`  
- **UI:** Native PS Vita framebuffer rendering  
- **APIs & Services:**  
  - The Pirate Bay (torrent search)  
  - Real-Debrid (magnet link resolving)

---

##  Features  

###  Torrent Search  
- Search torrents using The Pirate Bay  
- Displays seeders and leechers  
- On-screen keyboard for text input  
- Pagination support for results  

###  Real-Debrid Integration  
- Sends magnet links directly to Real-Debrid  
- Waits for torrent processing  
- Retrieves direct download URLs  

###  Download & Playback  
- Downloads files to `ux0:data/`  
- Ability to launch external video players after download  

---

##  Project Structure (Simplified)  
```text
VitaTPBPlayer/
├── src/                # C source code
├── resources/          # Assets and resources
├── sce_sys/            # PS Vita Live Area Design
├── Makefile            # VitaSDK build script
└── README.md           # Project documentation
```

---

##  Build & Installation

This project must be compiled using **VitaSDK** and installed on a **hacked PS Vita** (HENkaku / h-encore).

### Requirements

* PlayStation Vita with [HENkaku](https://henkaku.xyz/) or [h-encore](https://github.com/TheOfficialFloW/h-encore)
* [VitaSDK](https://vitasdk.org/) properly installed
* Active [Real-Debrid](https://real-debrid.com/) account and API token

### Build Steps

```bash
git clone https://github.com/mzzvxm/VitaTPBPlayer.git
cd VitaTPBPlayer
make
```

After building, a `.vpk` file will be generated.
Transfer it to your PS Vita and install it using **[VitaShell](https://github.com/TheOfficialFloW/VitaShell)**.

---

##  Legal Notice

* This project is for **educational purposes only**
* Downloading copyrighted material may be illegal in your country
* The developers take **no responsibility** for how this software is used
* Not affiliated with The Pirate Bay or Real-Debrid

---

##  Author

Developed by **mzzvxm**

Contributions, issues, and pull requests are welcome!
