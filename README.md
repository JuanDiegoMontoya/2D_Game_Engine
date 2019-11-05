# 3D_Voxel_Engine
Yet another voxel engine. Gallery at the bottom.

This is (currently) a tech demo designed to demonstrate the implementation of various rendering and procedural techniques in a large and dynamic environment.

## Tech
The project is written in C++ and GLSL. The project also uses OpenGL, GLM, GLFW for window handling, cereal for serialization, and libnoise for procedural noise.  
The project may, in the future, utilize FMOD for audio, freetype for text rendering, Lua for scripting, and stb for loading textures.

Performance was tested on this system for reference:
- AMD Ryzen 5 2600X CPU
- 16 GB RAM
- NVIDIA GeForce GTX 1060 6GB

## Features
(*WIP*) indicates that a feature is mostly complete but has some bugs that need to be sorted out. 
### Voxels
- Capable of rendering 10+ million blocks at 500+ FPS (when not generating chunks)
- Infinite size world
- Movement physics and terrain collision
- Procedurally generated terrain including hills, mountains, plains, rivers, etc.
- A variety of biomes, each with their own unique properties and features
- Meandering natural tunnels and caves
- Block destroying and placing
- A prefab editor
- Marching cubes implementation for smooth voxels (in worlds defined by a density field, enabled via preprocessor flag in chunk.h)

### Graphics
- Phong illumination model
- Frustum culling of chunks
- Baked ambient occlusion on blocks
- Realistic water effects
- Directional environment lighting
- Deferred rendering pipeline
- Post processing effects
- (*WIP*) Ray traced screen space water reflections
- (*WIP*) Cascaded shadow maps

### Other
- Portability. Uses (to my knowledge) no platform dependent libraries or headers. (Certain libraries would have to be rebuilt for platforms other than x64 Windows)
- Multithreaded mesh building and (*WIP*) terrain generation
- Graphics effects can be toggled dynamically by the user


## In-Engine
### Controls
- WASD for camera movement, mouse for looking
- \` (Grave accent, left of '1' key) will toggle the mouse cursor so the user can interact with screen elements
- Mouse 1 (LMB) will remove the currently highlighted block on the screen
- Mouse 2 (RMB) will place a block of the type currently shown rotating on the bottom half of the screen
- Scrolling up or down will change the active block to place
- Left Shift will increase camera speed by 10 times
- Left Control will slow camera speed to 1/10th

#### Prefab editing
- Tab will toggle the prefab editor menu
  - The most recently highlighted block will be highlighted in purple instead of white
  - Pressing 'F' will select the block and begin a region
    - Once three blocks have been selected, the region will be completed and will be encompassed by purple wireframe
  - Pressing the "save" button in the prefab menu will save the current region under the name written in the adjacent text box
  - Pressing the "load" button will generate a prefab of the given name in the adjacent box at the most recent highlighted purple position
  - Toggling the prefab editor (Tab x2) will reset the current region if a mistake has been made

## Gallery
Hover to see detail:

![Image of distant terrain.](https://github.com/JuanDiegoMontoya/3D_Voxel_Engine/blob/master/Images/distance03.png "Distant terrain showcasing fog, reflections, and biomes.")
![Image of distant and near terrain.](https://github.com/JuanDiegoMontoya/3D_Voxel_Engine/blob/master/Images/distance02.png "Distant and near terrain showcasing shading and shadows.")
![Image of snowy cave.](https://github.com/JuanDiegoMontoya/3D_Voxel_Engine/blob/master/Images/snow_cave.png "Snow cave.")
![Image showing marched cubes example.](https://github.com/JuanDiegoMontoya/3D_Voxel_Engine/blob/master/Images/marched01.png "Marching cubes implementation with scalar field.")
