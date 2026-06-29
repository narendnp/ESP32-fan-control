# ESP32 PID Fan Controller

PWM fan controller with DHT22 temperature sensing, PID control, web dashboard, and MQTT telemetry.

## Features

- DHT22 temperature & humidity sensor
- 25 kHz PWM fan control (4-pin fan)
- Tachometer RPM reading with interrupt debounce
- MANUAL mode (potentiometer or dashboard slider)
- AUTO mode with PID or Linear ramp-up
- Web dashboard served from ESP32 (HTTP polling, dark theme, history charts)
- MQTT telemetry (publishes every 2s, subscribes to commands)
- Physical button to toggle mode, LED indicator

## Hardware

| GPIO | Connection |
|------|-----------|
| 14 | DHT22 DATA |
| 25 | LED anode (via resistor) |
| 26 | Fan tachometer |
| 27 | Fan PWM input |
| 32 | Push button (to GND) |
| 33 | Potentiometer wiper |

## Quick Start

```
cp include/secrets.h.example include/secrets.h
# edit secrets.h with your WiFi & MQTT credentials
pio run -t upload
pio device monitor
```

Open the dashboard at `http://<esp-ip>/`.

## API

| Route | Description |
|-------|------------|
| `GET /` | Dashboard HTML |
| `GET /api/status` | JSON status (temp, humidity, PWM, RPM, mode) |
| `POST /api/cmd` | Send command (`mode_auto`, `mode_manual`, `pwm`, `setpoint`, `automode`) |

## Branches

- `v1` — Pot + DHT22 + auto/manual
- `v2` — MQTT added
- `v2.1` — Web dashboard, PID controller
- `v2.2` — History charts (current)
