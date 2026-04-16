# Tank_Level_Measurement

**Tank Level Measurement Toolkit (Embedded C)**

A lightweight collection of Embedded C utilities for tank level measurement, including:

- Volume calculations for common tank geometries
- Filtering algorithms for noisy sensor data
- Practical implementations used in real embedded systems

Designed for microcontrollers, PLC-style systems, and IoT applications.

**Features**

**Tank Geometry Support**

Convert level measurements (mm) into volume (litres):

- Rectangular tanks
- Vertical cylindrical tanks
- Horizontal cylindrical tanks (non-linear)

**Sensor Filtering**

Built-in filters for handling noisy or unstable readings:

Moving Average
Exponential Moving Average (EMA)
Median Filter (spike rejection)
Rate Limiter (enforces physical limits)
Kalman Filter:
1D (simple)
2-state (level + rate)

**Why This Exists**

Real-world tank sensors are messy:

Ultrasonic sensors suffer from foam, vapour, temperature
Radar sensors can produce spikes and reflections
Pressure sensors drift and vary slightly over time

This toolkit focuses on:

Stability
Accuracy
Real-world behaviour (not just theory)

**Typical Pipeline**-
raw_level_mm
    ↓
filtering (median / EMA)
    ↓
stable_level_mm
    ↓
volume calculation
    ↓
litres

**Example Usage**
1. Filter a sensor reading
float filtered = kalman_level_process(&kf, raw_level_mm, dt_sec);
2. Convert level to volume (rectangular tank)
float litres = tank_volume_rectangular_litres(1219.0f, 1219.0f, filtered);
3. Full pipeline
float process_tank(float raw, float dt)
{
    float filtered = kalman_level_process(&kf, raw, dt);
    return tank_volume_rectangular_litres(LENGTH_MM, WIDTH_MM, filtered);
}

**Supported Tank Types**
Tank Type	Formula Type	Notes
Rectangular	Linear	Simple L × W × H
Vertical Cylinder	Linear	πr²h
Horizontal Cylinder	Non-linear	Circular segment

**Applications**
Water storage tanks
Fuel tanks
Chemical processing
Industrial monitoring
IoT level sensing systems
DIY / hobby projects

**Configuration & Tuning**
Kalman Filter (recommended defaults)
Sensor Type	Q_level	Q_rate	R
Ultrasonic	0.05	0.10	100
Radar	0.02	0.05	25
Pressure	0.01	0.02	9
EMA
alpha = 0.1  // very smooth
alpha = 0.3  // balanced
alpha = 0.7  // fast response

**Design Notes**
Uses millimetres (mm) and litres (L) throughout
No dynamic memory allocation
Minimal dependencies (math.h only where required)
Suitable for low-resource embedded systems

**Future Improvements**
Fixed-point (integer-only) implementations
Additional tank geometries (conical, spherical)
Adaptive filtering
Multi-sensor fusion (e.g. radar + pressure)

**Contributing**
Contributions welcome:
Bug fixes
Performance improvements
Additional filters or geometries
Better examples

**Author**
Jim Graham
Poseidon Embedded Software Ltd
Embedded systems, signal processing, and real-world engineering solutions.
