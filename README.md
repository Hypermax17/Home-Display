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

Die Firmware läuft mit dem Mock-Backend, also ohne Netzwerk, alle Geräte schalten lokal.

1. **Firmware besorgen** – ohne lokale Toolchain: GitHub → Reiter *Actions* → letzter Lauf von *CI* → Artifact **hd_firmware**
   herunterladen und entpacken. Es enthält `hd_firmware.bin` (= komplette Anwendung) sowie die Ausbaustufen
   `hd_firmware_L0.bin` … `hd_firmware_L7.bin` für die Fehlersuche (siehe unten).
2. **Board anschließen:** Micro-USB-Kabel in die Buchse **CN14 „ST-LINK“** (oben, neben dem Ethernet-Port), Rechner/Netzteil dahinter.
   Auf Windows erscheint ein Laufwerk **DIS_F746NG**.
3. **`hd_firmware.bin` auf dieses Laufwerk kopieren.** Eine ST-LINK-LED blinkt während des Flashens, danach startet das Board neu.
4. Erwartet: kurz rot/grün/blaue Balken (Test des Anzeigepfads), dann „Räume“, Bedienung per Touch wie im Browser-Simulator.
   LED1 (grün) blinkt dabei 8× pro 3-Sekunden-Zyklus (Stufe 7).

**Fehlersuche – Ausbaustufen:** Jede Stufe fügt genau einen Schritt zur vorigen hinzu. Von unten nach oben flashen; die erste
Stufe, die nicht mehr wie beschrieben läuft, grenzt den Fehler ein. LED1 blinkt in Stufe *N* genau *N+1*-mal pro 3 s (so
erkennst du, welche Datei auf dem Board läuft). Dauerhaftes schnelles Blinken (5 Hz) = Init-Fehler, 3 sehr schnelle Blinks
mit Pause = CPU-Fault; dabei zeigt das Display auf rotem Grund Register (PC, LR, CFSR, HFSR, BFAR, MMFAR, SP, HP) –
bitte abfotografieren.

| Stufe | neu hinzugekommen | erwartet |
|---|---|---|
| 0 | HAL_Init, LED | LED 1 Puls / 3 s, Display weiß |
| 1 | Takt 216 MHz | LED 2 Pulse |
| 2 | I-/D-Cache | LED 3 Pulse |
| 3 | MPU für SDRAM | LED 4 Pulse |
| 4 | LCD- und SDRAM-Init | LED 5 Pulse, Display weiß |
| 5 | Layer, Testbild, Display an | LED 6 Pulse, Farbbalken |
| 6 | Touch-Init | LED 7 Pulse, Farbbalken |
| 7 | LVGL, UI, Datenschicht | LED 8 Pulse, Anwendung |
| 7 (`_sram1`) | wie 7, RAM erst ab 0x20010000 (statt DTCM) | wie 7 |

Das Flashen habe ich nur für den Windows-PC beschrieben; ob das iPad das ST-LINK-Laufwerk beschreiben kann, ist ungetestet.

Selbst bauen (Linux/WSL, `gcc-arm-none-eabi` und `cmake` installiert; HAL/BSP werden automatisch geladen):

```sh
cmake -S . -B build-stm32 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DHD_BUILD_SIM=OFF -DHD_BUILD_TESTS=OFF -DHD_BUILD_STM32=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-stm32 --target hd_firmware     # -> build-stm32/hd_firmware.bin
```

Größe: ca. 453 KB Flash (43 %), 200 KB RAM (61 %).

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
