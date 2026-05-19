# CLAUDE.md — claude-desktop-buddy (M5StickC Plus 2 port)

## Contesto

Fork di `anthropics/claude-desktop-buddy`.
Obiettivo: porting minimo da M5StickC Plus a M5StickC Plus 2.
**Nessun refactor. Nessuna feature aggiuntiva salvo esplicita richiesta.**

---

## Hardware target

| Parametro | Valore |
|---|---|
| Board | M5StickC Plus 2 |
| MCU | ESP32-PICO-V3-02 rev 3.1 |
| Flash | 8MB |
| Display | ST7789V2, 135×240 (identico a Plus) |
| Power IC | AXP2101 (era AXP192) |
| LED pin | GPIO 19 (era GPIO 10 — CRITICO) |
| Porta seriale | `/dev/cu.usbserial-XXXXXXXX` |
| OS sviluppo | macOS |

---

## Stack

- PlatformIO + Arduino framework
- Libreria board: `m5stack/M5StickCPlus2` (era `M5StickCPlus`)
- `bblanchon/ArduinoJson @ ^7.0.0` — invariato
- `bitbank2/AnimatedGIF @ ^2.1.1` — invariato
- BLE: Nordic UART Service, stack ESP32 nativo — invariato

---

## File da modificare

### Modifiche meccaniche (header swap only)

Include da cambiare in tutti questi file:
```
#include <M5StickCPlus.h>  →  #include <M5StickCPlus2.h>
```

File coinvolti:
- `src/main.cpp`
- `src/xfer.h`
- `src/character.cpp`
- `src/buddy.cpp`
- `src/buddies/axolotl.cpp`
- `src/buddies/blob.cpp`
- `src/buddies/cactus.cpp`
- `src/buddies/capybara.cpp`
- `src/buddies/cat.cpp`
- `src/buddies/chonk.cpp`
- `src/buddies/dragon.cpp`
- `src/buddies/duck.cpp`
- `src/buddies/ghost.cpp`
- `src/buddies/goose.cpp`
- `src/buddies/mushroom.cpp`
- `src/buddies/octopus.cpp`
- `src/buddies/owl.cpp`
- `src/buddies/penguin.cpp`
- `src/buddies/rabbit.cpp`
- `src/buddies/robot.cpp`
- `src/buddies/snail.cpp`
- `src/buddies/turtle.cpp`

### Modifiche sostanziali

#### `platformio.ini`
| Riga | Da | A | Rischio |
|---|---|---|---|
| env name | `m5stickc-plus` | `m5stickc-plus2` | Low |
| `board` | `m5stick-c` | `m5stick-c-plus2` (verificare con `pio boards`) | **High** |
| `board_build.partitions` | `no_ota.csv` | `default_8MB.csv` (opzionale) | Low |
| lib_deps | `M5StickCPlus` | `M5StickCPlus2` | **High** |

#### `src/main.cpp`
| Riga | Da | A | Rischio |
|---|---|---|---|
| 26 | `LED_PIN = 10` | `LED_PIN = 19` | **High** |
| 97 | `M5.Axp.ScreenBreath(x)` | `M5.Display.setBrightness(x)` | Medium |
| 102 | `M5.Axp.SetLDO2(true)` | Rimuovere | Medium |
| 112 | `M5.Beep.tone(f, d)` | `M5.Speaker.tone(f, d)` | Low |
| 308 | `M5.Axp.PowerOff()` | `M5.Power.powerOff()` | Low |
| 352–353 | `RTC_TimeTypeDef` / `RTC_DateTypeDef` | Verificare nomi struct in lib Plus 2 | **High** |
| 359 | `M5.Axp.GetVBusVoltage() > 4.0f` | `M5.Power.isCharging()` | Medium |
| 596–618 | Blocco DEVICE con AXP192 raw values | Refactor con `getBatteryLevel()` / `isCharging()` | **High** |
| 630 | `M5.Axp.GetTempInAXP192()` | `temperatureRead()` | Low |
| 685–686 | Stringhe credits | `"M5StickC Plus 2"` / `"ESP32 + AXP2101"` | Low |
| 942 | `M5.Beep.begin()` | Rimuovere | Low |
| 990 | `M5.Beep.update()` | Rimuovere | Low |
| 1056 | `M5.Axp.GetBtnPress() == 0x02` | `M5.BtnPWR.wasPressed()` | Medium |
| 1060 | `M5.Axp.SetLDO2(false)` | `M5.Display.setBrightness(0)` | Medium |
| 1246 | `M5.Axp.ScreenBreath(8)` | `M5.Display.setBrightness(8)` | Low |
| 1260 | `M5.Axp.SetLDO2(false)` | `M5.Display.setBrightness(0)` | Medium |

