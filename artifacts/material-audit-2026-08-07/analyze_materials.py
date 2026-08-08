#!/usr/bin/env python3
"""Extract exact material, texture-slot, and constant statistics from FJSCENE v8."""

from __future__ import annotations

import argparse
import csv
import json
import math
import mmap
from collections import Counter, defaultdict
from pathlib import Path
import struct

import numpy as np
from PIL import Image


INVALID = 0xFFFFFFFF
VECTOR_SPECS = [
    ("strings", 1), ("vertices", 32), ("indices", 4), ("samplers", 20),
    ("texture_payload_refs", 8), ("texture_mips", 24), ("textures", 40),
    ("texture_bindings", 16), ("materials", 88), ("submeshes", 28),
    ("mesh_lods", 16), ("meshes", 12), ("triangle_bool_streams", 16),
    ("triangle_bool_values", 1), ("corner_float_streams", 20),
    ("corner_float_values", 4), ("corner_color3_streams", 20),
    ("corner_color3_values", 12), ("corner_texcoord2_streams", 20),
    ("corner_texcoord2_values", 8), ("point_instances", 40),
    ("point_batches", 80), ("static_mesh_instances", 72),
]


def dtype(names, formats, offsets, itemsize):
    return np.dtype({"names": names, "formats": formats, "offsets": offsets,
                     "itemsize": itemsize})


PAYLOAD_REF_DTYPE = dtype(["texture", "key"], ["<u4", "<u4"], [0, 4], 8)
TEXTURE_MIP_DTYPE = dtype(
    ["width", "height", "row_pitch", "slice_pitch", "data_offset_local"],
    ["<u4", "<u4", "<u4", "<u4", "<u8"], [0, 4, 8, 12, 16], 24)
TEXTURE_DTYPE = dtype(
    ["name", "width", "height", "dxgi_format", "mip_offset", "mip_count",
     "data_offset", "data_size"],
    ["<u4", "<u4", "<u4", "<u4", "<u4", "<u4", "<u8", "<u8"],
    [0, 4, 8, 12, 16, 20, 24, 32], 40)
BINDING_DTYPE = dtype(
    ["texture", "sampler", "channel", "flags"], ["<u4"] * 4,
    [0, 4, 8, 12], 16)
MATERIAL_DTYPE = dtype(
    ["name", "base_color", "emissive", "roughness", "metallic", "opacity",
     "opacity_threshold", "ior", "specular", "clearcoat", "clearcoat_roughness",
     "slot_base_color", "slot_normal", "slot_roughness", "slot_metallic",
     "slot_opacity", "slot_emissive"],
    ["<u4", ("<f4", (4,)), ("<f4", (3,))] + ["<f4"] * 8 + ["<u4"] * 6,
    [0, 4, 20, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84], 88)
SUBMESH_DTYPE = dtype(
    ["name", "vertex_offset", "vertex_count", "index_offset", "index_count", "material", "flags"],
    ["<u4"] * 7, list(range(0, 28, 4)), 28)
MESH_LOD_DTYPE = dtype(
    ["submesh_offset", "submesh_count", "max_deviation", "reserved"],
    ["<u4", "<u4", "<f4", "<u4"], [0, 4, 8, 12], 16)
MESH_DTYPE = dtype(["name", "lod_offset", "lod_count"], ["<u4"] * 3,
                   [0, 4, 8], 12)

