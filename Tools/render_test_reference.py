#!/usr/bin/env python3
"""
render_test_reference.py - Replicate RTRT render tests in Blender Cycles

Generates ground-truth reference images (PFM and diagnostic PNG) by parsing
RTRT test manifests and .scene definitions, reconstructing the scene in Blender,
and rendering with Cycles in background mode.

Dual-mode script:
- Run from terminal/PowerShell with system Python:
    python Tools/render_test_reference.py --test pathtracer_cornell
  (Automatically discovers Blender and invokes it in background mode)
- Run inside Blender:
    blender --background --python Tools/render_test_reference.py -- --test pathtracer_cornell
"""

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import warnings

# Suppress known Blender API deprecation notices
warnings.filterwarnings("ignore", category=DeprecationWarning)

# Safe imports: bpy, mathutils, and numpy are bundled inside Blender
try:
    import bpy
    import numpy as np
    from mathutils import Matrix, Vector, Quaternion
except ImportError:
    bpy = None
    np = None
    Matrix = None
    Vector = None
    Quaternion = None


# -----------------------------------------------------------------------------
# System Python Launcher (Invoked when run outside Blender)
# -----------------------------------------------------------------------------
def find_blender():
    """Attempt to locate blender.exe on the system."""
    env_blender = os.environ.get("BLENDER_PATH")
    if env_blender and os.path.isfile(env_blender):
        return env_blender

    which_blender = shutil.which("blender")
    if which_blender:
        return which_blender

    common_paths = [
        r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe",
        r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe",
        r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe",
        r"C:\Program Files\Blender Foundation\Blender 4.3\blender.exe",
        r"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe",
        r"C:\Program Files\Blender Foundation\Blender\blender.exe",
    ]
    for p in common_paths:
        if os.path.isfile(p):
            return p

    return None


def run_via_blender(args):
    """Delegate script execution into Blender background mode."""
    blender_exe = args.blender or find_blender()
    if not blender_exe or not os.path.isfile(blender_exe):
        print("ERROR: Could not locate blender.exe.", file=sys.stderr)
        print("Please specify the path using --blender <path> or set BLENDER_PATH environment variable.", file=sys.stderr)
        sys.exit(1)

    this_script = os.path.abspath(__file__)
    forwarded_args = sys.argv[1:]

    # Remove any --blender argument from forwarded args
    filtered_args = []
    skip_next = False
    for a in forwarded_args:
        if skip_next:
            skip_next = False
            continue
        if a == "--blender":
            skip_next = True
            continue
        if a.startswith("--blender="):
            continue
        filtered_args.append(a)

    cmd = [blender_exe, "--background", "--python", this_script, "--"] + filtered_args
    print(f"[Launcher] Executing Blender: {blender_exe}")
    sys.stdout.flush()
    result = subprocess.run(cmd)
    sys.exit(result.returncode)


# -----------------------------------------------------------------------------
# Scene Tokenizer and Parser
# -----------------------------------------------------------------------------
class SceneBlock:
    """Represents a single block from an RTRT .scene file."""
    def __init__(self, block_type, name=""):
        self.type = block_type.lower()
        self.name = name
        self.properties = {}

    def get(self, key, default=None):
        return self.properties.get(key.lower(), default)

    def get_vec3(self, key, default=None):
        val = self.get(key)
        if val is None:
            return default
        try:
            tokens = val.split()
            return [float(tokens[0]), float(tokens[1]), float(tokens[2])]
        except Exception:
            return default

    def get_float(self, key, default=0.0):
        val = self.get(key)
        if val is None:
            return default
        try:
            return float(val.split()[0])
        except Exception:
            return default

    def get_bool(self, key, default=False):
        val = self.get(key)
        if val is None:
            return default
        v = val.strip().lower()
        return v in ("true", "1", "yes", "on")


def parse_scene_file(filepath):
    """Parse an RTRT .scene file into a list of SceneBlock objects."""
    if not os.path.isfile(filepath):
        raise FileNotFoundError(f"Scene file not found: {filepath}")

    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    blocks = []
    current_block = None
    state = "EXPECT_BLOCK"  # "EXPECT_BLOCK", "EXPECT_BRACE", "IN_BLOCK"

    for line in lines:
        line = line.split("#")[0].strip()
        if not line:
            continue
        tokens = line.split()
        if not tokens:
            continue

        if state == "EXPECT_BLOCK":
            block_type = tokens[0]
            block_name = tokens[1] if len(tokens) > 1 and tokens[1] != "{" else ""
            current_block = SceneBlock(block_type, block_name)
            if "{" in tokens:
                state = "IN_BLOCK"
                brace_idx = tokens.index("{")
                after_brace = tokens[brace_idx + 1:]
                if after_brace and after_brace[0] != "}":
                    current_block.properties[after_brace[0].lower()] = " ".join(after_brace[1:])
            else:
                state = "EXPECT_BRACE"
        elif state == "EXPECT_BRACE":
            if "{" in tokens:
                state = "IN_BLOCK"
                brace_idx = tokens.index("{")
                after_brace = tokens[brace_idx + 1:]
                if after_brace and after_brace[0] != "}":
                    current_block.properties[after_brace[0].lower()] = " ".join(after_brace[1:])
        elif state == "IN_BLOCK":
            if "}" in tokens:
                blocks.append(current_block)
                current_block = None
                state = "EXPECT_BLOCK"
            else:
                key = tokens[0].lower()
                val = " ".join(tokens[1:])
                current_block.properties[key] = val

    return blocks


