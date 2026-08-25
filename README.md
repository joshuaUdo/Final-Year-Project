# 🍽️ IoT & LAN Based Smart Cafeteria Management System
### Adeleke University — Final Year Project

A smart cafeteria management system that automates student meal verification using RFID technology, an ESP32 microcontroller, and a Supabase cloud backend. Built as a final year project at Adeleke University, Nigeria.

---

## 📌 Project Overview

The system replaces the manual cafeteria process at Adeleke University with an automated RFID-based verification terminal. Students tap their RFID card at the cafeteria point of service and the system verifies their meal eligibility in real time, displays the result on an OLED screen, and logs the collection to a cloud database.

The project was built across two complementary tracks:
- **LAN Network Infrastructure** — designed and simulated in Cisco Packet Tracer, modelling inter-cafeteria connectivity
- **IoT Embedded Terminal** — physical prototype using ESP32 + MFRC522 + OLED, communicating with Supabase over WiFi

---

## ✨ Features

- ✅ RFID card scanning for student identification
- ✅ Real-time meal eligibility verification via Supabase
- ✅ Duplicate meal collection prevention (enforced at database level)
- ✅ Meal window enforcement — Lunch (12pm–2pm) and Dinner (4pm–6pm)
- ✅ OLED display feedback for every scan result
- ✅ Offline mode — saves scan records to ESP32 internal flash (SPIFFS) during network outages
- ✅ Auto-sync of offline records to Supabase on reconnection
- ✅ Smart card enrollment — unregistered cards can be captured directly via the admin dashboard
- ✅ Web-based admin portal for student registration and meal monitoring

---

## 🛠️ Hardware Components

