# OpenGL 3.3 Implementation of Jos Stam's Stable Fluids Demo

![Screenshot of stable fluids demo](images/image0.png "Screenshot of stable fluids demo")
Jos Stam's original demo was written in C using GLUT for early versions of OpenGL. To learn real-time fluid simulation techniques I've ported his demo to OpenGL 3.3 and introduced a few modifications, including:
 1. **Periodic (toroidal) boundary conditions:** The fluid wraps around the window's edges both horizontally and vertically.
 2. **Colored visualization of velocity:** Fluid velocity is visualized as a color varying from blue at zero speed to magenta at high speeds. (Fluid density is indicated by the intensity of the color at each cell.)

To run the demo, run `make` and execute `demo`. This has been developed on (and will likely only run on) macOS.

```
make
./demo
```

## Goals
 - Convert boundary conditions from fixed to periodic - ***Done!***
 - Vary visuals based on density and/or velocity - ***Done!***
 - Expand simulation to three dimensions
 - Render in three dimensions

## Libraries

 - [GLFW](https://www.glfw.org)
 - [GLAD](https://github.com/Dav1dde/glad)

## References

- Stam, Jos. (2001). Stable Fluids. ACM SIGGRAPH 99. 1999. 10.1145/311535.311548.
- Stam, Jos. (2001). A Simple Fluid Solver based on the FFT. Journal of Graphics Tools. 6. 10.1080/10867651.2001.10487540.
- Stam, Jos. (2003). Real-Time Fluid Dynamics for Games.