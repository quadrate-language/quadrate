# Falcon 9 Rocket Simulation

3D orbital mechanics simulation of a Falcon 9 launch to orbit.

## Run

```bash
quad run main.qd
```

## Features

- Earth-Centered Inertial (ECI) coordinate system
- Two-stage rocket with realistic parameters
- Gravity turn trajectory
- Runge-Kutta 4 numerical integration
- Real-time terminal display with progress bars
- Orbital analysis (apoapsis, periapsis, eccentricity)
- Multi-file project structure (physics, integrator, orbital mechanics)
- Stack code throughout: the vehicle and stages in globals, the physics as one-line words
  (`gravity`, `thrust_dir`, `drag_accel`), and guidance clamped with `at_least` and `at_most`
