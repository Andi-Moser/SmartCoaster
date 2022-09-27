# SmartCoaster

## Das Projekt

SmartCoaster wurde als Projekt im Rahmen des Faches "Embedded Systems" an der Teko Luzern durchgeführt.

Aufgabenstellung des Projektes war es, mit einem ESP32 die Daten von mindestens einem Sensor auszuwerten und
über ein Output Device darzustellen.

### Zielsetzung

(...) Folgt

## Die Hardware

### Beetle ESP32

### HX711 Load Cell Amplifier

### 5kg Load Cell Gewichtssensor

### WS2812B NeoPixel LED Strip

## Aufbau der Hardware

### LEDs

- Roter Pin: 3.3V
- Schwarzer Pin: GND
- Grüner Pin: via 330 Ohm Widerstand zum ESP Pin 2

### HX711

#### Seite ESP32

- GND - GND
- DT - ESP Pin 16
- SCK - ESP Pin 17

#### Seite Load Cell

- E+ - Rotes Kabel
- E- - Schwarzes Kabel
- A+ - Weisses Kabel
- A- - Grünes oder gelbes Kabel

## Aufbau der Software

Die Software ist als simple StateMachine implementiert.

### State Diagram

(...) Folgt

### Stati

(...) Folgt

### Transitions

(...) Folgt