def load_scene_blocks(repo_root, scene_rel_path):
    """Locate and parse the scene file into SceneBlocks."""
    scene_file = os.path.join(repo_root, "Assets", scene_rel_path)
    if not os.path.isfile(scene_file):
        scene_file = os.path.join(repo_root, scene_rel_path)
    if not os.path.isfile(scene_file):
        raise FileNotFoundError(f"Could not locate scene file: {scene_rel_path}")

    print(f"Parsing scene: {scene_file}")
    return parse_scene_file(scene_file)


def categorize_scene_blocks(blocks):
    """Categorize raw scene blocks by type."""
    renderer_block = None
    camera_block = None
    materials = {}
    mesh_blocks = []
    gltf_blocks = []
    light_blocks = []

    for b in blocks:
        if b.type == "renderer":
            renderer_block = b
        elif b.type == "camera":
            camera_block = b
        elif b.type == "material":
            materials[b.name] = b
        elif b.type == "mesh":
            mesh_blocks.append(b)
        elif b.type == "gltf":
            gltf_blocks.append(b)
        elif b.type == "light":
            light_blocks.append(b)

    return renderer_block, camera_block, materials, mesh_blocks, gltf_blocks, light_blocks


# -----------------------------------------------------------------------------
# Manifest Resolver
# -----------------------------------------------------------------------------
class TestConfig:
    """Resolved test configuration from manifest."""
    def __init__(self, name):
        self.name = name
        self.scene_path = ""
        self.resolution = [1280, 720]
        self.bounces = 4
        self.samples = 256
        self.frames = 1
        self.environment_map = ""
        self.camera_override = None
        self.material_override = None
        self.baseline_path = ""
        self.reference_baseline_path = ""
        self.settings = {}


