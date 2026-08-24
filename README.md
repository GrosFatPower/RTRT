# RTRT

Various basic renderers 
=======================


Test3 : GPU Ray tracer
--------

A (poorly lit) ray tracer written in c++ and GLSL.

| ![diningroom_RT](./Captures/RayTracer_diningroom.scene_4255frames.png) | ![BarberShopChair_RT](./Captures/RayTracer_BarberShopChair_01.scene_163frames.png) |
| -------- | ------- |
| ![mustang_red_RT](./Captures/RayTracer_mustang_red.scene_40frames.png) | ![spaceship_RT](./Captures/RayTracer_spaceship.scene_50frames.png) |

Test4 : CPU Rasterizer
--------

Software rasterizer running on the CPU.
Basic phong reflecton model.

| ![CashRegister](./Captures/Rasterizer_CashRegister_01_4k.gltf.png) | ![BarberShopChair](./Captures/Rasterizer_BarberShopChair_01.scene.png) |
| -------- | ------- |
| ![mustang_red](./Captures/Rasterizer_mustang_red.scene.png) | ![rank3police](./Captures/Rasterizer_rank3police.scene.png) |
| ![spaceship_Rast](./Captures/Rasterizer_spaceship.scene.png) | ![Sponza_Rast](./Captures/Rasterizer_Sponza.png) |


Test5 : Render lab (GPU Path tracer / CPU Rasterizer / GPU Deferred renderer)
--------

| Path tracer | Software rasterizer | Deferred renderer |
| -------- | ------- | ------- |
| ![mustang_white_PT](./Captures/PathTracer_ABeautifulGame.png) | ![mustang_white_PT](./Captures/Rasterizer__Phong_ABeautifulGame.png) | ![mustang_white_PT](./Captures/Deferred_ABeautifulGame.png) |

Test6 : (Very) Basic FPS game
--------

![FPSGame_PT](./Captures/Test6_FPSGame_PathTracer.png)

![FPSGame_Rast](./Captures/Test6_FPSGame_Software.png)

![FPSGame_GPU](./Captures/Test6_FPSGame_Deferred.png)


References/Credits
--------
- Path tracer largely inspired from GLSL-PathTracer (https://github.com/knightcrawler25/GLSL-PathTracer)
- Rasterizer also inspired from CPURasterizer (https://github.com/Zielon/CPURasterizer)
- Scratch a pixel courses (https://www.scratchapixel.com/)