#### `src/xfer.h`
| Riga | Da | A | Rischio |
|---|---|---|---|
| 115 | `M5.Axp.GetBatVoltage() * 1000` | `M5.Power.getBatteryLevel()` (rinomina var) | Medium |
| 116 | `M5.Axp.GetBatCurrent()` | `0` (stub — AXP2101 non espone mA) | Medium |
| 117 | `M5.Axp.GetVBusVoltage()` | `M5.Power.isCharging()` | Low |
| 118 | Formula manuale `pct` | Rimuovere | Low |
| 124 | JSON field `mV` | Lasciare a `0` o rimuovere il campo | Medium |

> **Nota desktop app:** il campo `bat.mA` nel JSON status diventa sempre `0`. Verificare che il bridge macOS lo tolleri.

#### `src/data.h`
| Riga | Da | A | Rischio |
|---|---|---|---|
| 81 | `RTC_TimeTypeDef tm` | Aggiornare tipo (verificare lib) | **High** |
| 82 | `RTC_DateTypeDef dt` | Aggiornare tipo (verificare lib) | **High** |
| 84–85 | `M5.Rtc.SetTime/SetDate` | Stessi nomi (BM8563 identico) — solo struct | Medium |

> Include di `<M5StickCPlus.h>` arriva via chain da `xfer.h` — non serve modifica diretta.

---

## File NON da modificare

- `src/ble_bridge.cpp` / `.h` — BLE stack ESP32 nativo, invariato
- `src/stats.h` — solo `Preferences.h` e `LittleFS.h`
- `src/buddy_common.h` — costanti geometriche, display 135×240 identico
- `src/character.h` / `src/buddy.h` — solo dichiarazioni
- `tools/*.py` — utilities host-side

---

## Ordine di esecuzione consigliato

1. Verifica board ID: `pio boards | grep -i m5stick`
2. Modifica `platformio.ini`
3. `pio pkg install` → ispeziona `.pio/libdeps/m5stickc-plus2/M5StickCPlus2/src/` per RTC struct names
4. Header swap meccanico (22 file)
5. `src/main.cpp` — modifiche sostanziali
6. `src/xfer.h`
7. `src/data.h`
8. `pio run` — deve compilare zero errori
9. Flash: `pio run --target upload --upload-port /dev/cu.usbserial-XXXXXXXX`

---

## Checklist verifica post-flash

- [ ] LED NON si accende al boot (GPIO 19, active-low)
- [ ] BtnA cicla tra le modalità display senza freeze
- [ ] Power button breve: display off/on
- [ ] BtnA long: menu si apre
- [ ] Device face-down: display dim dopo ~0.5s
- [ ] INFO → DEVICE page: mostra battery % senza crash
- [ ] BLE pairing con Claude desktop bridge: funziona
- [ ] `pio run --target uploadfs`: GIF anima correttamente

---

## Note critiche

> ⚠️ **GPIO 10 su ESP32-PICO-V3-02 è collegato internamente alla SPI flash.**
> Usarlo come output può corrompere la flash o causare boot failure.
> Il LED è su **GPIO 19**. Questa modifica è bloccante — va fatta prima di qualsiasi flash.

> ⚠️ **RTC struct rename**: verificare sempre i nomi dei campi nella libreria installata prima di editare `main.cpp:352` e `data.h:81–82`. Il mismatch silenzioso produce orario garbage.

> ℹ️ **AXP2101 non espone voltage/current raw**: `bat.mV` e `bat.mA` nel JSON BLE diventano sempre `0`. Se il bridge desktop li usa per logica (non solo display), va aggiornato anche lato macOS.
