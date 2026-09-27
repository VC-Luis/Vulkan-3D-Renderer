# Vulkan 3D Graphics Engine
This is my homemade 3D graphics engine built in Vulkan. It is meant to be give a simple, handy and cross-platform way to set up other projects.
## Tools and dependencies
This engine uses the following tools:
- **Vulkan:** A low level graphics API used for interfacing with the GPU and the window
- **GLFW:** A simple API for creating and managing windows and user inputs
- **tinyobjloader:** A small library for loading .obj files
- **stb_image:** A library for processing images
- **CMake:** Used for compiling the project in a modular fashion
# Installation and setup
### Linux
Setting up this project on Linux is as simple as cloning this repository and running the CMake file
```console
git clone https://github.com/VC-Luis/Vulkan-3D-Renderer
cd Vulkan-3D-Renderer
cmake -B build -S .
cd build
make
```
****
# Roadmap
- Abstract the input system away from GLFW
- Simplify the renderer setup process
- Add lighting support
    - Add basic lamertial lighting
    - Implement the Blinn-Phong reflection model
- Add basic physics support (?)