def resolve_manifest_test(manifest_path, test_name):
    """Load manifest and resolve profile inheritance + test overrides."""
    with open(manifest_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    profiles = data.get("profiles", {})
    tests = data.get("tests", [])

    target_test = None
    for t in tests:
        if t.get("name") == test_name:
            target_test = t
            break

    if not target_test:
        available = [t.get("name") for t in tests if t.get("name")]
        raise ValueError(f"Test '{test_name}' not found in manifest {manifest_path}. Available: {', '.join(available[:10])}...")

    cfg = TestConfig(test_name)
    cfg.scene_path = target_test.get("scene", "")

    # 1. Profile defaults
    profile_name = target_test.get("profile", "")
    profile = profiles.get(profile_name, {})
    if "resolution" in profile:
        cfg.resolution = list(profile["resolution"])
    if "frames" in profile:
        cfg.frames = profile["frames"]
    if "settings" in profile:
        p_settings = profile["settings"]
        cfg.settings.update(p_settings)
        if "bounces" in p_settings:
            cfg.bounces = p_settings["bounces"]
        if "samples_per_pixel" in p_settings:
            cfg.samples = max(cfg.samples, p_settings["samples_per_pixel"])

    # 2. Test overrides
    if "resolution" in target_test:
        cfg.resolution = list(target_test["resolution"])
    if "frames" in target_test:
        cfg.frames = target_test["frames"]
    if "environment_map" in target_test:
        cfg.environment_map = target_test["environment_map"]
    if "settings" in target_test:
        cfg.settings.update(target_test["settings"])
        if "bounces" in target_test["settings"]:
            cfg.bounces = target_test["settings"]["bounces"]
    if "camera" in target_test:
        cfg.camera_override = target_test["camera"]
    if "material" in target_test:
        cfg.material_override = target_test["material"]

    cfg.baseline_path = target_test.get("baseline", f"Tests/Baselines/{test_name}.pfm")
    cfg.reference_baseline_path = target_test.get("reference_baseline", "")

    return cfg


def list_pathtracer_tests(manifest_path):
    """Return all test names using the pathtracer backend."""
    with open(manifest_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    profiles = data.get("profiles", {})
    pt_profiles = {name for name, p in profiles.items() if p.get("backend") == "pathtracer"}

    pt_tests = []
    for t in data.get("tests", []):
        if t.get("profile") in pt_profiles:
            pt_tests.append(t.get("name"))
    return pt_tests


# -----------------------------------------------------------------------------
# Blender Cycles Configuration
# -----------------------------------------------------------------------------
def setup_cycles_renderer(scene, cfg, samples_override=None):
    """Configure Blender Cycles engine, compute devices, samples, and color management."""
    scene.render.engine = 'CYCLES'
    prefs = bpy.context.preferences.addons['cycles'].preferences
    cuda_devices = prefs.get_devices_for_type('CUDA')
    optix_devices = prefs.get_devices_for_type('OPTIX')

    if optix_devices:
        prefs.compute_device_type = 'OPTIX'
        for d in prefs.devices:
            d.use = True
        scene.cycles.device = 'GPU'
    elif cuda_devices:
        prefs.compute_device_type = 'CUDA'
        for d in prefs.devices:
            d.use = True
        scene.cycles.device = 'GPU'
    else:
        scene.cycles.device = 'CPU'
    print(f"Cycles device: {scene.cycles.device}")

    cycles_samples = samples_override if samples_override else max(256, cfg.samples)
    scene.cycles.samples = cycles_samples
    scene.cycles.preview_samples = min(64, cycles_samples)
    scene.cycles.use_denoising = False
    scene.cycles.max_bounces = max(cfg.bounces, 4)
    scene.cycles.diffuse_bounces = max(cfg.bounces, 4)
    scene.cycles.glossy_bounces = max(cfg.bounces, 4)
    scene.cycles.transmission_bounces = max(cfg.bounces, 4)

    w, h = cfg.resolution
    scene.render.resolution_x = w
    scene.render.resolution_y = h
    scene.render.resolution_percentage = 100

    scene.view_settings.view_transform = 'Raw'
    scene.view_settings.look = 'None'

    return cycles_samples


def setup_world_environment(scene, cfg, renderer_block, repo_root):
    """Configure world background and equirectangular HDR skybox."""
    enable_background = cfg.settings.get("background", True)
    env_map_path = cfg.environment_map
    if not env_map_path and renderer_block:
        if renderer_block.get_bool("enableskybox", False) and enable_background:
            env_map_path = renderer_block.get("envmapfile") or renderer_block.get("envmap") or ""
    elif not enable_background:
        env_map_path = ""

    env_intensity = 1.0
    if renderer_block:
        env_intensity = renderer_block.get_float("envmapintensity", 1.0)
        hdr_mult = renderer_block.get_float("hdrmultiplier", 1.0)
        env_intensity *= hdr_mult

    world = bpy.data.worlds.new("RTRT_World")
    scene.world = world
    if hasattr(world, "use_nodes"):
        world.use_nodes = True
    wnodes = world.node_tree.nodes
    wnodes.clear()
    w_out = wnodes.new(type='ShaderNodeOutputWorld')
    w_bg = wnodes.new(type='ShaderNodeBackground')

    if env_map_path:
        env_file = os.path.join(repo_root, "Assets", env_map_path)
        if not os.path.isfile(env_file):
            env_file = os.path.join(repo_root, env_map_path)
        if os.path.isfile(env_file):
            print(f"Loading Environment Map: {env_file} (intensity: {env_intensity})")
            tex_env = wnodes.new(type='ShaderNodeTexEnvironment')
            tex_env.image = bpy.data.images.load(env_file)
            tex_coord = wnodes.new(type='ShaderNodeTexCoord')
            mapping = wnodes.new(type='ShaderNodeMapping')
            # Align RTRT +Y pole with Blender +Z pole: rotate +90 degrees around X
            mapping.inputs['Rotation'].default_value[0] = math.radians(90.0)

            world.node_tree.links.new(tex_coord.outputs['Generated'], mapping.inputs['Vector'])
            world.node_tree.links.new(mapping.outputs['Vector'], tex_env.inputs['Vector'])
            world.node_tree.links.new(tex_env.outputs['Color'], w_bg.inputs['Color'])
            w_bg.inputs['Strength'].default_value = env_intensity
        else:
            print(f"Warning: Environment map file not found: {env_map_path}")
            w_bg.inputs['Color'].default_value = (0.0, 0.0, 0.0, 1.0)
            w_bg.inputs['Strength'].default_value = 0.0
    else:
        w_bg.inputs['Color'].default_value = (0.0, 0.0, 0.0, 1.0)
        w_bg.inputs['Strength'].default_value = 0.0

    world.node_tree.links.new(w_bg.outputs['Background'], w_out.inputs['Surface'])


# -----------------------------------------------------------------------------
# Materials and Textures
# -----------------------------------------------------------------------------
def create_pbr_material(mat_name, block, repo_root, bpy_materials):
    """Build a Principled BSDF material with PBR properties and texture maps."""
    if mat_name in bpy_materials:
        return bpy_materials[mat_name]

    mat = bpy.data.materials.new(name=mat_name)
    if hasattr(mat, "use_nodes"):
        mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")

    if block:
        # PBR Scalar Parameters
        color = block.get_vec3("color") or block.get_vec3("albedo") or [0.8, 0.8, 0.8]
        roughness = block.get_float("roughness", 0.5)
        metallic = block.get_float("metallic", 0.0)
        ior = block.get_float("ior", 1.5)
        spectrans = block.get_float("spectrans", block.get_float("transmission", 0.0))

        if bsdf:
            bsdf.inputs['Base Color'].default_value = (color[0], color[1], color[2], 1.0)
            bsdf.inputs['Roughness'].default_value = roughness
            bsdf.inputs['Metallic'].default_value = metallic
            bsdf.inputs['IOR'].default_value = ior
            if 'Transmission Weight' in bsdf.inputs:
                bsdf.inputs['Transmission Weight'].default_value = spectrans
            elif 'Transmission' in bsdf.inputs:
                bsdf.inputs['Transmission'].default_value = spectrans

        # Textures: Albedo
        albedo_tex = block.get("albedotexture")
        if albedo_tex and bsdf:
            tex_path = os.path.join(repo_root, "Assets", albedo_tex)
            if os.path.isfile(tex_path):
                tex_node = nodes.new(type='ShaderNodeTexImage')
                tex_node.image = bpy.data.images.load(tex_path)
                mat.node_tree.links.new(tex_node.outputs['Color'], bsdf.inputs['Base Color'])

        # Textures: Metallic-Roughness (Green = Roughness, Blue = Metallic)
        mr_tex = block.get("metallicroughnesstexture")
        if mr_tex and bsdf:
            tex_path = os.path.join(repo_root, "Assets", mr_tex)
            if os.path.isfile(tex_path):
                tex_node = nodes.new(type='ShaderNodeTexImage')
                tex_node.image = bpy.data.images.load(tex_path)
                tex_node.image.colorspace_settings.name = 'Non-Color'
                sep_node = nodes.new(type='ShaderNodeSeparateColor')
                mat.node_tree.links.new(tex_node.outputs['Color'], sep_node.inputs['Color'])
                mat.node_tree.links.new(sep_node.outputs['Green'], bsdf.inputs['Roughness'])
                mat.node_tree.links.new(sep_node.outputs['Blue'], bsdf.inputs['Metallic'])

        # Textures: Normal Map
        normal_tex = block.get("normaltexture")
        if normal_tex and bsdf:
            tex_path = os.path.join(repo_root, "Assets", normal_tex)
            if os.path.isfile(tex_path):
                tex_node = nodes.new(type='ShaderNodeTexImage')
                tex_node.image = bpy.data.images.load(tex_path)
                tex_node.image.colorspace_settings.name = 'Non-Color'
                norm_map = nodes.new(type='ShaderNodeNormalMap')
                mat.node_tree.links.new(tex_node.outputs['Color'], norm_map.inputs['Color'])
                mat.node_tree.links.new(norm_map.outputs['Normal'], bsdf.inputs['Normal'])

    bpy_materials[mat_name] = mat
    return mat


def setup_scene_materials(materials_dict, repo_root):
    """Create all materials defined in the scene file."""
    bpy_materials = {}
    for name, block in materials_dict.items():
        create_pbr_material(name, block, repo_root, bpy_materials)
    return bpy_materials


# -----------------------------------------------------------------------------
# Geometry and Mesh Import
# -----------------------------------------------------------------------------
def import_mesh_instances(scene, mesh_blocks, bpy_materials, repo_root):
    """Import OBJ meshes with identity axis mapping and apply spatial transforms."""
    for mb in mesh_blocks:
        filename = mb.get("file")
        if not filename:
            continue
        mesh_file = os.path.join(repo_root, "Assets", filename)
        if not os.path.isfile(mesh_file):
            print(f"Warning: Mesh file not found: {mesh_file}")
            continue

        ext = os.path.splitext(filename)[1].lower()
        if ext == ".obj":
            before_objs = set(scene.objects)
            bpy.ops.wm.obj_import(filepath=mesh_file, forward_axis='Y', up_axis='Z')
            new_objs = [o for o in scene.objects if o not in before_objs]
            if not new_objs:
                continue

            mat_name = mb.get("material")
            scale = mb.get_vec3("scale", [1.0, 1.0, 1.0])
            pos = mb.get_vec3("position", [0.0, 0.0, 0.0])

            rot_str = mb.get("rotation")
            rot_quat = None
            if rot_str:
                tokens = [float(x) for x in rot_str.split()]
                if len(tokens) == 4:
                    rot_quat = (tokens[3], tokens[0], tokens[1], tokens[2])

            matrix_str = mb.get("matrix")
            mat_world = None
            if matrix_str:
                m_tokens = [float(x) for x in matrix_str.split()]
                if len(m_tokens) == 16:
                    mat_world = Matrix([
                        [m_tokens[0], m_tokens[4], m_tokens[8],  m_tokens[12]],
                        [m_tokens[1], m_tokens[5], m_tokens[9],  m_tokens[13]],
                        [m_tokens[2], m_tokens[6], m_tokens[10], m_tokens[14]],
                        [m_tokens[3], m_tokens[7], m_tokens[11], m_tokens[15]]
                    ])

            for obj in new_objs:
                # Assign material to every imported mesh object
                if mat_name and mat_name in bpy_materials:
                    obj.data.materials.clear()
                    obj.data.materials.append(bpy_materials[mat_name])

                # Apply transform
                if mat_world is not None:
                    obj.matrix_world = mat_world
                else:
                    obj.scale = (scale[0], scale[1], scale[2])
                    obj.location = (pos[0], pos[1], pos[2])
                    if rot_quat is not None:
                        obj.rotation_mode = 'QUATERNION'
                        obj.rotation_quaternion = rot_quat


def import_gltf_instances(scene, gltf_blocks, repo_root):
    """Import glTF scenes and invert Blender's default glTF axis conversion."""
    for gb in gltf_blocks:
        filename = gb.get("file")
        if not filename:
            continue
        gltf_file = os.path.join(repo_root, "Assets", filename)
        if not os.path.isfile(gltf_file):
            print(f"Warning: glTF file not found: {gltf_file}")
            continue

        print(f"Importing glTF: {gltf_file}")
        before_objs = set(scene.objects)
        bpy.ops.import_scene.gltf(filepath=gltf_file)
        new_objs = [o for o in scene.objects if o not in before_objs]

        # Undo Blender's automatic glTF axis conversion:
        # Blender maps (X, Y, Z)_gltf -> (X, -Z, Y)_blender.
        # To restore raw glTF coordinates matching RTRT:
        # X_out = X_in, Y_out = Z_in, Z_out = -Y_in.
        rot_fix = Matrix([
            [1.0,  0.0, 0.0, 0.0],
            [0.0,  0.0, 1.0, 0.0],
            [0.0, -1.0, 0.0, 0.0],
            [0.0,  0.0, 0.0, 1.0]
        ])
        for o in new_objs:
            if o.parent is None:
                o.matrix_world = rot_fix @ o.matrix_world


# -----------------------------------------------------------------------------
# Lighting
# -----------------------------------------------------------------------------
def _calculate_light_emission(light_block):
    """Compute emission color and intensity matching RTRT loader logic."""
    emission = light_block.get_vec3("emission", [1.0, 1.0, 1.0])
    intensity_val = light_block.get_float("intensity", 0.0)
    max_comp = max(emission[0], emission[1], emission[2])

    if intensity_val > 0.0:
        calc_intensity = intensity_val
    else:
        calc_intensity = max(max_comp, 1.0)

    clamped_emission = [min(c, 1.0) for c in emission]
    final_emission = [c * calc_intensity for c in clamped_emission]
    return emission, calc_intensity, final_emission


def create_quad_light(scene, idx, light_block, calc_intensity, final_emission):
    """Create an analytical quad light with transparent backing to avoid self-shadowing."""
    pos = light_block.get_vec3("position", [0.0, 0.0, 0.0])
    v1 = light_block.get_vec3("v1", [pos[0] + 1.0, pos[1], pos[2]])
    v2 = light_block.get_vec3("v2", [pos[0], pos[1], pos[2] + 1.0])
    p0 = pos
    p1 = v1
    p2 = [v1[0] + (v2[0] - pos[0]), v1[1] + (v2[1] - pos[1]), v1[2] + (v2[2] - pos[2])]
    p3 = v2

    mesh_data = bpy.data.meshes.new(f"QuadLightMesh_{idx}")
    mesh_data.from_pydata([p0, p1, p2, p3], [], [(0, 1, 2, 3)])
    mesh_data.update()
    light_obj = bpy.data.objects.new(f"QuadLight_{idx}", mesh_data)
    scene.collection.objects.link(light_obj)

    # Two-sided emission with transparent backface so ceiling rays are not blocked
    l_mat = bpy.data.materials.new(name=f"LightEmission_{idx}")
    if hasattr(l_mat, "use_nodes"):
        l_mat.use_nodes = True
    lnodes = l_mat.node_tree.nodes
    lnodes.clear()

    geom_node = lnodes.new(type='ShaderNodeNewGeometry')
    em_node = lnodes.new(type='ShaderNodeEmission')
    em_node.inputs['Color'].default_value = (final_emission[0], final_emission[1], final_emission[2], 1.0)
    em_node.inputs['Strength'].default_value = 1.0

    trans_node = lnodes.new(type='ShaderNodeBsdfTransparent')
    mix_node = lnodes.new(type='ShaderNodeMixShader')
    out_node = lnodes.new(type='ShaderNodeOutputMaterial')

    l_mat.node_tree.links.new(geom_node.outputs['Backfacing'], mix_node.inputs['Fac'])
    l_mat.node_tree.links.new(em_node.outputs['Emission'], mix_node.inputs[1])
    l_mat.node_tree.links.new(trans_node.outputs['BSDF'], mix_node.inputs[2])
    l_mat.node_tree.links.new(mix_node.outputs['Shader'], out_node.inputs['Surface'])

    light_obj.data.materials.append(l_mat)
    light_obj.visible_camera = False


def create_sphere_light(scene, idx, light_block, calc_intensity, emission):
    """Create a point light with flux scaled by diffuse sphere surface radiance."""
    pos = light_block.get_vec3("position", [0.0, 0.0, 0.0])
    radius = light_block.get_float("radius", 0.1)

    # Total radiant power (Watts) for diffuse sphere: Flux = 4 * PI^2 * R^2 * L_e
    l_data = bpy.data.lights.new(name=f"PointLight_{idx}", type='POINT')
    l_data.color = (min(1.0, emission[0]), min(1.0, emission[1]), min(1.0, emission[2]))
    l_data.energy = 4.0 * (math.pi ** 2) * (radius ** 2) * calc_intensity
    l_data.shadow_soft_size = radius

    l_obj = bpy.data.objects.new(f"PointLight_{idx}", l_data)
    l_obj.location = (pos[0], pos[1], pos[2])
    scene.collection.objects.link(l_obj)


def create_distant_light(scene, idx, light_block, calc_intensity, emission):
    """Create a directional sun light pointing along the specified direction."""
    l_data = bpy.data.lights.new(name=f"SunLight_{idx}", type='SUN')
    l_data.color = (min(1.0, emission[0]), min(1.0, emission[1]), min(1.0, emission[2]))
    l_data.energy = calc_intensity

    l_obj = bpy.data.objects.new(f"SunLight_{idx}", l_data)
    direction = light_block.get_vec3("direction", [0.0, -1.0, 0.0])
    dir_vec = Vector(direction).normalized()
    rot_quat = Vector((0, 0, -1)).rotation_difference(dir_vec)
    l_obj.rotation_mode = 'QUATERNION'
    l_obj.rotation_quaternion = rot_quat
    scene.collection.objects.link(l_obj)


def setup_scene_lights(scene, light_blocks):
    """Dispatch light creation based on light block types."""
    for idx, lb in enumerate(light_blocks):
        l_type = lb.get("type", "sphere").lower()
        emission, calc_intensity, final_emission = _calculate_light_emission(lb)

        if l_type == "quad":
            create_quad_light(scene, idx, lb, calc_intensity, final_emission)
        elif l_type == "sphere":
            create_sphere_light(scene, idx, lb, calc_intensity, emission)
        elif l_type == "distant":
            create_distant_light(scene, idx, lb, calc_intensity, emission)


# -----------------------------------------------------------------------------
# Camera
# -----------------------------------------------------------------------------
def setup_scene_camera(scene, camera_block, cfg):
    """Construct Blender camera matching RTRT position, pivot, FOV, and projection."""
    cam_data = bpy.data.cameras.new("RTRT_Camera")
    cam_obj = bpy.data.objects.new("RTRT_Camera", cam_data)
    scene.collection.objects.link(cam_obj)
    scene.camera = cam_obj

    cam_pos = [0.0, 0.0, -1.0]
    cam_pivot = [0.0, 0.0, 0.0]
    cam_fov = 60.0
    cam_near = 0.1
    cam_far = 100.0
    cam_type = "perspective"
    cam_ortho_height = 5.0

    if camera_block:
        cam_pos = camera_block.get_vec3("position", cam_pos)
        cam_pivot = camera_block.get_vec3("lookat") or camera_block.get_vec3("pivot") or cam_pivot
        cam_fov = camera_block.get_float("fov", cam_fov)
        cam_near = camera_block.get_float("near", cam_near)
        cam_far = camera_block.get_float("far", cam_far)
        cam_type = camera_block.get("type", "perspective").lower()
        cam_ortho_height = camera_block.get_float("orthographicheight", cam_ortho_height)

    if cfg.camera_override:
        ov = cfg.camera_override
        if "position" in ov:
            cam_pos = list(ov["position"])
        if "pivot" in ov:
            cam_pivot = list(ov["pivot"])
        if "fov" in ov:
            cam_fov = float(ov["fov"])
        if "near" in ov:
            cam_near = float(ov["near"])
        if "far" in ov:
            cam_far = float(ov["far"])

    # Compute Camera Basis Vectors matching RTRT Camera.cpp
    P = Vector(cam_pos)
    V = Vector(cam_pivot)
    dir_vec = (V - P).normalized()
    pitch = math.asin(dir_vec.y)
    yaw = math.atan2(dir_vec.z, dir_vec.x)

    F = Vector((
        math.cos(yaw) * math.cos(pitch),
        math.sin(pitch),
        math.sin(yaw) * math.cos(pitch)
    )).normalized()
    WorldUp = Vector((0.0, 1.0, 0.0))
    R = (F.cross(WorldUp)).normalized()
    U = (R.cross(F)).normalized()

    # Construct Blender Camera Matrix: Local X=R, Local Y=U, Local Z=-F
    cam_matrix = Matrix([
        [ R.x,  U.x, -F.x,  P.x],
        [ R.y,  U.y, -F.y,  P.y],
        [ R.z,  U.z, -F.z,  P.z],
        [ 0.0,  0.0,  0.0,  1.0]
    ])
    cam_obj.matrix_world = cam_matrix

    cam_data.clip_start = max(cam_near, 0.01)
    cam_data.clip_end = max(cam_far, 10.0)

    if cam_type == "orthographic":
        cam_data.type = 'ORTHO'
        cam_data.sensor_fit = 'VERTICAL'
        cam_data.ortho_scale = cam_ortho_height
    else:
        cam_data.type = 'PERSP'
        cam_data.sensor_fit = 'HORIZONTAL'
        cam_data.angle = math.radians(cam_fov)

    return cam_obj


# -----------------------------------------------------------------------------
# Rendering and Image I/O
# -----------------------------------------------------------------------------
def render_scene_to_pixels(scene, out_dir, w, h, cycles_samples):
    """Render the active scene to 32-bit OpenEXR and load float RGB scanlines."""
    exr_path = os.path.join(out_dir, "temp_cycles.exr")
    scene.render.image_settings.file_format = 'OPEN_EXR'
    scene.render.image_settings.color_depth = '32'
    scene.render.filepath = exr_path

    print(f"\n[Cycles] Rendering {w}x{h} with {cycles_samples} samples...")
    bpy.ops.render.render(write_still=True)
    print("[Cycles] Render complete.")

    exr_img = bpy.data.images.load(exr_path)
    pixels = np.array(exr_img.pixels[:], dtype=np.float32).reshape((h, w, 4))
    rgb_bottom_to_top = pixels[:, :, :3]
    bpy.data.images.remove(exr_img)
    if os.path.exists(exr_path):
        os.remove(exr_path)

    return rgb_bottom_to_top


def write_pfm(filepath, rgb_bottom_to_top, w, h):
    """Write 32-bit float RGB image in PFM format (bottom-to-top scanlines)."""
    with open(filepath, "wb") as f:
        header = f"PF\n{w} {h}\n-1.0\n".encode("ascii")
        f.write(header)
        f.write(rgb_bottom_to_top.astype("<f4").tobytes())
    print(f"[Output] Saved linear PFM: {filepath} ({os.path.getsize(filepath)} bytes)")


def write_diagnostic_png(filepath, rgb_bottom_to_top, w, h):
    """Write tonemapped preview PNG using Reinhard tonemapping + gamma 2.2."""
    rgb_top_to_bottom = np.flipud(rgb_bottom_to_top)
    mapped = rgb_top_to_bottom / (1.0 + np.maximum(0.0, rgb_top_to_bottom))
    srgb = np.power(np.clip(mapped, 0.0, 1.0), 1.0 / 2.2)
    png_rgba = np.zeros((h, w, 4), dtype=np.uint8)
    png_rgba[:, :, :3] = np.clip(srgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    png_rgba[:, :, 3] = 255

    preview_img = bpy.data.images.new("Preview", width=w, height=h)
    preview_pixels = (np.flipud(png_rgba).astype(np.float32) / 255.0).reshape(-1)
    preview_img.pixels = preview_pixels.tolist()
    preview_img.filepath_raw = filepath
    preview_img.file_format = 'PNG'
    preview_img.save()
    bpy.data.images.remove(preview_img)
    print(f"[Output] Saved preview PNG: {filepath}")


def evaluate_comparison(actual_pfm_path, rgb_bottom_to_top, w, h, out_dir):
    """Calculate error metrics (MAE, RMSE, Max) and save 5x diff preview against actual.pfm."""
    if not os.path.isfile(actual_pfm_path):
        return None

    with open(actual_pfm_path, "rb") as f:
        f.readline()
        dims = f.readline().decode().strip()
        f.readline()
        aw, ah = map(int, dims.split())
        actual_data = np.fromfile(f, dtype='<f4').reshape((ah, aw, 3))

    if (aw, ah) != (w, h):
        print(f"Warning: Dimensions mismatch with actual.pfm ({aw}x{ah} vs {w}x{h})")
        return None

    diff = np.abs(actual_data - rgb_bottom_to_top)
    mae = float(np.mean(diff))
    max_err = float(np.max(diff))
    rmse = float(np.sqrt(np.mean(diff ** 2)))

    comp_metrics = {
        "mae": mae,
        "max_absolute_error": max_err,
        "rmse": rmse,
        "actual_min": float(actual_data.min()),
        "actual_max": float(actual_data.max()),
        "actual_mean": float(actual_data.mean())
    }
    print(f"\n[Comparison vs actual.pfm]")
    print(f"  MAE (Mean Absolute Error): {mae:.6f}")
    print(f"  Max Error:                {max_err:.6f}")
    print(f"  RMSE:                     {rmse:.6f}")

    # Save 5x amplified difference image
    diff_top = np.flipud(diff)
    diff_vis = np.clip(diff_top * 5.0, 0.0, 1.0)
    diff_rgba = np.zeros((h, w, 4), dtype=np.float32)
    diff_rgba[:, :, :3] = np.flipud(diff_vis)
    diff_rgba[:, :, 3] = 1.0
    diff_img = bpy.data.images.new("DiffPreview", width=w, height=h)
    diff_img.pixels = diff_rgba.reshape(-1).tolist()
    diff_png_path = os.path.join(out_dir, "diff_amplified_5x.png")
    diff_img.filepath_raw = diff_png_path
    diff_img.file_format = 'PNG'
    diff_img.save()
    bpy.data.images.remove(diff_img)
    print(f"  Saved 5x diff preview:    {diff_png_path}")

    return comp_metrics


# -----------------------------------------------------------------------------
# Top-level Replication Coordinator
# -----------------------------------------------------------------------------
def run_blender_replication(cfg, repo_root, samples_override=None, output_dir=None, compare=False, update_baseline=False):
    """Reconstruct RTRT test scene in Blender, render in Cycles, and export ground truth."""
    print(f"\n=======================================================")
    print(f" Replicating Test in Blender Cycles: {cfg.name}")
    print(f"=======================================================")

    # 1. Reset scene & setup Cycles renderer
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    cycles_samples = setup_cycles_renderer(scene, cfg, samples_override)

    # 2. Parse and categorize scene blocks
    blocks = load_scene_blocks(repo_root, cfg.scene_path)
    renderer_block, camera_block, materials, mesh_blocks, gltf_blocks, light_blocks = categorize_scene_blocks(blocks)

    # 3. Construct scene elements
    setup_world_environment(scene, cfg, renderer_block, repo_root)
    bpy_materials = setup_scene_materials(materials, repo_root)
    import_mesh_instances(scene, mesh_blocks, bpy_materials, repo_root)
    import_gltf_instances(scene, gltf_blocks, repo_root)
    setup_scene_lights(scene, light_blocks)
    setup_scene_camera(scene, camera_block, cfg)

    # 4. Render to linear float pixels
    w, h = cfg.resolution
    out_dir = output_dir or os.path.join(repo_root, "Tests", "Artifacts", f"blender_reference_{cfg.name}")
    os.makedirs(out_dir, exist_ok=True)

    rgb_bottom_to_top = render_scene_to_pixels(scene, out_dir, w, h, cycles_samples)

    # 5. Export reference artifacts
    pfm_path = os.path.join(out_dir, "reference.pfm")
    png_path = os.path.join(out_dir, "reference.png")
    write_pfm(pfm_path, rgb_bottom_to_top, w, h)
    write_diagnostic_png(png_path, rgb_bottom_to_top, w, h)

    # 6. Quantitative comparison & report
    metrics = {
        "test": cfg.name,
        "resolution": [w, h],
        "cycles_samples": cycles_samples,
        "ref_min": float(rgb_bottom_to_top.min()),
        "ref_max": float(rgb_bottom_to_top.max()),
        "ref_mean": float(rgb_bottom_to_top.mean())
    }
    if compare:
        actual_pfm_path = os.path.join(repo_root, "Tests", "Artifacts", cfg.name, "actual.pfm")
        comp = evaluate_comparison(actual_pfm_path, rgb_bottom_to_top, w, h, out_dir)
        if comp:
            metrics["comparison"] = comp

    report_path = os.path.join(out_dir, "reference_report.json")
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(metrics, f, indent=2)

    # 7. Update baseline if requested
    if update_baseline:
        dest_path = os.path.join(repo_root, cfg.baseline_path)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        shutil.copy2(pfm_path, dest_path)
        print(f"[Update] Baseline updated: {dest_path}")

    return metrics


# -----------------------------------------------------------------------------
# Main Entry Point
# -----------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Replicate RTRT render tests in Blender Cycles ground truth.")
    parser.add_argument("--test", type=str, help="Name of the test case to replicate (e.g. pathtracer_cornell).")
    parser.add_argument("--all", action="store_true", help="Replicate all pathtracer test cases from the manifest.")
    parser.add_argument("--manifest", type=str, default="Tests/RenderTests.json", help="Path to test manifest JSON.")
    parser.add_argument("--samples", type=int, default=256, help="Cycles samples count for noise-free ground truth.")
    parser.add_argument("--compare", action="store_true", help="Compare output against existing actual.pfm if available.")
    parser.add_argument("--update-baseline", action="store_true", help="Copy reference PFM to test baseline path.")
    parser.add_argument("--output-dir", type=str, default="", help="Custom output directory.")
    parser.add_argument("--blender", type=str, default="", help="Path to blender.exe (when run with system Python).")

    is_inside_blender = "bpy" in sys.modules

    if not is_inside_blender:
        # Run via system Python: parse arguments and launch Blender in background mode
        args = parser.parse_args()
        run_via_blender(args)
    else:
        # Running inside Blender: arguments appear after '--'
        raw_args = sys.argv
        if "--" in raw_args:
            cli_args = raw_args[raw_args.index("--") + 1:]
        else:
            cli_args = []
        args = parser.parse_args(cli_args)

        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        manifest_path = os.path.join(repo_root, args.manifest)

        if not os.path.isfile(manifest_path):
            print(f"ERROR: Manifest not found: {manifest_path}", file=sys.stderr)
            sys.exit(1)

        tests_to_run = []
        if args.test:
            tests_to_run.append(args.test)
        elif args.all:
            tests_to_run = list_pathtracer_tests(manifest_path)
            print(f"Found {len(tests_to_run)} pathtracer test(s): {', '.join(tests_to_run)}")
        else:
            print("ERROR: Specify either --test <name> or --all", file=sys.stderr)
            sys.exit(1)

        results = []
        for tname in tests_to_run:
            try:
                cfg = resolve_manifest_test(manifest_path, tname)
                res = run_blender_replication(
                    cfg,
                    repo_root,
                    samples_override=args.samples,
                    output_dir=args.output_dir if len(tests_to_run) == 1 else None,
                    compare=args.compare,
                    update_baseline=args.update_baseline
                )
                results.append(res)
            except Exception as e:
                print(f"ERROR replicating test {tname}: {e}", file=sys.stderr)
                import traceback
                traceback.print_exc()

        print("\n=======================================================")
        print(f" Summary: {len(results)}/{len(tests_to_run)} tests processed successfully.")
        print("=======================================================\n")


if __name__ == "__main__":
    main()
