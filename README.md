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
* **`platform/sim`** – PC-Simulator (SDL-Fenster oder headless) mit UDP-Transport. **`platform/stm32`** – Portplan (noch nicht umgesetzt).

Navigation: `Räume` → `Raum (Geräte)` → `Gerät (Detail, z. B. Dimmer)`; Zahnrad → `Einstellungen` (KNX-Status, Gateway, Version).
Die Statusleiste zeigt dauerhaft den KNX-Link (grün verbunden / orange verbindet / rot getrennt); Befehle ohne Verbindung
werden mit Meldung abgewiesen.

Unterstützte Gerätetypen im Mockup: Schalter (DPT 1), Dimmer (DPT 1 + 5.001), Temperatursensor (DPT 9).

## Bauen & Ausprobieren (PC)

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

### Zielcompiler-Check (Cortex-M7)

```sh
cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DHD_BUILD_SIM=OFF -DHD_BUILD_TESTS=OFF
cmake --build build-arm --target hd_core hd_ui hd_config
```

## Nächste Schritte

1. STM32-Port (siehe `platform/stm32/README.md`): LTDC/SDRAM/Touch, lwIP, FreeRTOS-Tasks.
2. Generator ETS-Export (`.knxproj`) → `house_config.c`.
3. Weitere Gerätetypen (Jalousie, Szenen, Fensterkontakte, Heizung) – jeweils DPT in `dpt.c`, Typ in `model.h`, Karte in `ui_page_room.c`.
4. Einstellungen persistent (Gateway-IP), Bildschirmschoner/Helligkeit, ggf. Gateway-Auswahl per Discovery (SEARCH_REQUEST).

Fonts (`ui/fonts/`) sind mit `tools/gen_fonts.sh` erzeugt (Latin-1 für Umlaute + FontAwesome-Icons).