| Component | Purpose |
|-----------|---------|
| ESP32 Dev Board | Main microcontroller — WiFi, processing, control |
| MFRC522 RFID Reader | Reads student RFID card UIDs via SPI |
| MIFARE 1K RFID Cards | Student identification cards |
| SSD1306 OLED Display (0.96") | Visual feedback via I2C |
| 5V USB Power Supply | Powers the system |

---

## 🔌 Wiring / Pin Configuration

### MFRC522 → ESP32 (SPI)
| MFRC522 Pin | ESP32 Pin |
|-------------|-----------|
| SDA (SS) | GPIO 5 |
| SCK | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| RST | GPIO 4 |
| GND | GND |
| 3.3V | 3.3V |

### SSD1306 OLED → ESP32 (I2C)
| OLED Pin | ESP32 Pin |
|----------|-----------|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| GND | GND |
| VCC | 3.3V |

> ⚠️ Both peripherals must be powered at **3.3V** — do NOT connect to 5V.

---

## 🗄️ Database Structure (Supabase)

### Tables

**`students`** — Registered student records
```
id          uuid (primary key)
rfid_uid    text (unique)
full_name   text
last_meal_at timestamptz
```

**`meal_logs`** — Complete meal collection history
```
id           uuid (primary key)
student_id   uuid (foreign key → students)
rfid_uid     text
full_name    text
meal_type    text ('LUNCH' or 'DINNER')
collected_at timestamptz
```
> Has a unique constraint on `(rfid_uid, meal_type, collected_at::DATE)` to prevent duplicate entries.

**`offline_logs`** — Temporary table for records captured during network outages
```
id           uuid (primary key)
rfid_uid     text
meal_type    text
captured_at  timestamptz
synced_at    timestamptz
transferred  boolean (default false)
```

**`enrollment_queue`** — Manages card enrollment sessions between the web interface and ESP32
```
id          uuid (primary key)
status      text ('WAITING', 'SCANNED', 'COMPLETED', 'CANCELLED')
scanned_uid text
created_at  timestamptz
updated_at  timestamptz
```

### RPC Functions
- `verify_meal_access(target_uid)` — Core verification logic, returns `ACCESS_GRANTED`, `ALREADY_EATEN`, `NOT_MEAL_TIME`, or `INVALID_CARD`
- `check_enrollment_waiting()` — Called by ESP32 to check for open enrollment sessions
- `submit_enrollment_uid(session_id, card_uid)` — Called by ESP32 to submit a scanned UID to an enrollment session
- `transfer_offline_record(offline_id, use_time)` — Transfers a reviewed offline log to meal_logs

---

## ⚙️ System Flow

```
Student taps RFID card
        ↓
ESP32 reads card UID via MFRC522 (SPI)
        ↓
WiFi available?
   ├── YES → POST to Supabase RPC: verify_meal_access(uid)
   │              ↓
   │         Supabase checks:
   │           1. Is it meal time? (Lunch/Dinner window)
   │           2. Does the card exist in students table?
   │           3. Has this student already collected today?
   │              ↓
   │         Returns: ACCESS_GRANTED / ALREADY_EATEN /
   │                  NOT_MEAL_TIME / INVALID_CARD
   │
   └── NO  → Save record to SPIFFS (CSV) for later sync
        ↓
OLED displays result
        ↓
On WiFi reconnect → auto-sync offline records to Supabase
```

---

## 💻 Firmware Setup

### Requirements
- [PlatformIO](https://platformio.org/) in VS Code
- ESP32 Arduino framework
- Libraries:
  - `MFRC522`
  - `Adafruit SSD1306`
  - `Adafruit GFX`
  - `WiFi` (built-in)
  - `HTTPClient` (built-in)
  - `SPIFFS` (built-in)

### Configuration
Open `src/main.cpp` and update the following:

```cpp
// WiFi credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Supabase credentials
const char* supabaseUrl = "https://your-project.supabase.co";
const char* supabaseKey = "your-anon-key";
```

### Upload
```bash
# Build and upload via PlatformIO
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

---

## 🌐 Admin Web Dashboard

A single HTML file (`admin/cafeteria_admin.html`) that connects directly to Supabase via the REST API. Open it in any browser — no server required.

### Features
- 📊 Live stats — total students, lunch/dinner collections today, pending offline logs
- ➕ Register students by name and RFID UID
- 📡 **Scan Card via ESP32** — opens an enrollment session, student taps their card on the physical device, UID auto-fills in the form
- 👥 Student table with last meal time and meal type badge
- 🔍 Search by name or UID
- 📋 Offline log review panel — shows pending records with timestamp correction before transfer to meal_logs

---

## 🔄 OLED Display Messages

| Message | Meaning |
|---------|---------|
| ACCESS / Enjoy your meal! | Valid card, meal logged successfully |
| DENIED / Already collected! | Student already collected this meal today |
| DENIED / Outside meal hrs | Scanned outside lunch or dinner window |
| DENIED / Card not found | RFID card not registered in the system |
| ENROLLING / [UID] | Unregistered card detected during enrollment session |
| CARD READ! / Complete on web | UID captured, admin completes registration on web |
| OFFLINE / Scan logged | No WiFi — record saved to internal flash |
| Syncing / Offline logs... | WiFi restored — syncing saved records |
| System Ready / Scan Card... | Idle — waiting for next card tap |

---

## 🧪 Simulation (Wokwi)

The project was developed and validated using the [Wokwi](https://wokwi.com) online ESP32 simulator before physical hardware assembly.

For simulation, update WiFi credentials to:
```cpp
const char* ssid     = "Wokwi-GUEST";
const char* password = "";
// Add channel 6 for faster connection:
WiFi.begin(ssid, password, 6);
```

---

## 📁 Project Structure

```
smart-cafeteria/
├── src/
│   └── main.cpp              # ESP32 firmware
├── admin/
│   └── cafeteria_admin.html  # Web admin dashboard
├── database/
│   ├── supabase_setup.sql    # Tables and RPC functions
│   ├── enrollment_queue.sql  # Enrollment queue table and functions
│   └── offline_transfer.sql  # Offline log transfer function + unique constraint
├── simulation/
│   └── diagram.json          # Wokwi circuit diagram
├── platformio.ini            # PlatformIO project config
└── README.md
```

---

## 🔐 Security Notes

- All ESP32 ↔ Supabase communication uses **HTTPS/TLS**
- Every request requires an **API key** and **Authorization Bearer** header
- Duplicate meal prevention is enforced at the **database level** via a unique constraint
- The `verify_meal_access` function runs entirely server-side — the ESP32 cannot bypass it
- RFID UID cloning is a known limitation — production deployment should consider encrypted cards or PIN confirmation

---

## 👥 Authors

- **Udo Joshua Ohireme** (22/0293) — IoT Embedded System, Cloud Backend, Admin Web Interface
- **Oyeleke Fiyinfoluwa Francis** (22/0292) — LAN Network Design & Simulation

**Supervisor:** Mr. Akintunde A. E  
**Institution:** Adeleke University, Ede, Osun State, Nigeria  
**Year:** 2025

---

## 📄 License

This project was developed as an undergraduate final year project at Adeleke University. Feel free to reference or build upon it with appropriate attribution.
