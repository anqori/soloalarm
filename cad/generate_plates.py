#!/usr/bin/env python
"""Generate STL enclosure halves with standoff posts for the sail timer build.

Assumptions are documented in cad/README.md.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import trimesh
from shapely.geometry import box

# Geometry parameters
PLATE_THICKNESS_MM = 3.0
POST_HEIGHT_MM = 8.0
POST_SECTIONS = 64
WALL_THICKNESS_MM = 3.0
LID_LIP_THICKNESS_MM = 2.0
LID_LIP_HEIGHT_MM = 6.0
LID_LIP_CLEARANCE_MM = 0.4

# Enclosure sizing
LAYOUT_MARGIN_MM = 3.0
LAYOUT_GAP_MM = 3.0
ENCLOSURE_INNER_HEIGHT_MM = POST_HEIGHT_MM + 14.0

# Cable opening on the right wall of the lower half
CABLE_OPENING_W_MM = 12.0
CABLE_OPENING_H_MM = 6.0
CABLE_OPENING_SILL_MM = 4.0

# Hole + post sizing
GROVE_HOLE_D_MM = 2.2
GROVE_POST_OD_MM = 5.0

M3_HOLE_D_MM = 3.2
M3_POST_OD_MM = 7.0

# Grove standard hole patterns (local coords, origin at board bottom-left)
GROVE_20X20_HOLES = [
    (10.0, 5.0),
    (10.0, 15.0),
]

GROVE_20X40_HOLES = [
    (5.0, 5.0),
    (5.0, 15.0),
    (35.0, 10.0),
]

# Arduino Uno R3 mounting holes (mm), inferred from a common DXF/SCAD template
UNO_WIDTH_MM = 68.58
UNO_HEIGHT_MM = 53.34
UNO_HOLES = [
    (15.24, 2.54),   # A
    (66.04, 17.78),  # B
    (66.04, 45.72),  # C
    (13.97, 50.80),  # D
]

# Relay + DCDC assumptions (can be tuned)
RELAY_SIZE_MM = (40.0, 40.0)
RELAY_HOLES = [
    (3.5, 3.5),
    (3.5, 36.5),
    (36.5, 3.5),
    (36.5, 36.5),
]

DCDC_SIZE_MM = (45.0, 32.0)
DCDC_HOLES = [
    (3.0, 3.0),
    (3.0, 29.0),
    (42.0, 3.0),
    (42.0, 29.0),
]

SPEAKER_SIZE_MM = (20.0, 20.0)

OLED_PANEL_W_MM = 26.7
OLED_PANEL_H_MM = 19.26
OLED_CUTOUT_CLEARANCE_MM = 0.6

ROTARY_CUTOUT_D_MM = 7.0
BUTTON_CUTOUT_D_MM = 6.0
SWITCH_CUTOUT_W_MM = 8.0
SWITCH_CUTOUT_H_MM = 3.0


@dataclass(frozen=True)
class Module:
    name: str
    size: tuple[float, float]
    holes: list[tuple[float, float]]
    hole_d: float
    post_od: float


def plate_mesh(
    width: float,
    height: float,
    thickness: float,
    cutouts: list[tuple[float, float, float, float]] | None = None,
    round_cutouts: list[tuple[float, float, float]] | None = None,
) -> trimesh.Trimesh:
    outer = box(0.0, 0.0, width, height)
    poly = outer
    if cutouts:
        for x, y, w, h in cutouts:
            poly = poly.difference(box(x, y, x + w, y + h))
    if round_cutouts:
        for cx, cy, d in round_cutouts:
            poly = poly.difference(
                box(cx - d / 2.0, cy - d / 2.0, cx + d / 2.0, cy + d / 2.0).buffer(
                    d / 2.0
                )
            )
    return trimesh.creation.extrude_polygon(poly, thickness)


def rect_prism_mesh(
    x: float,
    y: float,
    width: float,
    height: float,
    z_base: float,
    depth: float,
) -> trimesh.Trimesh:
    prism = trimesh.creation.box(extents=(width, height, depth))
    prism.apply_translation((x + width / 2.0, y + height / 2.0, z_base + depth / 2.0))
    return prism


def post_mesh(
    x: float,
    y: float,
    hole_d: float,
    post_od: float,
    height: float,
    z_base: float,
) -> trimesh.Trimesh:
    ann = trimesh.creation.annulus(
        r_min=hole_d / 2.0,
        r_max=post_od / 2.0,
        height=height,
        sections=POST_SECTIONS,
    )
    ann.apply_translation((x, y, z_base + height / 2.0))
    return ann


def add_module_posts(
    meshes: list[trimesh.Trimesh],
    module: Module,
    origin: tuple[float, float],
) -> None:
    ox, oy = origin
    for hx, hy in module.holes:
        meshes.append(
            post_mesh(
                ox + hx,
                oy + hy,
                module.hole_d,
                module.post_od,
                POST_HEIGHT_MM,
                PLATE_THICKNESS_MM,
            )
        )


def translate_points(
    placements: dict[str, tuple[float, float]],
    dx: float,
    dy: float,
) -> dict[str, tuple[float, float]]:
    return {name: (x + dx, y + dy) for name, (x, y) in placements.items()}


def upper_cluster_size() -> tuple[float, float]:
    right_col_h = 20.0 + LAYOUT_GAP_MM + 20.0 + LAYOUT_GAP_MM + 20.0
    width = LAYOUT_MARGIN_MM * 2.0 + 40.0 + LAYOUT_GAP_MM + 20.0
    height = LAYOUT_MARGIN_MM * 2.0 + max(20.0, right_col_h)
    return (width, height)


def lower_cluster_size() -> tuple[float, float]:
    module_area_h = RELAY_SIZE_MM[1] + LAYOUT_GAP_MM + SPEAKER_SIZE_MM[1]
    module_area_w = RELAY_SIZE_MM[0] + LAYOUT_GAP_MM + DCDC_SIZE_MM[0]
    width = LAYOUT_MARGIN_MM * 2.0 + UNO_WIDTH_MM + LAYOUT_GAP_MM + module_area_w
    height = LAYOUT_MARGIN_MM * 2.0 + max(UNO_HEIGHT_MM, module_area_h)
    return (width, height)


def enclosure_footprint() -> tuple[float, float]:
    upper_w, upper_h = upper_cluster_size()
    lower_w, lower_h = lower_cluster_size()
    return (max(upper_w, lower_w), max(upper_h, lower_h))


def lid_lip_meshes(width: float, height: float) -> list[trimesh.Trimesh]:
    lip_x = WALL_THICKNESS_MM + LID_LIP_CLEARANCE_MM
    lip_y = WALL_THICKNESS_MM + LID_LIP_CLEARANCE_MM
    lip_w = width - 2.0 * lip_x
    lip_h = height - 2.0 * lip_y
    lip_z = PLATE_THICKNESS_MM

    return [
        rect_prism_mesh(
            lip_x,
            lip_y,
            LID_LIP_THICKNESS_MM,
            lip_h,
            lip_z,
            LID_LIP_HEIGHT_MM,
        ),
        rect_prism_mesh(
            width - lip_x - LID_LIP_THICKNESS_MM,
            lip_y,
            LID_LIP_THICKNESS_MM,
            lip_h,
            lip_z,
            LID_LIP_HEIGHT_MM,
        ),
        rect_prism_mesh(
            lip_x + LID_LIP_THICKNESS_MM,
            lip_y,
            lip_w - 2.0 * LID_LIP_THICKNESS_MM,
            LID_LIP_THICKNESS_MM,
            lip_z,
            LID_LIP_HEIGHT_MM,
        ),
        rect_prism_mesh(
            lip_x + LID_LIP_THICKNESS_MM,
            height - lip_y - LID_LIP_THICKNESS_MM,
            lip_w - 2.0 * LID_LIP_THICKNESS_MM,
            LID_LIP_THICKNESS_MM,
            lip_z,
            LID_LIP_HEIGHT_MM,
        ),
    ]


def enclosure_wall_meshes(
    width: float,
    height: float,
    cable_center_y: float,
) -> list[trimesh.Trimesh]:
    wall_z = PLATE_THICKNESS_MM
    wall_top_z = wall_z + ENCLOSURE_INNER_HEIGHT_MM
    slot_y = min(
        max(cable_center_y - CABLE_OPENING_W_MM / 2.0, WALL_THICKNESS_MM),
        height - WALL_THICKNESS_MM - CABLE_OPENING_W_MM,
    )
    slot_y_top = slot_y + CABLE_OPENING_W_MM
    slot_z = wall_z + CABLE_OPENING_SILL_MM
    slot_z_top = slot_z + CABLE_OPENING_H_MM

    meshes = [
        rect_prism_mesh(
            0.0,
            0.0,
            WALL_THICKNESS_MM,
            height,
            wall_z,
            ENCLOSURE_INNER_HEIGHT_MM,
        ),
        rect_prism_mesh(
            WALL_THICKNESS_MM,
            0.0,
            width - 2.0 * WALL_THICKNESS_MM,
            WALL_THICKNESS_MM,
            wall_z,
            ENCLOSURE_INNER_HEIGHT_MM,
        ),
        rect_prism_mesh(
            WALL_THICKNESS_MM,
            height - WALL_THICKNESS_MM,
            width - 2.0 * WALL_THICKNESS_MM,
            WALL_THICKNESS_MM,
            wall_z,
            ENCLOSURE_INNER_HEIGHT_MM,
        ),
    ]

    if slot_z > wall_z:
        meshes.append(
            rect_prism_mesh(
                width - WALL_THICKNESS_MM,
                0.0,
                WALL_THICKNESS_MM,
                height,
                wall_z,
                slot_z - wall_z,
            )
        )

    if wall_top_z > slot_z_top:
        meshes.append(
            rect_prism_mesh(
                width - WALL_THICKNESS_MM,
                0.0,
                WALL_THICKNESS_MM,
                height,
                slot_z_top,
                wall_top_z - slot_z_top,
            )
        )

    meshes.append(
        rect_prism_mesh(
            width - WALL_THICKNESS_MM,
            0.0,
            WALL_THICKNESS_MM,
            slot_y,
            slot_z,
            CABLE_OPENING_H_MM,
        )
    )
    meshes.append(
        rect_prism_mesh(
            width - WALL_THICKNESS_MM,
            slot_y_top,
            WALL_THICKNESS_MM,
            height - slot_y_top,
            slot_z,
            CABLE_OPENING_H_MM,
        )
    )

    return meshes


def build_upper_plate() -> tuple[trimesh.Trimesh, dict]:
    # Layout: OLED (20x40) left, Button + Switch stacked on right
    margin = LAYOUT_MARGIN_MM
    gap = LAYOUT_GAP_MM

    oled = Module("oled", (40.0, 20.0), GROVE_20X40_HOLES, GROVE_HOLE_D_MM, GROVE_POST_OD_MM)
    button = Module("button", (20.0, 20.0), GROVE_20X20_HOLES, GROVE_HOLE_D_MM, GROVE_POST_OD_MM)
    switch = Module("switch", (20.0, 20.0), GROVE_20X20_HOLES, GROVE_HOLE_D_MM, GROVE_POST_OD_MM)
    rotary = Module("rotary", (20.0, 20.0), GROVE_20X20_HOLES, GROVE_HOLE_D_MM, GROVE_POST_OD_MM)

    oled_cutout_w = OLED_PANEL_W_MM + OLED_CUTOUT_CLEARANCE_MM
    oled_cutout_h = OLED_PANEL_H_MM + OLED_CUTOUT_CLEARANCE_MM

    right_col_h = button.size[1] + gap + switch.size[1] + gap + rotary.size[1]
    cluster_w = margin * 2 + oled.size[0] + gap + button.size[0]
    cluster_h = margin * 2 + max(oled.size[1], right_col_h)
    plate_w, plate_h = enclosure_footprint()
    offset_x = (plate_w - cluster_w) / 2.0
    offset_y = (plate_h - cluster_h) / 2.0

    placements = translate_points(
        {
            "oled": (margin, margin + (right_col_h - oled.size[1]) / 2.0),
            "rotary": (margin + oled.size[0] + gap, margin + switch.size[1] + gap + button.size[1] + gap),
            "button": (margin + oled.size[0] + gap, margin + switch.size[1] + gap),
            "switch": (margin + oled.size[0] + gap, margin),
        },
        offset_x,
        offset_y,
    )
    oled_pos = placements["oled"]
    rotary_pos = placements["rotary"]
    button_pos = placements["button"]
    switch_pos = placements["switch"]

    cutout_x = oled_pos[0] + (oled.size[0] - oled_cutout_w) / 2.0
    cutout_y = oled_pos[1] + (oled.size[1] - oled_cutout_h) / 2.0

    rotary_cutout = (
        rotary_pos[0] + rotary.size[0] / 2.0,
        rotary_pos[1] + rotary.size[1] / 2.0,
        ROTARY_CUTOUT_D_MM,
    )
    button_cutout = (
        button_pos[0] + button.size[0] / 2.0,
        button_pos[1] + button.size[1] / 2.0,
        BUTTON_CUTOUT_D_MM,
    )
    switch_cutout = (
        switch_pos[0] + (switch.size[0] - SWITCH_CUTOUT_W_MM) / 2.0,
        switch_pos[1] + (switch.size[1] - SWITCH_CUTOUT_H_MM) / 2.0,
        SWITCH_CUTOUT_W_MM,
        SWITCH_CUTOUT_H_MM,
    )

    meshes: list[trimesh.Trimesh] = [
        plate_mesh(
            plate_w,
            plate_h,
            PLATE_THICKNESS_MM,
            cutouts=[(cutout_x, cutout_y, oled_cutout_w, oled_cutout_h), switch_cutout],
            round_cutouts=[rotary_cutout, button_cutout],
        )
    ]
    meshes.extend(lid_lip_meshes(plate_w, plate_h))
    add_module_posts(meshes, oled, oled_pos)
    add_module_posts(meshes, rotary, rotary_pos)
    add_module_posts(meshes, button, button_pos)
    add_module_posts(meshes, switch, switch_pos)

    merged = trimesh.util.concatenate(meshes)
    meta = {
        "plate_size_mm": (plate_w, plate_h),
        "cluster_size_mm": (cluster_w, cluster_h),
        "lid_lip_height_mm": LID_LIP_HEIGHT_MM,
        "placements": placements,
    }
    return merged, meta


def build_lower_plate() -> tuple[trimesh.Trimesh, dict]:
    # Layout: UNO left, module area on right (relay+speaker column, DCDC column)
    margin = LAYOUT_MARGIN_MM
    gap = LAYOUT_GAP_MM

    uno = Module("uno", (UNO_WIDTH_MM, UNO_HEIGHT_MM), UNO_HOLES, M3_HOLE_D_MM, M3_POST_OD_MM)
    relay = Module("relay", RELAY_SIZE_MM, RELAY_HOLES, M3_HOLE_D_MM, M3_POST_OD_MM)
    speaker = Module("speaker", SPEAKER_SIZE_MM, GROVE_20X20_HOLES, GROVE_HOLE_D_MM, GROVE_POST_OD_MM)
    dcdc = Module("dcdc", DCDC_SIZE_MM, DCDC_HOLES, M3_HOLE_D_MM, M3_POST_OD_MM)

    module_area_h = relay.size[1] + gap + speaker.size[1]
    module_area_w = relay.size[0] + gap + dcdc.size[0]

    cluster_w = margin * 2 + uno.size[0] + gap + module_area_w
    cluster_h = margin * 2 + max(uno.size[1], module_area_h)
    plate_w, plate_h = enclosure_footprint()
    offset_x = (plate_w - cluster_w) / 2.0
    offset_y = (plate_h - cluster_h) / 2.0

    uno_pos = (margin + offset_x, margin + offset_y)
    module_x = margin + offset_x + uno.size[0] + gap

    relay_pos = (module_x, margin + offset_y + speaker.size[1] + gap)
    speaker_pos = (module_x, margin + offset_y)
    dcdc_pos = (module_x + relay.size[0] + gap, margin + offset_y + (module_area_h - dcdc.size[1]))
    cable_center_y = (
        relay_pos[1] + relay.size[1] / 2.0 + dcdc_pos[1] + dcdc.size[1] / 2.0
    ) / 2.0

    meshes: list[trimesh.Trimesh] = [plate_mesh(plate_w, plate_h, PLATE_THICKNESS_MM)]
    meshes.extend(enclosure_wall_meshes(plate_w, plate_h, cable_center_y))
    add_module_posts(meshes, uno, uno_pos)
    add_module_posts(meshes, relay, relay_pos)
    add_module_posts(meshes, speaker, speaker_pos)
    add_module_posts(meshes, dcdc, dcdc_pos)

    merged = trimesh.util.concatenate(meshes)
    meta = {
        "plate_size_mm": (plate_w, plate_h),
        "cluster_size_mm": (cluster_w, cluster_h),
        "inner_height_mm": ENCLOSURE_INNER_HEIGHT_MM,
        "cable_opening_mm": {
            "width": CABLE_OPENING_W_MM,
            "height": CABLE_OPENING_H_MM,
            "wall": "right",
        },
        "placements": {
            "uno": uno_pos,
            "relay": relay_pos,
            "speaker": speaker_pos,
            "dcdc": dcdc_pos,
        },
    }
    return merged, meta


def main() -> None:
    out_dir = Path(__file__).resolve().parent / "out"
    out_dir.mkdir(parents=True, exist_ok=True)

    upper_mesh, upper_meta = build_upper_plate()
    lower_mesh, lower_meta = build_lower_plate()

    upper_path = out_dir / "upper_plate.stl"
    lower_path = out_dir / "lower_plate.stl"

    upper_mesh.export(upper_path)
    lower_mesh.export(lower_path)

    print("Wrote", upper_path)
    print("Wrote", lower_path)
    print("Upper meta:", upper_meta)
    print("Lower meta:", lower_meta)


if __name__ == "__main__":
    main()