SLOTS = ["base_color", "normal", "roughness", "metallic", "opacity", "emissive"]
SLOT_FIELDS = {slot: f"slot_{slot}" for slot in SLOTS}
CHANNEL_NAMES = {0: "RGBA", 1: "R", 2: "G", 3: "B", 4: "A", 5: "RGB"}
FLAG_NAMES = {0: "LINEAR", 1: "SRGB"}
DXGI_NAMES = {
    71: "BC1_UNORM", 72: "BC1_UNORM_SRGB", 80: "BC4_UNORM",
    83: "BC5_UNORM", 95: "BC6H_UF16", 98: "BC7_UNORM",
    99: "BC7_UNORM_SRGB",
}
DEFAULTS = {
    "base_color": (0.18, 0.18, 0.18, 1.0),
    "emissive": (0.0, 0.0, 0.0),
    "roughness": 0.5, "metallic": 0.0, "opacity": 1.0,
    "opacity_threshold": 0.0, "ior": 1.5, "specular": 0.5,
    "clearcoat": 0.0, "clearcoat_roughness": 0.01,
}
SHADER_SEMANTICS = {
    "base_color": "constant multiplied by texture RGB (and alpha unless RGB-only)",
    "normal": "texture only; no material normal constant",
    "roughness": "texture overrides constant",
    "metallic": "texture overrides constant",
    "opacity": "constant * base alpha * opacity texture",
    "emissive": "constant RGB multiplied by texture RGB",
}
CONSTANT_SHADER_USE = {
    "base_color": "used (multiplied by base-color texture)",
    "emissive": "used (multiplied by emissive texture)",
    "roughness": "used unless roughness texture overrides it",
    "metallic": "used unless metallic texture overrides it",
    "opacity": "used (multiplies albedo alpha and opacity texture)",
    "opacity_threshold": "used by clip()",
    "ior": "uploaded but not read by Forward.ps.hlsl",
    "specular": "uploaded but not read by Forward.ps.hlsl",
    "clearcoat": "uploaded but not read by Forward.ps.hlsl",
    "clearcoat_roughness": "uploaded but not read by Forward.ps.hlsl",
}


class Scene:
    def __init__(self, path: Path):
        self.path = path
        self.file = path.open("rb")
        self.mm = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)
        magic, version, header_size, vertex_size, info_size, payload_size, tex_size = \
            struct.unpack_from("<8sIIIIQQ", self.mm, 0)
        if (magic, version, header_size, vertex_size, info_size) != \
                (b"FJSCENE\0", 8, 40, 32, 16):
            raise ValueError("Expected the current FJSCENE v8 ABI")
        if payload_size + header_size != len(self.mm):
            raise ValueError("FJSCENE payload length mismatch")
        self.expected_texture_payload_size = tex_size
        self.vectors = {}
        cursor = header_size
        for name, item_size in VECTOR_SPECS:
            count = struct.unpack_from("<Q", self.mm, cursor)[0]
            cursor += 8
            size = count * item_size
            if cursor + size > len(self.mm):
                raise ValueError(f"{name} exceeds FJSCENE")
            self.vectors[name] = (cursor, count, item_size)
            cursor += size
        if cursor + 32 + 104 + 92 + 16 != len(self.mm):
            raise ValueError("FJSCENE trailing structure ABI mismatch")
        off, count, _ = self.vectors["strings"]
        self.strings = self.mm[off:off + count]
        self.payload_refs = self.array("texture_payload_refs", PAYLOAD_REF_DTYPE)
        self.texture_mips = self.array("texture_mips", TEXTURE_MIP_DTYPE)
        self.textures = self.array("textures", TEXTURE_DTYPE)
        self.bindings = self.array("texture_bindings", BINDING_DTYPE)
        self.materials = self.array("materials", MATERIAL_DTYPE)
        self.submeshes = self.array("submeshes", SUBMESH_DTYPE)
        self.mesh_lods = self.array("mesh_lods", MESH_LOD_DTYPE)
        self.meshes = self.array("meshes", MESH_DTYPE)
        self.sampler_count = self.vectors["samplers"][1]

    def array(self, name, array_dtype):
        offset, count, size = self.vectors[name]
        if array_dtype.itemsize != size:
            raise ValueError(f"{name} dtype mismatch")
        return np.ndarray((count,), dtype=array_dtype, buffer=self.mm, offset=offset)

    def string(self, offset, fallback=""):
        offset = int(offset)
        if offset < 0 or offset >= len(self.strings):
            return fallback
        end = self.strings.find(b"\0", offset)
        if end < 0:
            return fallback
        return self.strings[offset:end].decode("utf-8", errors="replace") or fallback

    def close(self):
        for attr in ("payload_refs", "texture_mips", "textures", "bindings",
                     "materials", "submeshes", "mesh_lods", "meshes"):
            setattr(self, attr, None)
        self.mm.close()
        self.file.close()


