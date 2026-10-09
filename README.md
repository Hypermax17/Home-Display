# Home Display – KNX-Visualisierung für STM32F746G-DISCO

Touch-Visualisierung (480×272) für ein KNX-Haus. Stand: **Mockup** – UI-Design, Menüführung und ein
Proof of Concept (eine Lampe über KNX schalten). Die Anbindung läuft über einen Server/Gateway im Netzwerk
(KNXnet/IP-Tunneling, z. B. KNX-IP-Interface oder `knxd`).

## Architektur

```
┌──────────────── UI-Thread/-Task ────────────────┐        ┌──────── Daten-Thread/-Task ────────┐
│ ui/        LVGL-Seiten, Navigation              │        │ core/data_service  Zustand, DPT    │
│            kennt nur model.h + app_bus.h        │        │ core/knx_backend   Interface       │
│                                                 │ cmd →  │    ├ knx_ip   KNXnet/IP-Tunneling  │
│   ui_cmd_set_switch / set_level  ───────────────┼────────┼──► └ knx_mock Loopback ohne Netz   │
│   Events → ui_state (Spiegel) → Seiten ◄────────┼─ evt ──┼─── (Zustandsänderungen, Link)      │
└─────────────────────────────────────────────────┘        └──────────────┬─────────────────────┘
        app_bus: zwei lock-freie SPSC-Queues (C11)                         │ knx_transport_t
        – einzige Verbindung zwischen UI und Daten –                       ▼
                                                              UDP-Socket (PC) / lwIP (STM32)
```

* **`core/`** – portables C11, keine HAL-, LVGL- oder Socket-Abhängigkeit. Modell (`model.h`), Queues (`app_bus`),
  DPT-Kodierung (1, 5.001, 9), KNXnet/IP-Client (`knx_ip`: Connect, Tunnelling mit Ack/Retry, Heartbeat, Reconnect),
  Datenservice. Host-getestet.
* **`ui/`** – LVGL 9.2. Kennt kein KNX. Zustand kommt ausschließlich per Event, Bedienung geht ausschließlich als Kommando raus.
  Die UI zeigt immer den vom Bus bestätigten Zustand (Geräte mit Rückmeldeadresse), kein optimistisches Raten.
* **`config/house_config.c`** – Räume, Geräte, Gruppenadressen. Später aus dem ETS-Export generiert (Schnittstelle `house_config()` bleibt).
* **`platform/sim`** – PC-Simulator (SDL-Fenster, headless oder WebAssembly im Browser) mit UDP-Transport (nativ). **`platform/stm32`** – Firmware für das Board (Mock-Backend; Ethernet/KNX folgt).

Navigation: `Räume` → `Raum (Geräte)` → `Gerät (Detail, z. B. Dimmer)`; Zahnrad → `Einstellungen` (KNX-Status, Gateway, Version).
Die Statusleiste zeigt dauerhaft den KNX-Link (grün verbunden / orange verbindet / rot getrennt); Befehle ohne Verbindung
werden mit Meldung abgewiesen.

Unterstützte Gerätetypen im Mockup: Schalter (DPT 1), Dimmer (DPT 1 + 5.001), Temperatursensor (DPT 9).

## Ausprobieren ohne Linux (Windows / iPad / Mac)

Die UI läuft als **Browser-Simulator** (WebAssembly, eine einzelne HTML-Datei, kein Install, Touch-Bedienung):

* **Fertig gebaut:** `docs/index.html` im Browser öffnen (Windows: Datei herunterladen und doppelklicken; funktioniert auch offline).
  Hinweis: GitHub zeigt HTML-Dateien nur als Text an – zum Ansehen herunterladen oder per GitHub Pages
  (Settings → Pages → Branch/`docs`) hosten; so ist sie auch auf dem iPad per URL erreichbar.
* Der Browser hat kein UDP, daher läuft dort das **Mock-Backend**: Alle Geräte sind lokal schaltbar, keine echte KNX-Verbindung.
  Den KNX-Pfad (Gateway, Telegramme) decken die Unit-Tests ab; mit echter Hardware testest du ihn über die native Variante
  (Linux/WSL, siehe unten) oder später direkt auf dem STM32.
* Selbst bauen (Emscripten, läuft auch unter WSL/macOS):
  ```sh
  emcmake cmake -S . -B build-web -DHD_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=MinSizeRel
  cmake --build build-web --target hd_web      # -> build-web/index.html
  ```

## Auf dem Board ausführen (STM32F746G-DISCO, Mock)

Die Firmware läuft mit dem Mock-Backend, also ohne Netzwerk; alle Geräte schalten lokal.

1. **Firmware besorgen** – ohne lokale Toolchain: GitHub → Reiter *Actions* → letzter Lauf von *CI* → Artifact **hd_firmware**
   herunterladen und entpacken (`hd_firmware.hex`/`.bin`; `.elf` und `.map` zum Nachschlagen von Fehleradressen).
