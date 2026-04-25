# ESP32 Pressure Pump Controller

Automatic pressure pump controller using an ESP32 with OTA firmware updates.

When a diaphragm pump fills a pressurized container with a float valve, the pressure switch can start toggling rapidly as the float valve approaches the closed position. This causes the pump to turn on and off very quickly.

This controller prevents rapid cycling by adding timing logic to the pressure switch signal, making pump operation smoother and more predictable.

The ESP32 monitors a pressure switch (pressostat) and controls the pump through a MOSFET driver. The system also includes safety timeout protection and OTA firmware updates.

---

## Features

- Automatic pump control based on pressure switch
- Debounce logic for stable sensor readings
- Safety timeout to prevent pump damage
- Cooldown delay between pump cycles
- OTA firmware updates over WiFi
- Works even if WiFi is unavailable
- Status LED indicating pump state

---

## Hardware

- ESP32 (Tested on Wemos S2 Mini)
- 12V diaphragm pump
- MOSFET driver module
- 12V power supply
- Step-down converter (12V → 5V)

---

## Pump Logic

Pump **starts** when:

- Pressure switch stays **LOW for at least 2 seconds**
- Pump cooldown time has passed

Pump **stops** when:

- Pressure switch stays **HIGH for at least 2 seconds**

---

## Safety Features

### Safety Timeout

If the pump runs longer than **240 seconds**, it is automatically stopped and the controller enters **safety lock mode**.

### Cooldown

The pump cannot restart for **3 seconds** after stopping.

---
