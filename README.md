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

## Requirements

- ESP32 development board (I used a WROOM-32D)
- DHT22 temperature & humidity sensor
- 4-pin PWM fan
- Push button
- LED (with 220Ω resistor)
- Potentiometer (10 kΩ)
- Some jumper wires and a breadboard for prototyping

[!NOTE] Depending on your setup, you might also need a Step-up/Down converter to power your fan if it requires a different voltage than the ESP32. On this project, I used an off-the-shelf 12V PWM PC fan and powered it with a standard 5V USB charger through an MT3608 step-down converter. The ESP32 was powered via USB from the same charger.

## Wiring

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
# edit secrets.h with your WiFi & MQTT credentials
cp include/secrets.h.example include/secrets.h

# build and upload firmware
pio run -t upload

# monitor serial output
pio device monitor
```

Open the dashboard at `http://<esp-ip>/` (*Usually it's 192.168.1.32*).