2. **Board anschließen:** Micro-USB-Kabel in die Buchse **CN14 „ST-LINK“** (oben, neben dem Ethernet-Port), Rechner/Netzteil dahinter.
3. **Mit STM32CubeProgrammer flashen** (kostenlos, ST-Konto zum Download nötig): *ST-LINK* wählen → *Connect* → Reiter
   *Erasing & Programming* → `hd_firmware.hex` wählen → *Verify programming* und *Run after programming* anhaken → *Start Programming*.
4. Erwartet: Oberfläche „Räume“, Bedienung per Touch wie im Browser-Simulator. LED1 (grün) blinkt 1× pro Sekunde.

**Nicht per Drag-and-Drop auf das Laufwerk `DIS_F746NG` flashen.** Auf diesem Board schreibt das ST-LINK-Laufwerk von großen Dateien nur die
ersten ~32–64 KB in den Flash (nachgewiesen mit einem Testprogramm, das die Flash-Sektoren ausliest). Die Anwendung ist ~455 KB groß; sie stürzt dann
vor `main()` ab, weil der Startcode `.data`/`.init_array` am Ende des Programms liest. Das Display bleibt weiß, LED1 blinkt 3× kurz, lang, 1× kurz
(Fehlersuche-Stand mit Stufenanzeige). Kleine Testprogramme unter 32 KB lassen sich so aber flashen.

**Fehleranzeige:** Bei einem CPU-Fault (Hard-/Bus-/Usage-/MemManage-Fault) zeigt das Display – sofern es schon läuft – auf rotem Grund
`PC`, `LR`, `CFSR`, `HFSR`, `BFAR`, `MMFAR`, `SP`, `HP`; LED1 blinkt 3× kurz mit Pause. Mit `PC`/`LR` und der `.elf` aus dem Artifact lässt sich die
Stelle nachschlagen: `arm-none-eabi-addr2line -f -e hd_firmware.elf 0x<PC>`. Dauerhaftes schnelles Blinken (5 Hz) heißt: SDRAM, LCD oder Touch
ließ sich nicht initialisieren.

Selbst bauen (Linux/WSL, `gcc-arm-none-eabi` und `cmake` installiert; HAL/BSP werden automatisch aus STM32CubeF7 geladen):

```sh
cmake -S . -B build-stm32 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DHD_BUILD_SIM=OFF -DHD_BUILD_TESTS=OFF -DHD_BUILD_STM32=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-stm32 --target hd_firmware     # -> build-stm32/hd_firmware.{hex,bin}
```

Größe: ca. 455 KB Flash (43 %), 200 KB RAM (61 %). Das Flashen habe ich nur für den Windows-PC beschrieben.

## Bauen & Ausprobieren (nativ, Linux / WSL)

```sh
sudo apt install cmake gcc libsdl2-dev        # LVGL wird per CMake geladen (Internet nötig)
cmake -S . -B build && cmake --build build -j
ctest --test-dir build                         # Unit-Tests (DPT, cEMI, KNXnet/IP-Session, Datenservice, Queues)

./build/hd_sim                                 # Fenster, Mock-Backend: alles lokal bedienbar
./build/hd_sim --gateway 192.168.1.10          # echtes KNXnet/IP-Gateway (Port 3671)
```

### Proof of Concept: Lampe schalten

1. In `config/house_config.c` `POC_LAMP_SWITCH_GA` (und ggf. `POC_LAMP_STATUS_GA`) auf deine Lampe setzen. Alle anderen Geräte sind lokale Demo.
2. `./build/hd_sim --gateway <ip-des-knx-interfaces>` → *Räume → Wohnzimmer → Deckenlicht* antippen.
   Schaltet jemand die Lampe am Taster, folgt die Anzeige über die Rückmeldeadresse.

Ohne Hardware: `python3 tools/fake_knx_gateway.py --toggle-after 10` startet einen virtuellen Gateway mit Lampen-Aktor
(simuliert auch einen Wandtaster), dann `./build/hd_sim --gateway 127.0.0.1`.

### Automatisierte Abläufe / Screenshots (headless)

```sh
./build/hd_sim --headless --script tests/scripts/tour.txt --shots out/    # schreibt PPM-Screenshots
```
Skriptbefehle: `wait`, `tap x y`, `drag x1 y1 x2 y2`, `shot name.ppm`, `quit` (siehe `tests/scripts/`).

### Zielcompiler-Check (nur Kern + UI)

```sh
cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DHD_BUILD_SIM=OFF -DHD_BUILD_TESTS=OFF
cmake --build build-arm --target hd_core hd_ui hd_config
```

## Nächste Schritte

1. KNX auf dem Board: ETH + lwIP, UDP-Transport, FreeRTOS-Tasks (siehe `platform/stm32/README.md`).
2. Generator ETS-Export (`.knxproj`) → `house_config.c`.
3. Weitere Gerätetypen (Jalousie, Szenen, Fensterkontakte, Heizung) – jeweils DPT in `dpt.c`, Typ in `model.h`, Karte in `ui_page_room.c`.
4. Einstellungen persistent (Gateway-IP), Bildschirmschoner/Helligkeit, ggf. Gateway-Auswahl per Discovery (SEARCH_REQUEST).

Fonts (`ui/fonts/`) sind mit `tools/gen_fonts.sh` erzeugt (Latin-1 für Umlaute + FontAwesome-Icons).
