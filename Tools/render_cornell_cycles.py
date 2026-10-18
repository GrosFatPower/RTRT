import bpy
import math
import os
import sys
import numpy as np
from mathutils import Matrix, Vector

def render_cornell():
    # 1. Reset Blender scene
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    # 2. Configure Cycles
    scene.render.engine = 'CYCLES'
    
    # Try GPU if available, fallback to CPU
    preferences = bpy.context.preferences.addons['cycles'].preferences
    cuda_devices = preferences.get_devices_for_type('CUDA')
    optix_devices = preferences.get_devices_for_type('OPTIX')
    if optix_devices:
        preferences.compute_device_type = 'OPTIX'
        for d in preferences.devices:
            d.use = True
        scene.cycles.device = 'GPU'
    elif cuda_devices:
        preferences.compute_device_type = 'CUDA'
        for d in preferences.devices:
            d.use = True
        scene.cycles.device = 'GPU'
    else:
        scene.cycles.device = 'CPU'
    print(f"Cycles device: {scene.cycles.device}")

    # Set quality / samples
    scene.cycles.samples = 256
    scene.cycles.preview_samples = 64
    scene.cycles.use_denoising = False
    scene.cycles.max_bounces = 4
    scene.cycles.diffuse_bounces = 4
    scene.cycles.glossy_bounces = 4
    scene.cycles.transmission_bounces = 4

    # 3. Resolution
    scene.render.resolution_x = 720
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100

    # 4. Color Management (Raw linear radiance)
    scene.view_settings.view_transform = 'Raw'
    scene.view_settings.look = 'None'

    # World background: pure black
    world = bpy.data.worlds.new("CornellWorld")
    scene.world = world
    world.use_nodes = True
    bg_node = world.node_tree.nodes.get("Background")
    if bg_node:
        bg_node.inputs['Color'].default_value = (0.0, 0.0, 0.0, 1.0)
        bg_node.inputs['Strength'].default_value = 0.0

    # 5. Materials
    # Base colors from cornell_box.scene
    mat_colors = {
        "ceiling_white":   (0.725, 0.71, 0.68),
        "back_wall_white": (0.725, 0.71, 0.68),
        "floor_white":     (0.725, 0.71, 0.68),
        "small_box_white": (0.725, 0.71, 0.68),
        "large_box_white": (0.725, 0.71, 0.68),
        "red":             (0.63, 0.065, 0.05),
        "green":           (0.14, 0.45, 0.091)
    }

    materials = {}
    for mat_name, rgb in mat_colors.items():
        mat = bpy.data.materials.new(name=mat_name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs['Base Color'].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
            bsdf.inputs['Roughness'].default_value = 0.5
            bsdf.inputs['Metallic'].default_value = 0.0
            bsdf.inputs['IOR'].default_value = 1.5
        materials[mat_name] = mat

    # 6. Import meshes
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    mesh_dir = os.path.join(repo_root, "Assets", "cornell_box")

    mesh_specs = [
        ("cbox_ceiling.obj",  "ceiling_white",   (0.278,  0.5488, 0.27955)),
        ("cbox_floor.obj",    "floor_white",     (0.2756, 0.0,    0.2796)),
        ("cbox_back.obj",     "back_wall_white", (0.2764, 0.2744, 0.5592)),
        ("cbox_smallbox.obj", "small_box_white", (0.1855, 0.0825, 0.169)),
        ("cbox_largebox.obj", "large_box_white", (0.3685, 0.165,  0.35125)),
        ("cbox_greenwall.obj","green",           (0.0,    0.2744, 0.2796)),
        ("cbox_redwall.obj",  "red",             (0.5536, 0.2744, 0.2796))
    ]

    for filename, mat_name, pos in mesh_specs:
        filepath = os.path.join(mesh_dir, filename)
        # Identity import: preserves original OBJ vertex coordinates directly into Blender
        bpy.ops.wm.obj_import(filepath=filepath, forward_axis='Y', up_axis='Z')
        obj = bpy.context.selected_objects[0]
        obj.name = filename.replace(".obj", "")
        obj.scale = (0.01, 0.01, 0.01)
        obj.location = pos
        # Assign material
        obj.data.materials.clear()
        obj.data.materials.append(materials[mat_name])

    # 7. Create Quad Area Light
    # In RTRT: pos=(0.343, 0.5478, 0.227), v1=(0.343, 0.5478, 0.332), v2=(0.213, 0.5478, 0.227)
    # Emission: (17.0, 17.0, 17.0)
    light_p0 = (0.34299999, 0.54779997, 0.22700010)
    light_p1 = (0.34299999, 0.54779997, 0.33200008)
    light_p2 = (0.21300001, 0.54779997, 0.33200008)
    light_p3 = (0.21300001, 0.54779997, 0.22700010)

    mesh_data = bpy.data.meshes.new("QuadLightMesh")
    mesh_data.from_pydata(
        [light_p0, light_p1, light_p2, light_p3],
        [],
        [(0, 1, 2, 3)]
    )
    mesh_data.update()
    light_obj = bpy.data.objects.new("QuadLight", mesh_data)
    scene.collection.objects.link(light_obj)

    light_mat = bpy.data.materials.new("LightEmission")
    light_mat.use_nodes = True
    nodes = light_mat.node_tree.nodes
    nodes.clear()

    geom_node = nodes.new(type='ShaderNodeNewGeometry')
    emission_node = nodes.new(type='ShaderNodeEmission')
    emission_node.inputs['Color'].default_value = (17.0, 17.0, 17.0, 1.0)
    emission_node.inputs['Strength'].default_value = 1.0

    transparent_node = nodes.new(type='ShaderNodeBsdfTransparent')
    mix_node = nodes.new(type='ShaderNodeMixShader')
    output_node = nodes.new(type='ShaderNodeOutputMaterial')

    light_mat.node_tree.links.new(geom_node.outputs['Backfacing'], mix_node.inputs['Fac'])
    light_mat.node_tree.links.new(emission_node.outputs['Emission'], mix_node.inputs[1])
    light_mat.node_tree.links.new(transparent_node.outputs['BSDF'], mix_node.inputs[2])
    light_mat.node_tree.links.new(mix_node.outputs['Shader'], output_node.inputs['Surface'])
    light_obj.data.materials.append(light_mat)

    # In RTRT, primary rays don't hit the light quad (u_ShowLights == 0)
    light_obj.visible_camera = False

    # 8. Setup Camera matching pathtracer_cornell
    # Pos: [0.276, 0.265, -0.75], Pivot: [0.276, 0.265, 0.1], FOV: 40.0
    cam_data = bpy.data.cameras.new("CornellCam")
    cam_data.sensor_fit = 'VERTICAL'
    cam_data.angle = math.radians(40.0)
    cam_data.clip_start = 0.5
    cam_data.clip_end = 50.0

    cam_obj = bpy.data.objects.new("CornellCam", cam_data)
    scene.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    # Camera World Matrix
    # Local X (Right): (-1, 0, 0)
    # Local Y (Up):    (0, 1, 0)
    # Local -Z (Fwd):  (0, 0, 1) -> Local Z: (0, 0, -1)
    # Origin:          (0.276, 0.265, -0.75)
    cam_matrix = Matrix([
        [-1.0,  0.0,  0.0,  0.276],
        [ 0.0,  1.0,  0.0,  0.265],
        [ 0.0,  0.0, -1.0, -0.750],
        [ 0.0,  0.0,  0.0,  1.000]
    ])
    cam_obj.matrix_world = cam_matrix

    # 9. Render to 32-bit OpenEXR
    artifacts_dir = os.path.join(repo_root, "Tests", "Artifacts", "blender_reference_cornell")
    os.makedirs(artifacts_dir, exist_ok=True)
    exr_path = os.path.join(artifacts_dir, "temp_cycles.exr")

    scene.render.image_settings.file_format = 'OPEN_EXR'
    scene.render.image_settings.color_depth = '32'
    scene.render.filepath = exr_path

    print("Starting Cycles render...")
    bpy.ops.render.render(write_still=True)
    print("Cycles render complete.")

    # 10. Load rendered EXR float32 pixels
    exr_img = bpy.data.images.load(exr_path)
    w, h = exr_img.size
    pixels = np.array(exr_img.pixels[:], dtype=np.float32).reshape((h, w, 4))
    rgb_bottom_to_top = pixels[:, :, :3]  # Scanline 0 is bottom
    bpy.data.images.remove(exr_img)
    if os.path.exists(exr_path):
        os.remove(exr_path)

    # 11. Write PFM (PFM stores scanlines bottom-to-top)
    pfm_path = os.path.join(artifacts_dir, "reference.pfm")
    with open(pfm_path, "wb") as f:
        header = f"PF\n{w} {h}\n-1.0\n".encode("ascii")
        f.write(header)
        f.write(rgb_bottom_to_top.astype("<f4").tobytes())
    print(f"Saved linear PFM: {pfm_path} ({os.path.getsize(pfm_path)} bytes)")

    # 12. Write tone-mapped PNG preview matching RTRT's WriteDiagnosticPNG
    # (Reinhard x / (1 + x) followed by gamma 2.2, top-to-bottom scanlines)
    rgb_top_to_bottom = np.flipud(rgb_bottom_to_top)
    mapped = rgb_top_to_bottom / (1.0 + np.maximum(0.0, rgb_top_to_bottom))
    srgb = np.power(np.clip(mapped, 0.0, 1.0), 1.0 / 2.2)
    png_rgba = np.zeros((h, w, 4), dtype=np.uint8)
    png_rgba[:, :, :3] = np.clip(srgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    png_rgba[:, :, 3] = 255

    # Save PNG using Blender image API
    png_path = os.path.join(artifacts_dir, "reference.png")
    # Pack flat into Blender Image
    preview_img = bpy.data.images.new("Preview", width=w, height=h)
    # Blender image.pixels expects bottom-to-top float [0, 1]
    preview_pixels = (np.flipud(png_rgba).astype(np.float32) / 255.0).reshape(-1)
    preview_img.pixels = preview_pixels.tolist()
    preview_img.filepath_raw = png_path
    preview_img.file_format = 'PNG'
    preview_img.save()
    bpy.data.images.remove(preview_img)
    print(f"Saved PNG preview: {png_path}")

if __name__ == "__main__":
    render_cornell()
