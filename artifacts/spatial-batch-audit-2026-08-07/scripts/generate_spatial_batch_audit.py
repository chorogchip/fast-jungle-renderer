#!/usr/bin/env python3
"""Audit FastJungle point batching directly from a cooked v8 scene.

The script intentionally mirrors SceneBatchBuilder.cpp: category-specific XZ
cells, Morton ordering inside each cell, and a hard cluster capacity.  It then
uses transformed mesh AABBs to measure cluster tightness and conservative
frustum-culling waste.  Matplotlib/Pillow are the only non-stdlib dependencies.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import html
import json
import math
import mmap
import os
from pathlib import Path
import shutil
import struct
import tempfile
from dataclasses import dataclass
from typing import Iterable

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import patches
import numpy as np
from PIL import Image


CATEGORY_NAMES = [
    "Anthurium", "Nettle", "ShrubSorrel", "Shrub", "Grass_B", "Grass_A",
    "Pyramid_Grass_B", "Pyramid_Moss", "QueenForest", "RiverForest",
    "RiverSapling", "RiverSeedling",
]
CATEGORY_CELL_SIZES = np.array(
    [128, 128, 128, 128, 128, 128, 32, 32, 128, 128, 64, 64],
    dtype=np.float64,
)
CATEGORY_COLORS = [
    "#ef6c52", "#f49b45", "#c1a94b", "#78ad55", "#29a36a", "#248b77",
    "#5c79cf", "#825ec4", "#ad5ab0", "#d95d87", "#6b7b8c", "#9e795d",
]
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


def explicit_dtype(names, formats, offsets, itemsize):
    return np.dtype({"names": names, "formats": formats, "offsets": offsets,
                     "itemsize": itemsize})


VERTEX_DTYPE = explicit_dtype(["position"], [("<f4", (3,))], [0], 32)
SUBMESH_DTYPE = explicit_dtype(
    ["name", "vertex_offset", "vertex_count", "index_offset", "index_count", "material", "flags"],
    ["<u4"] * 7, list(range(0, 28, 4)), 28)
MESH_LOD_DTYPE = explicit_dtype(
    ["submesh_offset", "submesh_count", "max_deviation", "reserved"],
    ["<u4", "<u4", "<f4", "<u4"], [0, 4, 8, 12], 16)
MESH_DTYPE = explicit_dtype(
    ["name", "lod_offset", "lod_count"], ["<u4"] * 3, [0, 4, 8], 12)
POINT_INSTANCE_DTYPE = explicit_dtype(
    ["position", "orientation", "scale"],
    [("<f4", (3,)), ("<f4", (4,)), ("<f4", (3,))], [0, 12, 28], 40)
POINT_BATCH_DTYPE = explicit_dtype(
    ["mesh", "local_transform", "category", "instance_offset", "instance_count"],
    ["<u4", ("<f4", (16,)), "<u4", "<u4", "<u4"], [0, 4, 68, 72, 76], 80)
STATIC_INSTANCE_DTYPE = explicit_dtype(
    ["name", "mesh", "world_transform"], ["<u4", "<u4", ("<f4", (16,))],
    [0, 4, 8], 72)


class CookedScene:
    def __init__(self, path: Path):
        self.path = path
        self.file = path.open("rb")
        self.mm = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)
        if len(self.mm) < 40:
            raise ValueError("Cooked scene is shorter than its header")
        magic, version, header_size, vertex_size, info_size, payload_size, texture_size = \
            struct.unpack_from("<8sIIIIQQ", self.mm, 0)
        if magic != b"FJSCENE\0" or version != 8:
            raise ValueError(f"Expected FJSCENE v8, got magic={magic!r}, version={version}")
        if header_size != 40 or vertex_size != 32 or info_size != 16:
            raise ValueError("Cooked scene ABI header differs from the audited source")
        if payload_size + header_size != len(self.mm):
            raise ValueError("Cooked scene payload size does not match file size")
        self.texture_payload_size = texture_size
        self.vectors: dict[str, tuple[int, int, int]] = {}
        cursor = header_size
        for name, item_size in VECTOR_SPECS:
            count = struct.unpack_from("<Q", self.mm, cursor)[0]
            cursor += 8
            byte_count = count * item_size
            if cursor + byte_count > len(self.mm):
                raise ValueError(f"Vector {name} exceeds the cooked scene")
            self.vectors[name] = (cursor, count, item_size)
            cursor += byte_count
        self.components_offset = cursor
        self.camera_offset = cursor + 32
        self.environment_offset = self.camera_offset + 104
        self.info_offset = self.environment_offset + 92
        cursor = self.info_offset + 16
        if cursor != len(self.mm):
            raise ValueError(f"Scene schema ended at {cursor}, file ends at {len(self.mm)}")
        strings_off, strings_count, _ = self.vectors["strings"]
        self.strings = self.mm[strings_off:strings_off + strings_count]
        self.vertices = self.array("vertices", VERTEX_DTYPE)
        self.submeshes = self.array("submeshes", SUBMESH_DTYPE)
        self.mesh_lods = self.array("mesh_lods", MESH_LOD_DTYPE)
        self.meshes = self.array("meshes", MESH_DTYPE)
        self.point_instances = self.array("point_instances", POINT_INSTANCE_DTYPE)
        self.point_batches = self.array("point_batches", POINT_BATCH_DTYPE)
        self.static_instances = self.array("static_mesh_instances", STATIC_INSTANCE_DTYPE)

    def array(self, name: str, dtype: np.dtype) -> np.ndarray:
        offset, count, item_size = self.vectors[name]
        if dtype.itemsize != item_size:
            raise ValueError(f"dtype for {name} is {dtype.itemsize}, expected {item_size}")
        return np.ndarray((count,), dtype=dtype, buffer=self.mm, offset=offset)

    def name(self, offset: int, fallback: str) -> str:
        if offset < 0 or offset >= len(self.strings):
            return fallback
        end = self.strings.find(b"\0", offset)
        if end < 0:
            return fallback
        return self.strings[offset:end].decode("utf-8", errors="replace") or fallback

    def authored_camera(self):
        off = self.camera_offset
        name_off = struct.unpack_from("<I", self.mm, off)[0]
        world = np.frombuffer(self.mm, dtype="<f4", count=16, offset=off + 4).reshape(4, 4).copy()
        focal, hap, vap = struct.unpack_from("<fff", self.mm, off + 68)
        near, far = struct.unpack_from("<ff", self.mm, off + 96)
        eye = world[3, :3].astype(np.float64)
        forward = world[2, :3].astype(np.float64)
        if focal > 0 and vap > 0:
            fov = 2.0 * math.atan(0.5 * vap / focal)
        else:
            fov = math.radians(60.0)
        aspect = hap / vap if vap > 0 and hap > 0 else 16.0 / 9.0
        return {
            "name": self.name(name_off, "AuthoredCamera"), "eye": eye,
            "target": eye + forward, "up": world[1, :3].astype(np.float64),
            "fov": fov, "aspect": aspect, "near": max(float(near), 0.01),
            "far": max(float(far), float(near) + 1.0), "kind": "authored",
        }

    def close(self):
        for attr in ("vertices", "submeshes", "mesh_lods", "meshes",
                     "point_instances", "point_batches", "static_instances"):
            setattr(self, attr, None)
        self.mm.close()
        self.file.close()


@dataclass
class MeshInfo:
    name: str
    minimum: np.ndarray
    maximum: np.ndarray
    lod0_triangles: int
    lod0_submeshes: int


@dataclass
class ClusterSet:
    order: np.ndarray
    starts: np.ndarray
    counts: np.ndarray
    batch_ids: np.ndarray
    cell_x: np.ndarray
    cell_z: np.ndarray
    minimum: np.ndarray
    maximum: np.ndarray
    cap: int
    multiplier: float


def morton_spread(value: np.ndarray) -> np.ndarray:
    value = value.astype(np.uint32, copy=True) & np.uint32(0x0000FFFF)
    value = (value | (value << np.uint32(8))) & np.uint32(0x00FF00FF)
    value = (value | (value << np.uint32(4))) & np.uint32(0x0F0F0F0F)
    value = (value | (value << np.uint32(2))) & np.uint32(0x33333333)
    value = (value | (value << np.uint32(1))) & np.uint32(0x55555555)
    return value


def mesh_information(scene: CookedScene) -> list[MeshInfo]:
    infos: list[MeshInfo] = []
    positions = scene.vertices["position"]
    for mesh_id, mesh in enumerate(scene.meshes):
        lod = scene.mesh_lods[int(mesh["lod_offset"])]
        minimum = np.full(3, np.inf, dtype=np.float64)
        maximum = np.full(3, -np.inf, dtype=np.float64)
        triangles = 0
        for local in range(int(lod["submesh_count"])):
            sub = scene.submeshes[int(lod["submesh_offset"]) + local]
            begin = int(sub["vertex_offset"])
            end = begin + int(sub["vertex_count"])
            if end > begin:
                p = positions[begin:end]
                minimum = np.minimum(minimum, np.min(p, axis=0))
                maximum = np.maximum(maximum, np.max(p, axis=0))
            triangles += int(sub["index_count"]) // 3
        if not np.all(np.isfinite(minimum)):
            minimum[:] = 0
            maximum[:] = 0
        infos.append(MeshInfo(
            scene.name(int(mesh["name"]), f"mesh_{mesh_id}"), minimum, maximum,
            triangles, int(lod["submesh_count"])))
    return infos


def quaternion_matrix(q: np.ndarray) -> np.ndarray:
    x, y, z, w = (q[:, i] for i in range(4))
    result = np.empty((len(q), 3, 3), dtype=np.float32)
    result[:, 0, 0] = 1 - 2 * (y * y + z * z)
    result[:, 0, 1] = 2 * (x * y + z * w)
    result[:, 0, 2] = 2 * (x * z - y * w)
    result[:, 1, 0] = 2 * (x * y - z * w)
    result[:, 1, 1] = 1 - 2 * (x * x + z * z)
    result[:, 1, 2] = 2 * (y * z + x * w)
    result[:, 2, 0] = 2 * (x * z + y * w)
    result[:, 2, 1] = 2 * (y * z - x * w)
    result[:, 2, 2] = 1 - 2 * (x * x + y * y)
    return result


def build_instance_bounds(scene: CookedScene, meshes: list[MeshInfo], output_path: Path):
    count = len(scene.point_instances)
    bounds = np.memmap(output_path, dtype="<f4", mode="w+", shape=(count, 6))
    covered = np.zeros(count, dtype=np.bool_)
    finite = True
    for batch_id, batch in enumerate(scene.point_batches):
        offset = int(batch["instance_offset"])
        amount = int(batch["instance_count"])
        end = offset + amount
        if offset < 0 or end > count or np.any(covered[offset:end]):
            raise ValueError(f"PointBatch {batch_id} has overlapping or invalid instance range")
        covered[offset:end] = True
        mesh = meshes[int(batch["mesh"])]
        center = ((mesh.minimum + mesh.maximum) * 0.5).astype(np.float32)
        extent = ((mesh.maximum - mesh.minimum) * 0.5).astype(np.float32)
        batch_matrix = np.asarray(batch["local_transform"], dtype=np.float32).reshape(4, 4)
        for chunk_begin in range(offset, end, 250_000):
            chunk_end = min(chunk_begin + 250_000, end)
            instances = scene.point_instances[chunk_begin:chunk_end]
            q = np.asarray(instances["orientation"], dtype=np.float32)
            rotation = quaternion_matrix(q)
            scale_rotation = np.asarray(instances["scale"], dtype=np.float32)[:, :, None] * rotation
            world3 = np.einsum("ij,njk->nik", batch_matrix[:3, :3], scale_rotation,
                               optimize=True).astype(np.float32)
            translation = np.einsum("i,nij->nj", batch_matrix[3, :3], scale_rotation,
                                    optimize=True)
            translation += np.asarray(instances["position"], dtype=np.float32)
            world_center = np.einsum("i,nij->nj", center, world3, optimize=True) + translation
            world_extent = np.einsum("i,nij->nj", extent, np.abs(world3), optimize=True)
            bounds[chunk_begin:chunk_end, :3] = world_center - world_extent
            bounds[chunk_begin:chunk_end, 3:] = world_center + world_extent
            finite &= bool(np.all(np.isfinite(bounds[chunk_begin:chunk_end])))
    if not np.all(covered):
        missing = int(np.count_nonzero(~covered))
        raise ValueError(f"{missing} point instances are not owned by any PointBatch")
    bounds.flush()
    return bounds, finite


def batch_spatial_order(scene: CookedScene, batch_id: int, multiplier: float, cap: int):
    batch = scene.point_batches[batch_id]
    offset = int(batch["instance_offset"])
    count = int(batch["instance_count"])
    category = int(batch["category"])
    if category < 0 or category >= len(CATEGORY_NAMES):
        raise ValueError(f"PointBatch {batch_id} has invalid category {category}")
    source = np.arange(offset, offset + count, dtype=np.uint32)
    p = scene.point_instances[offset:offset + count]["position"]
    cell_size = float(CATEGORY_CELL_SIZES[category] * multiplier)
    x = np.asarray(p[:, 0], dtype=np.float64)
    z = np.asarray(p[:, 2], dtype=np.float64)
    cell_x = np.floor(x / cell_size).astype(np.int32)
    cell_z = np.floor(z / cell_size).astype(np.int32)
    nx = np.clip((x - cell_x.astype(np.float64) * cell_size) / cell_size, 0.0, 1.0)
    nz = np.clip((z - cell_z.astype(np.float64) * cell_size) / cell_size, 0.0, 1.0)
    qx = (nx * 65535.0 + 0.5).astype(np.uint32)
    qz = (nz * 65535.0 + 0.5).astype(np.uint32)
    morton = morton_spread(qx) | (morton_spread(qz) << np.uint32(1))
    permutation = np.lexsort((source, morton, cell_z, cell_x))
    order = source[permutation]
    sx = cell_x[permutation]
    sz = cell_z[permutation]
    if count == 0:
        return order, np.empty(0, np.int64), np.empty(0, np.uint32), sx, sz
    cell_starts = np.r_[0, np.flatnonzero((sx[1:] != sx[:-1]) | (sz[1:] != sz[:-1])) + 1]
    cell_ends = np.r_[cell_starts[1:], count]
    starts = []
    for begin, end in zip(cell_starts.tolist(), cell_ends.tolist()):
        starts.extend(range(begin, end, cap))
    starts = np.asarray(starts, dtype=np.int64)
    ends = np.r_[starts[1:], count]
    counts = (ends - starts).astype(np.uint32)
    return order, starts, counts, sx[starts], sz[starts]


def build_clusters(scene: CookedScene, instance_bounds: np.ndarray,
                   multiplier: float, cap: int) -> ClusterSet:
    order_parts, start_parts, count_parts, batch_parts, cx_parts, cz_parts = [], [], [], [], [], []
    order_cursor = 0
    for batch_id in range(len(scene.point_batches)):
        order, starts, counts, cell_x, cell_z = batch_spatial_order(scene, batch_id, multiplier, cap)
        order_parts.append(order)
        start_parts.append(starts + order_cursor)
        count_parts.append(counts)
        batch_parts.append(np.full(len(starts), batch_id, dtype=np.uint32))
        cx_parts.append(cell_x.astype(np.int32, copy=False))
        cz_parts.append(cell_z.astype(np.int32, copy=False))
        order_cursor += len(order)
    order = np.concatenate(order_parts) if order_parts else np.empty(0, np.uint32)
    starts = np.concatenate(start_parts) if start_parts else np.empty(0, np.int64)
    counts = np.concatenate(count_parts) if count_parts else np.empty(0, np.uint32)
    batch_ids = np.concatenate(batch_parts) if batch_parts else np.empty(0, np.uint32)
    cell_x = np.concatenate(cx_parts) if cx_parts else np.empty(0, np.int32)
    cell_z = np.concatenate(cz_parts) if cz_parts else np.empty(0, np.int32)
    ordered_bounds = np.asarray(instance_bounds[order])
    minimum = np.minimum.reduceat(ordered_bounds[:, :3], starts, axis=0).astype(np.float32)
    maximum = np.maximum.reduceat(ordered_bounds[:, 3:], starts, axis=0).astype(np.float32)
    del ordered_bounds
    return ClusterSet(order, starts, counts, batch_ids, cell_x, cell_z,
                      minimum, maximum, cap, multiplier)


def look_at_planes(eye, target, up, fov, aspect, near, far):
    eye = np.asarray(eye, dtype=np.float64)
    zaxis = np.asarray(target, dtype=np.float64) - eye
    zaxis /= np.linalg.norm(zaxis)
    up = np.asarray(up, dtype=np.float64)
    xaxis = np.cross(up, zaxis)
    if np.linalg.norm(xaxis) < 1e-8:
        up = np.array([0.0, 0.0, 1.0])
        xaxis = np.cross(up, zaxis)
    xaxis /= np.linalg.norm(xaxis)
    yaxis = np.cross(zaxis, xaxis)
    view = np.array([
        [xaxis[0], yaxis[0], zaxis[0], 0],
        [xaxis[1], yaxis[1], zaxis[1], 0],
        [xaxis[2], yaxis[2], zaxis[2], 0],
        [-np.dot(xaxis, eye), -np.dot(yaxis, eye), -np.dot(zaxis, eye), 1],
    ], dtype=np.float64)
    ys = 1.0 / math.tan(fov * 0.5)
    xs = ys / aspect
    projection = np.array([
        [xs, 0, 0, 0], [0, ys, 0, 0],
        [0, 0, far / (far - near), 1],
        [0, 0, -near * far / (far - near), 0],
    ], dtype=np.float64)
    m = view @ projection
    planes = np.array([
        m[:, 0] + m[:, 3], -m[:, 0] + m[:, 3],
        m[:, 1] + m[:, 3], -m[:, 1] + m[:, 3],
        m[:, 2], -m[:, 2] + m[:, 3],
    ])
    planes /= np.linalg.norm(planes[:, :3], axis=1)[:, None]
    return planes


def intersects(bounds: np.ndarray, planes: np.ndarray) -> np.ndarray:
    result = np.ones(len(bounds), dtype=np.bool_)
    minimum, maximum = bounds[:, :3], bounds[:, 3:]
    for plane in planes:
        positive = np.where(plane[:3] >= 0, maximum, minimum)
        result &= positive @ plane[:3] + plane[3] >= 0
    return result


def camera_suite(scene: CookedScene, world_min: np.ndarray, world_max: np.ndarray):
    center = (world_min + world_max) * 0.5
    extent = world_max - world_min
    radius = max(float(np.linalg.norm(extent) * 0.5), 1.0)
    cameras = []
    for elevation_index, elevation in enumerate((0.14, 0.38)):
        distance = radius * (1.25 if elevation_index == 0 else 1.05)
        for azimuth in range(0, 360, 30):
            a = math.radians(azimuth)
            horizontal = distance * math.cos(elevation)
            eye = center + np.array([
                math.cos(a) * horizontal, math.sin(elevation) * distance,
                math.sin(a) * horizontal])
            cameras.append({"name": f"orbit_e{elevation_index}_{azimuth:03d}", "kind": "orbit",
                            "eye": eye, "target": center, "up": np.array([0., 1., 0.]),
                            "fov": math.radians(60), "aspect": 16 / 9,
                            "near": max(radius * 0.0002, 0.1), "far": radius * 4})
    for grid_id, (fx, fz, heading) in enumerate([
        (-.25, -.25, 35), (-.25, -.25, 215), (.25, -.25, 145), (.25, -.25, 325),
        (-.25, .25, 55), (-.25, .25, 235), (.25, .25, 125), (.25, .25, 305),
    ]):
        eye = np.array([center[0] + extent[0] * fx,
                        world_min[1] + max(extent[1] * .12, 2.0),
                        center[2] + extent[2] * fz])
        h = math.radians(heading)
        target = eye + np.array([math.sin(h), 0.02, math.cos(h)]) * radius
        cameras.append({"name": f"interior_{grid_id:02d}", "kind": "interior",
                        "eye": eye, "target": target, "up": np.array([0., 1., 0.]),
                        "fov": math.radians(75), "aspect": 16 / 9,
                        "near": .1, "far": radius * 3})
    for top_id, (fx, fz) in enumerate(((-.2, -.2), (.2, -.2), (-.2, .2), (.2, .2))):
        target = np.array([center[0] + extent[0] * fx, center[1], center[2] + extent[2] * fz])
        eye = target + np.array([0., radius * 1.1, 0.])
        cameras.append({"name": f"top_{top_id:02d}", "kind": "top",
                        "eye": eye, "target": target, "up": np.array([0., 0., 1.]),
                        "fov": math.radians(55), "aspect": 1.0,
                        "near": .1, "far": radius * 3})
    try:
        authored = scene.authored_camera()
        authored["far"] = max(authored["far"], radius * 3)
        cameras.append(authored)
    except Exception:
        pass
    for camera in cameras:
        camera["planes"] = look_at_planes(camera["eye"], camera["target"], camera["up"],
                                            camera["fov"], camera["aspect"],
                                            camera["near"], camera["far"])
    return cameras


def polygon_area(points: np.ndarray) -> float:
    points = np.unique(points, axis=0)
    if len(points) < 3:
        return 0.0
    points = points[np.lexsort((points[:, 1], points[:, 0]))]
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower = []
    for p in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    upper = []
    for p in points[::-1]:
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    hull = np.asarray(lower[:-1] + upper[:-1])
    return float(abs(np.dot(hull[:, 0], np.roll(hull[:, 1], -1)) -
                     np.dot(hull[:, 1], np.roll(hull[:, 0], -1))) * 0.5)


def cluster_shape_metrics(scene: CookedScene, clusters: ClusterSet,
                          instance_bounds: np.ndarray):
    n = len(clusters.counts)
    fill = np.empty(n, np.float32)
    anisotropy = np.empty(n, np.float32)
    rms_radius = np.empty(n, np.float32)
    p95_radius = np.empty(n, np.float32)
    point_area = np.empty(n, np.float32)
    instance_area_pressure = np.empty(n, np.float32)
    positions = scene.point_instances["position"]
    for cluster_id, (start, count) in enumerate(zip(clusters.starts, clusters.counts)):
        ids = clusters.order[int(start):int(start) + int(count)]
        points = np.asarray(positions[ids][:, [0, 2]], dtype=np.float64)
        pmin, pmax = np.min(points, axis=0), np.max(points, axis=0)
        area = float(np.prod(np.maximum(pmax - pmin, 0)))
        point_area[cluster_id] = area
        hull = polygon_area(points)
        fill[cluster_id] = hull / area if area > 1e-9 else 1.0
        centered = points - np.mean(points, axis=0)
        distances = np.linalg.norm(centered, axis=1)
        rms_radius[cluster_id] = math.sqrt(float(np.mean(distances * distances)))
        p95_radius[cluster_id] = float(np.percentile(distances, 95))
        if len(points) > 1:
            cov = centered.T @ centered / len(points)
            eigen = np.linalg.eigvalsh(cov)
            anisotropy[cluster_id] = float(eigen[-1] / max(eigen[0], 1e-8))
        else:
            anisotropy[cluster_id] = 1.0
        b = np.asarray(instance_bounds[ids])
        individual_area = np.maximum(b[:, 3] - b[:, 0], 0) * np.maximum(b[:, 5] - b[:, 2], 0)
        cluster_area = max(float((clusters.maximum[cluster_id, 0] - clusters.minimum[cluster_id, 0]) *
                                 (clusters.maximum[cluster_id, 2] - clusters.minimum[cluster_id, 2])), 1e-8)
        instance_area_pressure[cluster_id] = float(np.sum(individual_area) / cluster_area)
    return {
        "position_fill_ratio": fill, "anisotropy": anisotropy,
        "rms_radius": rms_radius, "p95_radius": p95_radius,
        "position_aabb_area": point_area,
        "instance_area_pressure": instance_area_pressure,
    }


def evaluate_cameras(scene: CookedScene, meshes: list[MeshInfo], clusters: ClusterSet,
                     instance_bounds: np.ndarray, cameras: list[dict], visibility_path: Path):
    shape = (len(cameras), len(instance_bounds))
    visibility = np.memmap(visibility_path, dtype=np.bool_, mode="w+", shape=shape)
    cluster_bounds = np.c_[clusters.minimum, clusters.maximum]
    accepted_count = np.zeros(len(clusters.counts), np.uint32)
    false_count = np.zeros(len(clusters.counts), np.uint32)
    candidate_sum = np.zeros(len(clusters.counts), np.uint64)
    actual_sum = np.zeros(len(clusters.counts), np.uint64)
    camera_rows, category_rows = [], []
    batch_categories = scene.point_batches["category"].astype(np.int64)
    cluster_categories = batch_categories[clusters.batch_ids]
    cluster_triangles = np.array([meshes[int(scene.point_batches[int(b)]["mesh"])].lod0_triangles
                                  for b in clusters.batch_ids], dtype=np.uint64)
    cluster_submeshes = np.array([meshes[int(scene.point_batches[int(b)]["mesh"])].lod0_submeshes
                                  for b in clusters.batch_ids], dtype=np.uint64)
    missed_total = 0
    for camera_id, camera in enumerate(cameras):
        ivis = intersects(np.asarray(instance_bounds), camera["planes"])
        visibility[camera_id] = ivis
        cvis = intersects(cluster_bounds, camera["planes"])
        actual = np.add.reduceat(ivis[clusters.order].astype(np.uint16), clusters.starts).astype(np.uint32)
        false = cvis & (actual == 0)
        rejected_actual = int(np.sum(actual[~cvis], dtype=np.uint64))
        missed_total += rejected_actual
        accepted_count += cvis
        false_count += false
        candidate_sum += np.where(cvis, clusters.counts, 0)
        actual_sum += np.where(cvis, actual, 0)
        candidate = int(np.sum(clusters.counts[cvis], dtype=np.uint64))
        actual_candidate = int(np.sum(actual[cvis], dtype=np.uint64))
        actual_global = int(np.count_nonzero(ivis))
        visible_draws = int(np.sum(cluster_submeshes[cvis], dtype=np.uint64))
        ideal_draws = int(np.sum(cluster_submeshes[cvis & (actual > 0)], dtype=np.uint64))
        candidate_triangles = int(np.sum(clusters.counts[cvis].astype(np.uint64) * cluster_triangles[cvis]))
        actual_triangles = int(np.sum(actual[cvis].astype(np.uint64) * cluster_triangles[cvis]))
        camera_rows.append({
            "camera_id": camera_id, "camera": camera["name"], "kind": camera["kind"],
            "visible_clusters": int(np.count_nonzero(cvis)),
            "false_positive_clusters": int(np.count_nonzero(false)),
            "candidate_instances": candidate, "actual_visible_instances": actual_global,
            "actual_in_candidate_clusters": actual_candidate,
            "instance_waste_ratio": (candidate - actual_candidate) / candidate if candidate else 0,
            "visible_lod0_draws": visible_draws, "ideal_lod0_draws": ideal_draws,
            "candidate_lod0_triangles": candidate_triangles,
            "actual_lod0_triangles": actual_triangles,
            "rejected_actual_instances": rejected_actual,
        })
        for category in range(len(CATEGORY_NAMES)):
            mask = cluster_categories == category
            selected = cvis & mask
            candidate_cat = int(np.sum(clusters.counts[selected], dtype=np.uint64))
            actual_cat = int(np.sum(actual[selected], dtype=np.uint64))
            category_rows.append({
                "camera_id": camera_id, "camera": camera["name"], "kind": camera["kind"],
                "category": CATEGORY_NAMES[category],
                "visible_clusters": int(np.count_nonzero(selected)),
                "false_positive_clusters": int(np.count_nonzero(false & mask)),
                "candidate_instances": candidate_cat, "actual_visible_instances": actual_cat,
                "instance_waste_ratio": (candidate_cat - actual_cat) / candidate_cat if candidate_cat else 0,
            })
    visibility.flush()
    return camera_rows, category_rows, visibility, {
        "accepted_count": accepted_count, "false_count": false_count,
        "candidate_sum": candidate_sum, "actual_sum": actual_sum,
        "missed_total": missed_total,
    }


def evaluate_sweep(scene: CookedScene, meshes: list[MeshInfo], instance_bounds: np.ndarray,
                   production: ClusterSet, cameras: list[dict], visibility: np.ndarray):
    rows = []
    sweep_camera_count = min(24, len(cameras))
    configurations = [(m, c) for m in (0.5, 1.0, 2.0) for c in (128, 256, 512)]
    for multiplier, cap in configurations:
        clusters = production if (multiplier == 1.0 and cap == 256) else \
            build_clusters(scene, instance_bounds, multiplier, cap)
        cluster_bounds = np.c_[clusters.minimum, clusters.maximum]
        batch_ids = clusters.batch_ids.astype(np.int64)
        submeshes = np.array([meshes[int(scene.point_batches[b]["mesh"])].lod0_submeshes
                              for b in batch_ids], dtype=np.uint64)
        candidate_total = actual_total = false_total = accepted_total = draws_total = ideal_draws_total = 0
        for camera_id in range(sweep_camera_count):
            cvis = intersects(cluster_bounds, cameras[camera_id]["planes"])
            actual = np.add.reduceat(visibility[camera_id][clusters.order].astype(np.uint16),
                                     clusters.starts).astype(np.uint32)
            candidate_total += int(np.sum(clusters.counts[cvis], dtype=np.uint64))
            actual_total += int(np.sum(actual[cvis], dtype=np.uint64))
            false_total += int(np.count_nonzero(cvis & (actual == 0)))
            accepted_total += int(np.count_nonzero(cvis))
            draws_total += int(np.sum(submeshes[cvis], dtype=np.uint64))
            ideal_draws_total += int(np.sum(submeshes[cvis & (actual > 0)], dtype=np.uint64))
        size = clusters.maximum - clusters.minimum
        rows.append({
            "cell_multiplier": multiplier, "cluster_cap": cap,
            "cluster_count": len(clusters.counts),
            "mean_occupancy": float(np.mean(clusters.counts / cap)),
            "p10_occupancy": float(np.percentile(clusters.counts / cap, 10)),
            "p95_diagonal_m": float(np.percentile(np.linalg.norm(size, axis=1), 95)),
            "camera_samples": sweep_camera_count,
            "candidate_instances": candidate_total, "actual_visible_instances": actual_total,
            "instance_waste_ratio": (candidate_total - actual_total) / candidate_total if candidate_total else 0,
            "accepted_clusters": accepted_total, "false_positive_clusters": false_total,
            "false_positive_cluster_ratio": false_total / accepted_total if accepted_total else 0,
            "average_visible_lod0_draws": draws_total / sweep_camera_count,
            "average_ideal_lod0_draws": ideal_draws_total / sweep_camera_count,
            "is_current": multiplier == 1.0 and cap == 256,
        })
        if clusters is not production:
            del clusters
    current = next(r for r in rows if r["is_current"])
    for row in rows:
        draw_ratio = row["average_visible_lod0_draws"] / max(current["average_visible_lod0_draws"], 1)
        row["balanced_score"] = row["instance_waste_ratio"] + 0.15 * draw_ratio
    return rows


def write_csv(path: Path, rows: list[dict]):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    with path.open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        while chunk := file.read(8 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def percentile(values, p):
    return float(np.percentile(values, p)) if len(values) else 0.0


def build_rows(scene: CookedScene, meshes: list[MeshInfo], clusters: ClusterSet,
               shapes: dict, culling: dict):
    size = clusters.maximum - clusters.minimum
    area = size[:, 0] * size[:, 2]
    volume = size[:, 0] * size[:, 1] * size[:, 2]
    diagonal = np.linalg.norm(size, axis=1)
    utilization = clusters.counts / clusters.cap
    waste = np.divide(culling["candidate_sum"] - culling["actual_sum"],
                      culling["candidate_sum"], out=np.zeros(len(clusters.counts), np.float64),
                      where=culling["candidate_sum"] != 0)
    false_rate = np.divide(culling["false_count"], culling["accepted_count"],
                           out=np.zeros(len(clusters.counts), np.float64),
                           where=culling["accepted_count"] != 0)
    cluster_rows = []
    for i in range(len(clusters.counts)):
        batch_id = int(clusters.batch_ids[i])
        batch = scene.point_batches[batch_id]
        category = int(batch["category"])
        mesh = meshes[int(batch["mesh"])]
        cluster_rows.append({
            "cluster_id": i, "batch_id": batch_id, "category": CATEGORY_NAMES[category],
            "mesh": mesh.name, "cell_x": int(clusters.cell_x[i]), "cell_z": int(clusters.cell_z[i]),
            "instance_count": int(clusters.counts[i]), "capacity_utilization": float(utilization[i]),
            "min_x": float(clusters.minimum[i, 0]), "min_y": float(clusters.minimum[i, 1]),
            "min_z": float(clusters.minimum[i, 2]), "max_x": float(clusters.maximum[i, 0]),
            "max_y": float(clusters.maximum[i, 1]), "max_z": float(clusters.maximum[i, 2]),
            "extent_x_m": float(size[i, 0]), "extent_y_m": float(size[i, 1]),
            "extent_z_m": float(size[i, 2]), "xz_area_m2": float(area[i]),
            "volume_m3": float(volume[i]), "diagonal_m": float(diagonal[i]),
            "position_fill_ratio": float(shapes["position_fill_ratio"][i]),
            "anisotropy": float(shapes["anisotropy"][i]),
            "rms_radius_m": float(shapes["rms_radius"][i]),
            "p95_radius_m": float(shapes["p95_radius"][i]),
            "instance_area_pressure": float(shapes["instance_area_pressure"][i]),
            "accepted_camera_count": int(culling["accepted_count"][i]),
            "false_positive_camera_count": int(culling["false_count"][i]),
            "false_positive_rate": float(false_rate[i]),
            "candidate_instances_sum": int(culling["candidate_sum"][i]),
            "actual_instances_sum": int(culling["actual_sum"][i]),
            "culling_waste_ratio": float(waste[i]),
        })
    batch_rows = []
    for batch_id, batch in enumerate(scene.point_batches):
        mask = clusters.batch_ids == batch_id
        indices = np.flatnonzero(mask)
        category = int(batch["category"])
        mesh = meshes[int(batch["mesh"])]
        candidate = int(np.sum(culling["candidate_sum"][mask], dtype=np.uint64))
        actual = int(np.sum(culling["actual_sum"][mask], dtype=np.uint64))
        batch_rows.append({
            "batch_id": batch_id, "category": CATEGORY_NAMES[category], "mesh": mesh.name,
            "instance_offset": int(batch["instance_offset"]),
            "instance_count": int(batch["instance_count"]), "cluster_count": len(indices),
            "mean_capacity_utilization": float(np.mean(utilization[mask])),
            "p10_capacity_utilization": percentile(utilization[mask], 10),
            "median_cluster_diagonal_m": percentile(diagonal[mask], 50),
            "p95_cluster_diagonal_m": percentile(diagonal[mask], 95),
            "median_position_fill_ratio": percentile(shapes["position_fill_ratio"][mask], 50),
            "candidate_instances_sum": candidate, "actual_instances_sum": actual,
            "culling_waste_ratio": (candidate - actual) / candidate if candidate else 0,
            "lod0_triangles_per_instance": mesh.lod0_triangles,
            "lod0_submeshes_per_cluster": mesh.lod0_submeshes,
        })
    category_rows = []
    batch_categories = scene.point_batches["category"].astype(np.int64)
    cluster_categories = batch_categories[clusters.batch_ids]
    for category, name in enumerate(CATEGORY_NAMES):
        cmask = cluster_categories == category
        bmask = batch_categories == category
        candidate = int(np.sum(culling["candidate_sum"][cmask], dtype=np.uint64))
        actual = int(np.sum(culling["actual_sum"][cmask], dtype=np.uint64))
        accepted = int(np.sum(culling["accepted_count"][cmask], dtype=np.uint64))
        false = int(np.sum(culling["false_count"][cmask], dtype=np.uint64))
        category_rows.append({
            "category": name, "source_batches": int(np.count_nonzero(bmask)),
            "instances": int(np.sum(scene.point_batches["instance_count"][bmask], dtype=np.uint64)),
            "clusters": int(np.count_nonzero(cmask)),
            "mean_capacity_utilization": float(np.mean(utilization[cmask])) if np.any(cmask) else 0,
            "median_cluster_diagonal_m": percentile(diagonal[cmask], 50),
            "p95_cluster_diagonal_m": percentile(diagonal[cmask], 95),
            "median_position_fill_ratio": percentile(shapes["position_fill_ratio"][cmask], 50),
            "candidate_instances_sum": candidate, "actual_instances_sum": actual,
            "culling_waste_ratio": (candidate - actual) / candidate if candidate else 0,
            "false_positive_cluster_ratio": false / accepted if accepted else 0,
        })
    return cluster_rows, batch_rows, category_rows


def configure_plot():
    plt.rcParams.update({
        "figure.facecolor": "#f6f3eb", "axes.facecolor": "#fffdf8",
        "axes.edgecolor": "#c7c0b4", "axes.labelcolor": "#34312d",
        "text.color": "#34312d", "xtick.color": "#5e5952", "ytick.color": "#5e5952",
        "grid.color": "#ded8cd", "grid.alpha": .65, "font.size": 10,
    })


def save_figure(fig, path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def make_charts(scene: CookedScene, clusters: ClusterSet, cluster_rows, category_rows,
                camera_rows, sweep_rows, output: Path):
    configure_plot()
    charts = output / "charts"
    positions = scene.point_instances["position"]
    stride = max(1, len(positions) // 500_000)
    sample = np.asarray(positions[::stride])
    fig, ax = plt.subplots(figsize=(12, 8))
    hb = ax.hexbin(sample[:, 0], sample[:, 2], gridsize=220, bins="log", mincnt=1, cmap="viridis")
    fig.colorbar(hb, ax=ax, label="sampled instance density (log)")
    ax.set(title="World-space point density (XZ)", xlabel="X (m)", ylabel="Z (m)", aspect="equal")
    save_figure(fig, charts / "world_density.png")

    fig, ax = plt.subplots(figsize=(11, 6))
    values = [r["capacity_utilization"] for r in cluster_rows]
    ax.hist(values, bins=np.linspace(0, 1, 41), color="#2d8a6e", edgecolor="white")
    ax.axvline(np.mean(values), color="#c65342", lw=2, label=f"mean {np.mean(values):.2f}")
    ax.set(title="Production cluster capacity utilization", xlabel="instance count / 256", ylabel="clusters")
    ax.grid(axis="y"); ax.legend()
    save_figure(fig, charts / "cluster_occupancy.png")

    rng = np.random.default_rng(20260807)
    ids = rng.choice(len(cluster_rows), min(15000, len(cluster_rows)), replace=False)
    x = np.array([cluster_rows[i]["xz_area_m2"] for i in ids])
    y = np.array([cluster_rows[i]["culling_waste_ratio"] for i in ids])
    c = np.array([cluster_rows[i]["capacity_utilization"] for i in ids])
    fig, ax = plt.subplots(figsize=(11, 7))
    sc = ax.scatter(x, y, c=c, s=10, alpha=.45, cmap="plasma", vmin=0, vmax=1)
    fig.colorbar(sc, ax=ax, label="capacity utilization")
    ax.set_xscale("log"); ax.set_ylim(-.02, 1.02); ax.grid(True)
    ax.set(title="Cluster footprint vs conservative culling waste", xlabel="cluster XZ AABB area (m², log)",
           ylabel="candidate-instance waste ratio")
    save_figure(fig, charts / "area_vs_culling_waste.png")

    names = [r["category"] for r in category_rows]
    waste = [r["culling_waste_ratio"] for r in category_rows]
    occupancy = [r["mean_capacity_utilization"] for r in category_rows]
    fig, (ax_waste, ax_occupancy) = plt.subplots(
        1, 2, figsize=(13, 7), sharey=True,
        gridspec_kw={"width_ratios": [1.0, 1.25], "wspace": .08})
    yloc = np.arange(len(names))
    ax_waste.barh(yloc, waste, color="#d36c55")
    ax_occupancy.barh(yloc, occupancy, color="#3b9b79")
    ax_waste.set_yticks(yloc, names); ax_waste.invert_yaxis()
    ax_waste.set_xlim(0, max(max(waste) * 1.2, .005)); ax_occupancy.set_xlim(0, 1)
    ax_waste.grid(axis="x"); ax_occupancy.grid(axis="x")
    ax_waste.set(title="Culling waste", xlabel="candidate-instance waste ratio")
    ax_occupancy.set(title="Capacity utilization", xlabel="mean instance count / 256")
    fig.suptitle("Category-level batching efficiency")
    save_figure(fig, charts / "category_efficiency.png")

    fig, ax = plt.subplots(figsize=(12, 6))
    x = np.arange(len(camera_rows))
    waste = np.array([r["instance_waste_ratio"] for r in camera_rows])
    false = np.array([r["false_positive_clusters"] / max(r["visible_clusters"], 1) for r in camera_rows])
    ax.plot(x, waste, marker="o", ms=3, label="instance waste")
    ax.plot(x, false, marker=".", label="false-positive clusters")
    for kind in sorted({r["kind"] for r in camera_rows}):
        ids_kind = [i for i, r in enumerate(camera_rows) if r["kind"] == kind]
        if ids_kind:
            ax.axvspan(min(ids_kind)-.5, max(ids_kind)+.5, alpha=.05, label=f"{kind} samples")
    ax.set_ylim(0, 1); ax.set(title="Culling efficiency across deterministic cameras",
                              xlabel="camera sample", ylabel="ratio")
    ax.grid(True); ax.legend(ncol=3, fontsize=8)
    save_figure(fig, charts / "camera_culling.png")

    fig, ax = plt.subplots(figsize=(10, 7))
    current = next(r for r in sweep_rows if r["is_current"])
    for row in sweep_rows:
        is_current = row["is_current"]
        ax.scatter(row["average_visible_lod0_draws"], row["instance_waste_ratio"],
                   s=130 if is_current else 75, marker="*" if is_current else "o",
                   color="#c65342" if is_current else "#357c69", zorder=4)
        offsets = {
            (0.5, 128): (7, 9), (0.5, 256): (-18, -18), (0.5, 512): (-42, 11),
            (1.0, 128): (7, 8), (1.0, 256): (8, -18), (1.0, 512): (-42, 10),
            (2.0, 128): (7, 8), (2.0, 256): (7, 8), (2.0, 512): (7, 8),
        }
        ax.annotate(f"{row['cell_multiplier']:g}x/{row['cluster_cap']}",
                    (row["average_visible_lod0_draws"], row["instance_waste_ratio"]),
                    xytext=offsets[(row["cell_multiplier"], row["cluster_cap"])],
                    textcoords="offset points", fontsize=8)
    ax.axvline(current["average_visible_lod0_draws"], color="#c65342", ls="--", alpha=.4)
    ax.axhline(current["instance_waste_ratio"], color="#c65342", ls="--", alpha=.4)
    ax.grid(True); ax.set(title="Cell size / cluster cap trade-off (24 orbit cameras)",
                          xlabel="average visible LOD0 draw calls", ylabel="instance culling waste")
    save_figure(fig, charts / "configuration_pareto.png")

    fig, ax = plt.subplots(figsize=(11, 6))
    current_count = next(r["cluster_count"] for r in sweep_rows if r["is_current"])
    labels = [f"{r['cell_multiplier']:g}x\n{r['cluster_cap']}" for r in sweep_rows]
    counts = [r["cluster_count"] for r in sweep_rows]
    colors = ["#c65342" if r["is_current"] else "#4d927a" for r in sweep_rows]
    ax.bar(labels, counts, color=colors); ax.axhline(current_count, color="#c65342", ls="--", alpha=.5)
    ax.set(title="Cluster count by spatial configuration", xlabel="cell multiplier / cap", ylabel="clusters")
    ax.grid(axis="y")
    save_figure(fig, charts / "configuration_cluster_count.png")


def make_cluster_details(scene: CookedScene, clusters: ClusterSet, cluster_rows, output: Path):
    details = output / "details"
    details.mkdir(parents=True, exist_ok=True)
    ranked = sorted(cluster_rows, key=lambda r: (
        r["culling_waste_ratio"] * max(r["accepted_camera_count"], 1),
        r["xz_area_m2"] * (1 - r["position_fill_ratio"])), reverse=True)[:18]
    positions = scene.point_instances["position"]
    for row in ranked:
        cluster_id = row["cluster_id"]
        start, count = int(clusters.starts[cluster_id]), int(clusters.counts[cluster_id])
        ids = clusters.order[start:start + count]
        p = np.asarray(positions[ids])
        fig, ax = plt.subplots(figsize=(7, 7))
        ax.scatter(p[:, 0], p[:, 2], s=18, alpha=.72, color="#207a62")
        mn, mx = clusters.minimum[cluster_id], clusters.maximum[cluster_id]
        ax.add_patch(patches.Rectangle((mn[0], mn[2]), mx[0]-mn[0], mx[2]-mn[2],
                                       fill=False, lw=2, color="#c65342", label="cluster AABB"))
        ax.set_aspect("equal"); ax.grid(True); ax.legend()
        ax.set(title=f"Cluster {cluster_id} · {row['category']} · {count} instances\n"
                     f"waste {row['culling_waste_ratio']:.1%}, fill {row['position_fill_ratio']:.1%}",
               xlabel="X (m)", ylabel="Z (m)")
        save_figure(fig, details / f"cluster_{cluster_id:05d}.png")
    return ranked


def fmt(value):
    if isinstance(value, bool): return "yes" if value else "no"
    if isinstance(value, int): return f"{value:,}"
    if isinstance(value, float):
        if 0 <= value <= 1: return f"{value:.1%}"
        return f"{value:,.2f}"
    return str(value)


def html_table(rows: list[dict], columns: list[str], limit=None):
    selected = rows if limit is None else rows[:limit]
    header = "".join(f"<th>{html.escape(c)}</th>" for c in columns)
    body = []
    for row in selected:
        body.append("<tr>" + "".join(f"<td>{html.escape(fmt(row[c]))}</td>" for c in columns) + "</tr>")
    return f"<div class='table-wrap'><table><thead><tr>{header}</tr></thead><tbody>{''.join(body)}</tbody></table></div>"


def make_report(output: Path, summary: dict, category_rows, batch_rows, camera_rows,
                sweep_rows, worst_rows):
    current = next(r for r in sweep_rows if r["is_current"])
    best = min(sweep_rows, key=lambda r: r["balanced_score"])
    worst_categories = sorted(category_rows, key=lambda r: r["culling_waste_ratio"], reverse=True)
    detail_cards = "".join(
        f"<a class='detail' href='../details/cluster_{r['cluster_id']:05d}.png'>"
        f"<img src='../details/cluster_{r['cluster_id']:05d}.png' alt='cluster {r['cluster_id']}'>"
        f"<span>#{r['cluster_id']} · {html.escape(r['category'])}</span></a>" for r in worst_rows)
    report = f"""<!doctype html><html lang='ko'><head><meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'><title>FastJungle Spatial Batch Audit</title>