def write_csv(path: Path, rows: list[dict]):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as file:
        if not rows:
            return
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def joined_counter(counter: Counter) -> str:
    return "; ".join(f"{key}:{value}" for key, value in sorted(counter.items()))


def float_key(value):
    array = np.atleast_1d(value).astype(np.float64)
    return tuple(round(float(x), 6) for x in array)


def display_value(value):
    key = float_key(value)
    return key[0] if len(key) == 1 else list(key)


def portable_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def analyze(scene: Scene, output: Path):
    texture_path = scene.path.with_suffix(".fjtex")
    with texture_path.open("rb") as file:
        header = file.read(32)
    magic, version, header_size, metadata_size, payload_size = struct.unpack("<8sIIQQ", header)
    if (magic, version, header_size) != (b"FJTEX\0\0\0", 3, 32):
        raise ValueError("Expected FJTEX v3")
    if texture_path.stat().st_size != header_size + metadata_size + payload_size:
        raise ValueError("FJTEX file size mismatch")
    if payload_size != scene.expected_texture_payload_size:
        raise ValueError("FJSCENE/FJTEX payload size mismatch")

    errors = []
    payload_key_by_texture = {}
    payload_ref_counts = Counter()
    for ref in scene.payload_refs:
        texture_id = int(ref["texture"])
        payload_ref_counts[texture_id] += 1
        if texture_id >= len(scene.textures):
            errors.append(f"payload ref has invalid texture {texture_id}")
            continue
        payload_key_by_texture[texture_id] = scene.string(ref["key"], "<invalid-key>")
    if set(payload_key_by_texture) != set(range(len(scene.textures))):
        errors.append("payload refs do not cover every texture exactly")
    if any(count != 1 for count in payload_ref_counts.values()):
        errors.append("duplicate texture payload refs")

    texture_valid = np.ones(len(scene.textures), dtype=np.bool_)
    for texture_id, texture in enumerate(scene.textures):
        mip_offset, mip_count = int(texture["mip_offset"]), int(texture["mip_count"])
        data_offset, data_size = int(texture["data_offset"]), int(texture["data_size"])
        valid = (
            int(texture["width"]) > 0 and int(texture["height"]) > 0 and mip_count > 0 and
            mip_offset + mip_count <= len(scene.texture_mips) and data_size > 0 and
            data_offset + data_size <= payload_size
        )
        texture_valid[texture_id] = valid
        if not valid:
            errors.append(f"texture {texture_id} metadata/payload range invalid")

    binding_reference_count = Counter()
    texture_slot_users = defaultdict(lambda: {slot: set() for slot in SLOTS})
    material_slot_binding = {}
    for material_id, material in enumerate(scene.materials):
        for slot in SLOTS:
            binding_id = int(material[SLOT_FIELDS[slot]])
            material_slot_binding[(material_id, slot)] = binding_id
            if binding_id == INVALID:
                continue
            binding_reference_count[binding_id] += 1
            if binding_id >= len(scene.bindings):
                errors.append(f"material {material_id} {slot} binding out of range: {binding_id}")
                continue
            binding = scene.bindings[binding_id]
            texture_id, sampler_id = int(binding["texture"]), int(binding["sampler"])
            if texture_id >= len(scene.textures):
                errors.append(f"binding {binding_id} texture out of range: {texture_id}")
                continue
            if sampler_id >= scene.sampler_count:
                errors.append(f"binding {binding_id} sampler out of range: {sampler_id}")
            if int(binding["channel"]) not in CHANNEL_NAMES:
                errors.append(f"binding {binding_id} channel invalid")
            if int(binding["flags"]) not in FLAG_NAMES:
                errors.append(f"binding {binding_id} flags invalid")
            if not texture_valid[texture_id]:
                errors.append(f"binding {binding_id} points to invalid payload texture {texture_id}")
            texture_slot_users[texture_id][slot].add(material_id)

    material_meshes = defaultdict(set)
    material_submeshes_all = Counter()
    material_submeshes_lod0 = Counter()
    material_triangles_lod0 = Counter()
    for mesh_id, mesh in enumerate(scene.meshes):
        mesh_name = scene.string(mesh["name"], f"mesh_{mesh_id}")
        for local_lod in range(int(mesh["lod_count"])):
            lod = scene.mesh_lods[int(mesh["lod_offset"]) + local_lod]
            for local_submesh in range(int(lod["submesh_count"])):
                submesh = scene.submeshes[int(lod["submesh_offset"]) + local_submesh]
                material_id = int(submesh["material"])
                if material_id == INVALID:
                    continue
                if material_id >= len(scene.materials):
                    errors.append(f"submesh material out of range: {material_id}")
                    continue
                material_meshes[material_id].add(mesh_name)
                material_submeshes_all[material_id] += 1
                if local_lod == 0:
                    material_submeshes_lod0[material_id] += 1
                    material_triangles_lod0[material_id] += int(submesh["index_count"]) // 3

    slot_rows = []
    for slot in SLOTS:
        assigned_materials, binding_ids, texture_ids = [], set(), set()
        channels, flags, formats = Counter(), Counter(), Counter()
        valid_payload_assignments = 0
        for material_id in range(len(scene.materials)):
            binding_id = material_slot_binding[(material_id, slot)]
            if binding_id == INVALID:
                continue
            assigned_materials.append(material_id)
            if binding_id >= len(scene.bindings):
                continue
            binding_ids.add(binding_id)
            binding = scene.bindings[binding_id]
            texture_id = int(binding["texture"])
            channels[CHANNEL_NAMES.get(int(binding["channel"]), "INVALID")] += 1
            flags[FLAG_NAMES.get(int(binding["flags"]), "INVALID")] += 1
            if texture_id < len(scene.textures):
                texture_ids.add(texture_id)
                texture = scene.textures[texture_id]
                formats[DXGI_NAMES.get(int(texture["dxgi_format"]), str(int(texture["dxgi_format"])))] += 1
                valid_payload_assignments += int(texture_valid[texture_id])
        unique_payload_bytes = sum(int(scene.textures[i]["data_size"]) for i in texture_ids)
        slot_rows.append({
            "slot": slot, "shader_semantics": SHADER_SEMANTICS[slot],
            "assigned_materials": len(assigned_materials),
            "missing_materials": len(scene.materials) - len(assigned_materials),
            "assigned_percent": len(assigned_materials) / len(scene.materials),
            "unique_bindings": len(binding_ids), "unique_textures": len(texture_ids),
            "valid_payload_assignments": valid_payload_assignments,
            "all_assigned_resolve_to_payload": (
                valid_payload_assignments == len(assigned_materials)
                if assigned_materials else "N/A"),
            "channels": joined_counter(channels), "color_space_flags": joined_counter(flags),
            "dxgi_formats": joined_counter(formats),
            "unique_payload_bytes": unique_payload_bytes,
        })

    constant_rows = []
    for field, default in DEFAULTS.items():
        values = scene.materials[field]
        keys = [float_key(value) for value in values]
        counts = Counter(keys)
        default_key = float_key(default)
        default_count = counts[default_key]
        nonzero = sum(any(abs(x) > 1e-6 for x in key) for key in keys)
        arr = np.asarray(values, dtype=np.float64)
        constant_rows.append({
            "constant": field, "forward_shader_use": CONSTANT_SHADER_USE[field],
            "default": json.dumps(display_value(default)),
            "materials": len(values), "default_count": default_count,
            "non_default_count": len(values) - default_count,
            "nonzero_count": nonzero, "unique_value_count": len(counts),
            "minimum": json.dumps(display_value(np.min(arr, axis=0))),
            "maximum": json.dumps(display_value(np.max(arr, axis=0))),
            "most_common_values": "; ".join(
                f"{json.dumps(list(key) if len(key)>1 else key[0])}:{count}"
                for key, count in counts.most_common(8)),
        })

    material_rows, metallic_rows = [], []
    source_channel_cache = {}
    for material_id, material in enumerate(scene.materials):
        material_name = scene.string(material["name"], f"material_{material_id}")
        row = {
            "material_id": material_id, "material": material_name,
            "base_color": json.dumps(display_value(material["base_color"])),
            "emissive": json.dumps(display_value(material["emissive"])),
            "roughness": float(material["roughness"]),
            "metallic": float(material["metallic"]), "opacity": float(material["opacity"]),
            "opacity_threshold": float(material["opacity_threshold"]),
            "ior": float(material["ior"]), "specular": float(material["specular"]),
            "clearcoat": float(material["clearcoat"]),
            "clearcoat_roughness": float(material["clearcoat_roughness"]),
            "lod0_submesh_count": material_submeshes_lod0[material_id],
            "all_lod_submesh_count": material_submeshes_all[material_id],
            "lod0_triangles": material_triangles_lod0[material_id],
            "meshes": " | ".join(sorted(material_meshes[material_id])),
        }
        for slot in SLOTS:
            binding_id = material_slot_binding[(material_id, slot)]
            row[f"{slot}_binding"] = "" if binding_id == INVALID else binding_id
            row[f"{slot}_texture"] = ""
            row[f"{slot}_channel"] = ""
            row[f"{slot}_format"] = ""
            if binding_id != INVALID and binding_id < len(scene.bindings):
                binding = scene.bindings[binding_id]
                texture_id = int(binding["texture"])
                if texture_id < len(scene.textures):
                    texture = scene.textures[texture_id]
                    row[f"{slot}_texture"] = scene.string(texture["name"], f"texture_{texture_id}")
                    row[f"{slot}_channel"] = CHANNEL_NAMES.get(int(binding["channel"]), "INVALID")
                    row[f"{slot}_format"] = DXGI_NAMES.get(
                        int(texture["dxgi_format"]), str(int(texture["dxgi_format"])))
        material_rows.append(row)
        metallic_binding = material_slot_binding[(material_id, "metallic")]
        metallic_constant = float(material["metallic"])
        if abs(metallic_constant) > 1e-6 or metallic_binding != INVALID:
            source_status = "constant"
            source_path_text = ""
            source_min = source_max = source_mean = ""
            source_nonzero = source_total = ""
            effective_nonzero = abs(metallic_constant) > 1e-6
            texture_id = INVALID
            if metallic_binding != INVALID and metallic_binding < len(scene.bindings):
                binding = scene.bindings[metallic_binding]
                texture_id = int(binding["texture"])
                channel = int(binding["channel"])
                cache_key = (texture_id, channel)
                if cache_key not in source_channel_cache:
                    source_path = Path(payload_key_by_texture.get(texture_id, ""))
                    source_path_text = portable_path(source_path)
                    channel_index = {1: 0, 2: 1, 3: 2, 4: 3}.get(channel)
                    try:
                        if channel_index is None:
                            raise ValueError(
                                f"source scalar channel statistics require R/G/B/A, got {channel}")
                        with Image.open(source_path) as image:
                            pixels = np.asarray(image)
                        if pixels.ndim == 2:
                            pixels = pixels[:, :, np.newaxis]
                        if channel_index >= pixels.shape[2]:
                            raise ValueError(
                                f"image mode has {pixels.shape[2]} channels, requested {channel_index}")
                        values = pixels[:, :, channel_index].astype(np.float64)
                        source_channel_cache[cache_key] = {
                            "status": "decoded",
                            "path": source_path_text,
                            "minimum": int(np.min(values)),
                            "maximum": int(np.max(values)),
                            "mean": float(np.mean(values)),
                            "nonzero": int(np.count_nonzero(values)),
                            "total": int(values.size),
                        }
                    except Exception as exception:
                        errors.append(
                            f"metallic source channel decode failed for texture {texture_id}: {exception}")
                        source_channel_cache[cache_key] = {
                            "status": "error", "path": source_path_text,
                            "minimum": "", "maximum": "", "mean": "",
                            "nonzero": "", "total": "",
                        }
                source_stats = source_channel_cache[cache_key]
                source_status = source_stats["status"]
                source_path_text = source_stats["path"]
                source_min = source_stats["minimum"]
                source_max = source_stats["maximum"]
                source_mean = source_stats["mean"]
                source_nonzero = source_stats["nonzero"]
                source_total = source_stats["total"]
                effective_nonzero = (
                    source_status == "decoded" and int(source_max) > 0)
            metallic_rows.append({
                "material_id": material_id, "material": material_name,
                "runtime_source": "texture_override" if metallic_binding != INVALID else "constant",
                "metallic_constant": metallic_constant,
                "constant_used_by_shader": metallic_binding == INVALID,
                "metallic_binding": "" if metallic_binding == INVALID else metallic_binding,
                "metallic_texture": row["metallic_texture"],
                "metallic_texture_id": "" if texture_id == INVALID else texture_id,
                "metallic_channel": row["metallic_channel"],
                "metallic_format": row["metallic_format"],
                "source_texture_path": source_path_text,
                "source_channel_status": source_status,
                "source_channel_min_u8": source_min,
                "source_channel_max_u8": source_max,
                "source_channel_mean_u8": source_mean,
                "source_nonzero_pixels": source_nonzero,
                "source_total_pixels": source_total,
                "effective_nonzero": effective_nonzero,
                "lod0_submesh_count": material_submeshes_lod0[material_id],
                "lod0_triangles": material_triangles_lod0[material_id],
                "meshes": row["meshes"],
            })

    texture_rows = []
    for texture_id, texture in enumerate(scene.textures):
        users = texture_slot_users[texture_id]
        all_materials = set().union(*users.values())
        texture_rows.append({
            "texture_id": texture_id,
            "texture": scene.string(texture["name"], f"texture_{texture_id}"),
            "payload_key": payload_key_by_texture.get(texture_id, ""),
            "width": int(texture["width"]), "height": int(texture["height"]),
            "mip_count": int(texture["mip_count"]),
            "dxgi_format": DXGI_NAMES.get(int(texture["dxgi_format"]), str(int(texture["dxgi_format"]))),
            "payload_bytes": int(texture["data_size"]),
            "payload_valid": bool(texture_valid[texture_id]),
            "material_reference_count": len(all_materials),
            **{f"{slot}_materials": len(users[slot]) for slot in SLOTS},
        })

    assigned_binding_ids = set(binding_reference_count)
    material_texture_ids = {int(scene.bindings[b]["texture"]) for b in assigned_binding_ids
                            if b < len(scene.bindings)}
    base_rgba_pairs = set()
    for material_id in range(len(scene.materials)):
        binding_id = material_slot_binding[(material_id, "base_color")]
        if binding_id != INVALID and binding_id < len(scene.bindings):
            binding = scene.bindings[binding_id]
            if int(binding["channel"]) == 0:
                base_rgba_pairs.add((int(binding["texture"]), int(binding["sampler"])))
    folded_opacity_bindings = 0
    for binding_id in range(len(scene.bindings)):
        if binding_id in assigned_binding_ids:
            continue
        binding = scene.bindings[binding_id]
        if (int(binding["channel"]) == 4 and
                (int(binding["texture"]), int(binding["sampler"])) in base_rgba_pairs):
            folded_opacity_bindings += 1
    empty_slots = [row["slot"] for row in slot_rows if row["assigned_materials"] == 0]
    summary = {
        "scene": scene.path.as_posix(), "texture_file": texture_path.as_posix(),
        "materials": len(scene.materials), "texture_bindings": len(scene.bindings),
        "textures": len(scene.textures), "samplers": scene.sampler_count,
        "material_slot_referenced_bindings": len(assigned_binding_ids),
        "unreferenced_bindings": len(scene.bindings) - len(assigned_binding_ids),
        "folded_opacity_bindings": folded_opacity_bindings,
        "material_slot_referenced_textures": len(material_texture_ids),
        "textures_not_referenced_by_material_slots": len(scene.textures) - len(material_texture_ids),
        "empty_texture_slots": empty_slots,
        "metallic_constant_nonzero_materials": int(np.count_nonzero(np.abs(scene.materials["metallic"]) > 1e-6)),
        "metallic_texture_materials": next(row["assigned_materials"] for row in slot_rows if row["slot"] == "metallic"),
        "metallic_slot_or_constant_materials": len(metallic_rows),
        "metallic_effective_nonzero_materials": sum(
            bool(row["effective_nonzero"]) for row in metallic_rows),
        "metallic_effect_unknown_materials": sum(
            row["source_channel_status"] == "error" for row in metallic_rows),
        "metallic_unique_texture_ids": sorted({
            row["metallic_texture_id"] for row in metallic_rows
            if row["metallic_texture_id"] != ""}),
        "metallic_source_channel_observations": [
            {"texture_id": texture_id, "channel": CHANNEL_NAMES[channel], **stats}
            for (texture_id, channel), stats in sorted(source_channel_cache.items())
        ],
        "unused_materials": sum(material_submeshes_all[i] == 0 for i in range(len(scene.materials))),
        "fjtex_payload_bytes": payload_size,
        "validation": {"errors": errors, "all_passed": not errors},
    }

    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "slot_stats.csv", slot_rows)
    write_csv(output / "constant_stats.csv", constant_rows)
    write_csv(output / "materials.csv", material_rows)
    write_csv(output / "metallic_users.csv", metallic_rows)
    write_csv(output / "texture_usage.csv", texture_rows)
    (output / "summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")

    slot_table = "\n".join(
        f"| {r['slot']} | {r['assigned_materials']} | {r['missing_materials']} | "
        f"{r['unique_textures']} | {r['all_assigned_resolve_to_payload']} | {r['channels']} |"
        for r in slot_rows)
    constant_table = "\n".join(
        f"| {r['constant']} | {r['forward_shader_use']} | {r['default']} | {r['default_count']} | "
        f"{r['non_default_count']} | {r['unique_value_count']} | {r['minimum']} | {r['maximum']} |"
        for r in constant_rows)
    metallic_table = "\n".join(
        f"| {r['material_id']} | {r['material']} | {r['runtime_source']} | "
        f"{r['metallic_constant']:.6g} | {r['metallic_texture'] or '-'} | "
        f"{r['metallic_channel'] or '-'} | {r['source_channel_min_u8']}..{r['source_channel_max_u8']} | "
        f"{r['source_nonzero_pixels']}/{r['source_total_pixels']} | "
        f"{r['effective_nonzero']} | {r['lod0_submesh_count']} | "
        f"{(r['meshes'] or '-').replace(' | ', '<br>')} |"
        for r in metallic_rows) or "| - | 없음 | - | - | - | - | - | - | - | - | - |"
    readme = f"""# FastJungle material audit

입력: `{scene.path.as_posix()}` (`FJSCENE v8`) / `{texture_path.as_posix()}` (`FJTEX v3`)

## 결론

- Material {len(scene.materials)}개, cooked texture binding {len(scene.bindings)}개, texture {len(scene.textures)}개를 전수 검사했습니다.
- material slot에서 참조되는 binding은 {len(assigned_binding_ids)}개이고, cook 후 참조되지 않는 binding은 {len(scene.bindings) - len(assigned_binding_ids)}개입니다.
- 참조되지 않는 binding 중 {folded_opacity_bindings}개는 base-color alpha로 합쳐진 opacity A-channel binding입니다.
- binding → texture → mip → FJTEX payload 범위 검증: **{'PASS' if not errors else 'FAIL'}**.
- 하나도 사용되지 않는 texture slot: **{', '.join(empty_slots) if empty_slots else '없음'}**.
- metallic constant가 0이 아닌 material: **{summary['metallic_constant_nonzero_materials']}개**.
- metallic texture를 쓰는 material: **{summary['metallic_texture_materials']}개**.
- metallic slot 또는 nonzero constant가 있는 material: **{len(metallic_rows)}개**.
- 원본 metallic 채널에 실제 nonzero pixel이 있는 material: **{summary['metallic_effective_nonzero_materials']}개**.
- metallic slot 64개는 모두 `MI_Terrain_X*_Y*`이며 같은 `Metallic-Roughness.png`의 B 채널을 사용합니다.
- 그 원본 B 채널은 min=max=0, nonzero pixel 0/1024이므로 실제 metallic 값은 전부 0입니다.

## Texture slot

| slot | assigned materials | missing materials | unique textures | payload valid | cooked channels |
|---|---:|---:|---:|---|---|
{slot_table}

Shader 의미: base color·opacity·emissive는 상수와 texture를 곱합니다. roughness·metallic은 texture가 있으면 material 상수를 완전히 덮어씁니다. normal은 texture-only입니다.

Material 상수 10개는 모두 CPU에서 GPU material buffer로 복사됩니다. 다만 현재 Forward shader는 IOR·specular·clearcoat·clearcoat roughness를 읽지 않습니다.

## Material constants

`default_count`는 compiler fallback/구조체 기본값과 같은 material 수입니다. texture 연결 시 base color와 emissive는 compiler가 곱셈용 neutral 값 1로 바꿀 수 있습니다.

| constant | Forward shader | default | default count | non-default | unique | min | max |
|---|---|---|---:|---:|---:|---|---|
{constant_table}

## Metallic users

| id | material | runtime source | constant | texture | channel | source min..max | nonzero pixels | effective nonzero | LOD0 submeshes | meshes |
|---:|---|---|---:|---|---|---|---:|---|---:|---|
{metallic_table}

## Raw files

- `analyze_materials.py`: FJSCENE v8/FJTEX v3 분석 재현 스크립트
- `slot_stats.csv`: slot별 실제 binding/payload 통계
- `constant_stats.csv`: material 상수 분포
- `materials.csv`: 187개 material 전체 값과 모든 slot
- `metallic_users.csv`: metallic slot/constant 사용자와 원본 채널 실측값
- `texture_usage.csv`: 242개 cooked texture의 slot별 사용자와 payload
- `summary.json`: 검증 invariant와 집계

## Reproduce

```powershell
python analyze_materials.py --scene assets/cooked/spatial-audit/JungleRuins-spatial-audit.fjscene --output material-audit-2026-08-07
```
"""
    (output / "README.md").write_text(readme, encoding="utf-8")
    return summary, slot_rows, constant_rows, metallic_rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    scene = Scene(args.scene)
    try:
        summary, slots, constants, metallic = analyze(scene, args.output)
        print(json.dumps({
            "summary": summary,
            "slots": [{"slot": r["slot"], "assigned": r["assigned_materials"],
                       "missing": r["missing_materials"]} for r in slots],
            "metallic_user_count": len(metallic),
            "metallic_material_ids": [row["material_id"] for row in metallic],
        }, ensure_ascii=False))
    finally:
        scene.close()


if __name__ == "__main__":
    main()