<style>
:root{{--paper:#f4f0e7;--ink:#2d302d;--muted:#676c65;--green:#246f5b;--red:#b94e3f;--line:#d6cfc2;--card:#fffdf8}}
*{{box-sizing:border-box}} body{{margin:0;background:var(--paper);color:var(--ink);font:15px/1.55 system-ui,sans-serif}}
header{{background:#173f35;color:#fff;padding:54px max(5vw,24px) 44px}} header p{{max-width:900px;color:#d5e9df}}
main{{max-width:1380px;margin:auto;padding:34px 24px 80px}} h2{{margin-top:44px;border-bottom:1px solid var(--line);padding-bottom:8px}}
.cards{{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px}} .card{{background:var(--card);border:1px solid var(--line);padding:18px;border-radius:10px}}
.card strong{{display:block;font-size:25px;color:var(--green)}} .callout{{border-left:5px solid var(--green);background:var(--card);padding:18px 22px;margin:20px 0}}
.warning{{border-left-color:var(--red)}} .charts{{display:grid;grid-template-columns:repeat(auto-fit,minmax(440px,1fr));gap:18px}} .charts img{{width:100%;background:white;border:1px solid var(--line);border-radius:8px}}
.table-wrap{{overflow:auto;max-height:650px;border:1px solid var(--line);background:var(--card)}} table{{border-collapse:collapse;width:100%;font-size:13px}} th{{position:sticky;top:0;background:#e8e1d5;z-index:1}} th,td{{padding:8px 10px;border-bottom:1px solid #e6dfd3;text-align:right;white-space:nowrap}} th:first-child,td:first-child{{text-align:left}}
.details{{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:12px}} .detail{{color:var(--ink);text-decoration:none;background:var(--card);border:1px solid var(--line);padding:8px}} .detail img{{width:100%}} .detail span{{display:block;padding:5px}}
code{{background:#e7e1d7;padding:2px 5px;border-radius:3px}} footer{{color:var(--muted);margin-top:50px}}
@media(max-width:700px){{.charts{{grid-template-columns:1fr}}}}
</style></head><body><header><h1>FastJungle Spatial Batch Audit</h1>
<p>원격 <code>{summary['git_commit']}</code>의 v8 release cook 결과에서 production 공간 clustering을 재현하고, transformed instance AABB를 기준으로 배치 밀도와 보수적 frustum culling 비용을 측정한 보고서입니다.</p></header><main>
<section class='cards'>
<div class='card'><span>Point instances</span><strong>{summary['point_instances']:,}</strong></div>
<div class='card'><span>Source batches</span><strong>{summary['source_batches']:,}</strong></div>
<div class='card'><span>Runtime clusters</span><strong>{summary['runtime_clusters']:,}</strong></div>
<div class='card'><span>Mean occupancy</span><strong>{summary['mean_occupancy']:.1%}</strong></div>
<div class='card'><span>Camera samples</span><strong>{summary['camera_samples']}</strong></div>
<div class='card'><span>Aggregate culling waste</span><strong>{summary['aggregate_culling_waste']:.1%}</strong></div>
</section>
<div class='callout {'warning' if summary['aggregate_culling_waste'] > .5 else ''}'><b>판정:</b> 현재 1.0x cell / 256 cap은 평균 LOD0 draw {current['average_visible_lod0_draws']:,.0f}개, instance waste {current['instance_waste_ratio']:.1%}입니다. 이 sweep의 균형 점수 최저는 {best['cell_multiplier']:g}x / {best['cluster_cap']} ({best['average_visible_lod0_draws']:,.0f} draws, {best['instance_waste_ratio']:.1%} waste)입니다. 이는 제한된 카메라 샘플에 대한 진단값이며 즉시 설정을 바꾸라는 결론은 아닙니다.</div>
<h2>핵심 해석</h2><ul>
<li>가장 높은 category culling waste: <b>{worst_categories[0]['category']}</b> {worst_categories[0]['culling_waste_ratio']:.1%}.</li>
<li>Cluster AABB가 통과했지만 실제 instance AABB가 하나도 통과하지 않은 비율과, 통과 cluster 안의 불필요 instance 비율을 분리했습니다.</li>
<li>LOD 선택은 일부러 고정하지 않았습니다. 공간 구조 자체를 보기 위해 모든 workload 수치는 LOD0 triangle/draw template 기준입니다.</li>
<li>정확성 invariant: 누락 instance {summary['invariants']['missing_instances']}, 중복 instance {summary['invariants']['duplicate_instances']}, cap 위반 {summary['invariants']['oversized_clusters']}, cluster가 놓친 visible instance {summary['invariants']['rejected_visible_instances']}.</li>
</ul>
<h2>공간 분포와 배치 품질</h2><div class='charts'>
<img src='../charts/world_density.png'><img src='../charts/cluster_occupancy.png'>
<img src='../charts/area_vs_culling_waste.png'><img src='../charts/category_efficiency.png'></div>
<h2>카메라 culling과 설정 sweep</h2><div class='charts'>
<img src='../charts/camera_culling.png'><img src='../charts/configuration_pareto.png'>
<img src='../charts/configuration_cluster_count.png'></div>
<h2>카테고리 요약</h2>{html_table(category_rows, ['category','instances','clusters','mean_capacity_utilization','median_cluster_diagonal_m','culling_waste_ratio','false_positive_cluster_ratio'])}
<h2>설정 비교</h2>{html_table(sweep_rows, ['cell_multiplier','cluster_cap','cluster_count','mean_occupancy','instance_waste_ratio','false_positive_cluster_ratio','average_visible_lod0_draws','balanced_score'])}
<h2>Source PointBatch</h2>{html_table(batch_rows, ['batch_id','category','mesh','instance_count','cluster_count','mean_capacity_utilization','p95_cluster_diagonal_m','culling_waste_ratio'])}
<h2>Worst cluster 상세</h2><p>카메라에서 반복적으로 낭비가 발생하고 공간 fill이 낮은 cluster를 우선 표시합니다. 이미지를 누르면 원본을 엽니다.</p><div class='details'>{detail_cards}</div>
<h2>재현 방법</h2><p><code>python tools/generate_spatial_batch_audit.py --scene &lt;JungleRuins.fjscene&gt; --output &lt;folder&gt; --git-commit &lt;sha&gt;</code></p>
<p>원시 표는 <code>raw/cluster_metrics.csv</code>, <code>raw/source_batch_metrics.csv</code>, <code>raw/category_summary.csv</code>, <code>raw/camera_metrics.csv</code>, <code>raw/camera_category_metrics.csv</code>, <code>raw/configuration_sweep.csv</code>에 있습니다.</p>
<footer>Generated deterministically with NumPy, Matplotlib and Pillow. Report paths are relative and GitHub-compatible.</footer>
</main></body></html>"""
    report_dir = output / "report"
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / "index.html").write_text(report, encoding="utf-8")


def make_readme(output: Path, summary: dict, category_rows, sweep_rows):
    current = next(r for r in sweep_rows if r["is_current"])
    best = min(sweep_rows, key=lambda r: r["balanced_score"])
    worst = max(category_rows, key=lambda r: r["culling_waste_ratio"])
    text = f"""# FastJungle spatial batch audit

Remote commit `{summary['git_commit']}`, Release-cooked FJSCENE v8 기준 공간 배치 감사 결과입니다.

| Metric | Result |
|---|---:|
| Point instances | {summary['point_instances']:,} |
| Source PointBatch | {summary['source_batches']:,} |
| Runtime PointCluster | {summary['runtime_clusters']:,} |
| Mean cluster occupancy | {summary['mean_occupancy']:.1%} |
| Deterministic cameras | {summary['camera_samples']} |
| Aggregate candidate-instance waste | {summary['aggregate_culling_waste']:.1%} |
| All correctness invariants passed | {summary['invariants']['all_passed']} |

현재 `1.0x cell / cap 256`은 24개 orbit sample에서 평균 LOD0 draw {current['average_visible_lod0_draws']:,.0f}개와 instance waste {current['instance_waste_ratio']:.1%}를 기록했습니다. 제한된 sweep의 균형 점수 최저는 `{best['cell_multiplier']:g}x / {best['cluster_cap']}`입니다. 가장 큰 category waste는 **{worst['category']} ({worst['culling_waste_ratio']:.1%})**였습니다.

> 이 값은 transformed instance AABB를 actual visibility 대용으로 사용한 보수적 진단입니다. LOD 효과를 분리하기 위해 draw/triangle workload는 LOD0 기준입니다.

## Overview

![World density](charts/world_density.png)

![Category efficiency](charts/category_efficiency.png)

![Camera culling](charts/camera_culling.png)

![Configuration Pareto](charts/configuration_pareto.png)

## Files

- [Interactive-style HTML report](report/index.html)
- [Summary JSON](summary.json)
- [Methodology](methodology.md)
- [Cluster metrics](raw/cluster_metrics.csv)
- [Source batch metrics](raw/source_batch_metrics.csv)
- [Category summary](raw/category_summary.csv)
- [Camera metrics](raw/camera_metrics.csv)
- [Configuration sweep](raw/configuration_sweep.csv)
- [Worst-cluster detail images](details/)
"""
    (output / "README.md").write_text(text, encoding="utf-8")


def verify_artifacts(output: Path):
    images = list(output.rglob("*.png"))
    for path in images:
        with Image.open(path) as image:
            image.verify()
    text = (output / "report" / "index.html").read_text(encoding="utf-8")
    missing = []
    import re
    for relative in re.findall(r"(?:src|href)='([^']+)'", text):
        if relative.startswith(("http:", "https:", "#")):
            continue
        target = (output / "report" / relative).resolve()
        if not target.exists(): missing.append(relative)
    if missing:
        raise ValueError(f"Report contains missing links: {missing[:10]}")
    return len(images)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--git-commit", default="unknown")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scene = CookedScene(args.scene.resolve())
    try:
        print("[1/8] Reading mesh bounds and workloads", flush=True)
        meshes = mesh_information(scene)
        with tempfile.TemporaryDirectory(prefix="fj-spatial-audit-", ignore_cleanup_errors=True) as temp:
            temp = Path(temp)
            print("[2/8] Transforming exact instance AABBs", flush=True)
            instance_bounds, finite = build_instance_bounds(scene, meshes, temp / "instance_bounds.f32")
            print("[3/8] Reproducing production clusters", flush=True)
            production = build_clusters(scene, instance_bounds, 1.0, 256)
            print("[4/8] Computing per-cluster spatial shape metrics", flush=True)
            shapes = cluster_shape_metrics(scene, production, instance_bounds)
            world_min = np.min(production.minimum, axis=0)
            world_max = np.max(production.maximum, axis=0)
            cameras = camera_suite(scene, world_min, world_max)
            print(f"[5/8] Evaluating {len(cameras)} exact AABB camera samples", flush=True)
            camera_rows, camera_category_rows, visibility, culling = evaluate_cameras(
                scene, meshes, production, instance_bounds, cameras, temp / "visibility.bool")
            print("[6/8] Sweeping 9 cell/cap configurations", flush=True)
            sweep_rows = evaluate_sweep(scene, meshes, instance_bounds, production, cameras, visibility)
            print("[7/8] Writing CSV, charts, and cluster details", flush=True)
            cluster_rows, batch_rows, category_rows = build_rows(
                scene, meshes, production, shapes, culling)
            write_csv(output / "raw" / "cluster_metrics.csv", cluster_rows)
            write_csv(output / "raw" / "source_batch_metrics.csv", batch_rows)
            write_csv(output / "raw" / "category_summary.csv", category_rows)
            write_csv(output / "raw" / "camera_metrics.csv", camera_rows)
            write_csv(output / "raw" / "camera_category_metrics.csv", camera_category_rows)
            write_csv(output / "raw" / "configuration_sweep.csv", sweep_rows)
            make_charts(scene, production, cluster_rows, category_rows, camera_rows, sweep_rows, output)
            worst_rows = make_cluster_details(scene, production, cluster_rows, output)
            order_unique = len(np.unique(production.order))
            candidate_total = sum(r["candidate_instances"] for r in camera_rows)
            actual_total = sum(r["actual_in_candidate_clusters"] for r in camera_rows)
            summary = {
                "git_commit": args.git_commit,
                "cook_provenance": "Remote commit plus a temporary 1 MiB texture-hash buffer stack-to-heap crash fix; cooked content logic is unchanged.",
                "scene": args.scene.as_posix(),
                "scene_size_bytes": args.scene.stat().st_size,
                "scene_sha256": file_sha256(args.scene),
                "point_instances": len(scene.point_instances),
                "source_batches": len(scene.point_batches),
                "runtime_clusters": len(production.counts),
                "mean_occupancy": float(np.mean(production.counts / 256)),
                "camera_samples": len(cameras),
                "aggregate_culling_waste": (candidate_total - actual_total) / candidate_total,
                "world_bounds": {"min": world_min.tolist(), "max": world_max.tolist()},
                "invariants": {
                    "missing_instances": len(scene.point_instances) - order_unique,
                    "duplicate_instances": len(production.order) - order_unique,
                    "oversized_clusters": int(np.count_nonzero(production.counts > 256)),
                    "non_finite_instance_bounds": 0 if finite else 1,
                    "rejected_visible_instances": int(culling["missed_total"]),
                    "all_passed": bool(order_unique == len(scene.point_instances) and
                                       len(production.order) == order_unique and
                                       np.all(production.counts <= 256) and finite and
                                       culling["missed_total"] == 0),
                },
            }
            make_report(output, summary, category_rows, batch_rows, camera_rows, sweep_rows, worst_rows)
            make_readme(output, summary, category_rows, sweep_rows)
            (output / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
            methodology = """# Methodology\n\n- Input: FastJungle `FJSCENE` v8 metadata produced by the Release cooker.\n- Production parity: category cell sizes and Morton/cell/source ordering mirror `SceneBatchBuilder.cpp`; clusters never cross a source batch or cell and use cap 256.\n- Bounds: each LOD0 mesh AABB is transformed with `PointBatch::local_transform * PointInstance SRT`, matching `SceneBoundsBuilder.cpp`.\n- Visibility: the same six-plane positive-vertex AABB/frustum test as `Frustum.hpp`. `actual` means an individual transformed instance AABB intersects; it is still conservative geometry visibility.\n- Sweep: 0.5x/1x/2x category cell sizes crossed with caps 128/256/512, measured on 24 deterministic orbit cameras.\n- Workload: triangle and draw figures use LOD0 intentionally so LOD policy does not hide spatial batching effects.\n- Balanced score: instance waste + 0.15 × draw count relative to the current configuration. It is a comparison aid, not an engine objective function.\n"""
            (output / "methodology.md").write_text(methodology, encoding="utf-8")
            print("[8/8] Validating images and report links", flush=True)
            image_count = verify_artifacts(output)
            print(json.dumps({"output": str(output), "images": image_count, **summary}, ensure_ascii=False))
            visibility._mmap.close()
            instance_bounds._mmap.close()
    finally:
        scene.close()


if __name__ == "__main__":
    main()
