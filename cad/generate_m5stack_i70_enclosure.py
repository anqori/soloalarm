#!/usr/bin/env python3
"""Generate an i70-style replacement lower enclosure for M5Stack Tough.

The coordinate system is:
- X: left/right across the front plate.
- Y: vertical across the front plate.
- Z: front/back, with the front face at positive Z and the rear cavity behind.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from build123d import Box, BuildPart, Cylinder, Locations, Mode, export_step, export_stl


@dataclass(frozen=True)
class CableHole:
  x: float
  y: float
  diameter: float


@dataclass(frozen=True)
class EnclosureConfig:
  # Raymarine i50/i60/i70s-style instrument face.
  front_width: float = 110.0
  front_height: float = 115.0
  target_depth: float = 44.0
  front_plate_thickness: float = 4.0
  front_corner_radius: float = 8.0

  # Rear electronics cavity behind the front plate.
  rear_box_width: float = 96.0
  rear_box_height: float = 94.0
  rear_box_depth: float = 40.0
  rear_box_corner_radius: float = 6.0
  wall_thickness: float = 3.0
  rear_wall_thickness: float = 3.0

  # M5Stack Tough upper-half interface.
  m5_width: float = 58.0
  m5_height: float = 58.0
  m5_corner_radius: float = 4.0
  m5_cutout_clearance: float = 0.8
  m5_front_recess_extra: float = 3.0
  m5_front_recess_depth: float = 1.5
  m5_body_insert_depth: float = 20.0

  # Raised ridge for the original M5Stack gasket.
  gasket_ridge_width: float = 1.4
  gasket_ridge_height: float = 1.2
  gasket_ridge_extra: float = 2.6

  # M5Stack upper-half fastener bosses, relative to the 58 x 58 mm body area.
  # Measure and adjust after teardown.
  m5_screw_dx: float = 49.0
  m5_screw_dy: float = 49.0
  m5_screw_clearance_diameter: float = 3.2
  m5_boss_diameter: float = 7.0
  m5_boss_height: float = 7.0
  m5_screw_column_diameter: float = 8.5
  m5_screw_head_counterbore_diameter: float = 6.2
  m5_screw_head_counterbore_depth: float = 3.0

  # Front panel mounting.
  include_panel_screws: bool = False
  panel_screw_dx: float = 88.0
  panel_screw_dy: float = 92.0
  panel_screw_clearance_diameter: float = 3.4
  panel_screw_counterbore_diameter: float = 6.5
  panel_screw_counterbore_depth: float = 1.4

  # Rear cable exits, intended for cable glands or printed strain reliefs.
  rear_cable_holes: tuple[CableHole, ...] = field(
    default_factory=lambda: (
      CableHole(x=0.0, y=-30.0, diameter=11.0),
    )
  )

  @property
  def rear_depth(self) -> float:
    return self.target_depth - self.front_plate_thickness

  @property
  def front_z(self) -> float:
    return self.front_plate_thickness / 2.0

  @property
  def rear_face_z(self) -> float:
    return -self.front_plate_thickness / 2.0 - self.rear_box_depth


def rounded_box_xy(
  width: float,
  height: float,
  depth: float,
  radius: float,
  z_center: float,
  mode: Mode = Mode.ADD,
) -> None:
  """Create a prism with rounded XY corners and depth along Z."""
  straight_w = max(width - 2.0 * radius, 0.01)
  straight_h = max(height - 2.0 * radius, 0.01)

  # Build the rounded rectangle from a center strip, a cross strip, and corners.
  with Locations((0, 0, z_center)):
    Box(straight_w, height, depth, mode=mode)
    Box(width, straight_h, depth, mode=mode)
  with Locations(
    (-width / 2.0 + radius, -height / 2.0 + radius, z_center),
    (width / 2.0 - radius, -height / 2.0 + radius, z_center),
    (-width / 2.0 + radius, height / 2.0 - radius, z_center),
    (width / 2.0 - radius, height / 2.0 - radius, z_center),
  ):
    Cylinder(radius=radius, height=depth, mode=mode)


def screw_positions(dx: float, dy: float) -> tuple[tuple[float, float], ...]:
  return (
    (-dx / 2.0, -dy / 2.0),
    (dx / 2.0, -dy / 2.0),
    (-dx / 2.0, dy / 2.0),
    (dx / 2.0, dy / 2.0),
  )


def build_enclosure(cfg: EnclosureConfig):
  cutout_w = cfg.m5_width + cfg.m5_cutout_clearance
  cutout_h = cfg.m5_height + cfg.m5_cutout_clearance
  recess_w = cfg.m5_width + cfg.m5_front_recess_extra * 2.0
  recess_h = cfg.m5_height + cfg.m5_front_recess_extra * 2.0
  ridge_inner_w = cutout_w
  ridge_inner_h = cutout_h
  ridge_outer_w = ridge_inner_w + cfg.gasket_ridge_width * 2.0 + cfg.gasket_ridge_extra
  ridge_outer_h = ridge_inner_h + cfg.gasket_ridge_width * 2.0 + cfg.gasket_ridge_extra
  rear_box_z = -cfg.front_plate_thickness / 2.0 - cfg.rear_box_depth / 2.0
  cavity_w = cfg.rear_box_width - cfg.wall_thickness * 2.0
  cavity_h = cfg.rear_box_height - cfg.wall_thickness * 2.0
  cavity_depth = cfg.rear_box_depth - cfg.rear_wall_thickness + 0.2
  cavity_z = cfg.rear_face_z + cfg.rear_wall_thickness + cavity_depth / 2.0

  with BuildPart() as part:
    rounded_box_xy(
      cfg.front_width,
      cfg.front_height,
      cfg.front_plate_thickness,
      cfg.front_corner_radius,
      0.0,
    )

    rounded_box_xy(
      cfg.rear_box_width,
      cfg.rear_box_height,
      cfg.rear_box_depth,
      cfg.rear_box_corner_radius,
      rear_box_z,
    )

    rounded_box_xy(cavity_w, cavity_h, cavity_depth, cfg.rear_box_corner_radius - 2.0, cavity_z, Mode.SUBTRACT)

    # Front flush pocket and body opening for the Tough upper shell. The body
    # opening follows the approximate upper-shell insertion depth and does not
    # continue through the rear wall.
    rounded_box_xy(
      recess_w,
      recess_h,
      cfg.m5_front_recess_depth + 0.2,
      cfg.m5_corner_radius + cfg.m5_front_recess_extra,
      cfg.front_z - cfg.m5_front_recess_depth / 2.0,
      Mode.SUBTRACT,
    )
    rounded_box_xy(
      cutout_w,
      cutout_h,
      cfg.m5_body_insert_depth,
      cfg.m5_corner_radius,
      cfg.front_z - cfg.m5_body_insert_depth / 2.0,
      Mode.SUBTRACT,
    )

    # Gasket ridge projecting from the back of the front plate.
    ridge_z = -cfg.front_plate_thickness / 2.0 - cfg.gasket_ridge_height / 2.0
    rounded_box_xy(ridge_outer_w, ridge_outer_h, cfg.gasket_ridge_height, cfg.m5_corner_radius + 2.0, ridge_z)
    rounded_box_xy(ridge_inner_w, ridge_inner_h, cfg.gasket_ridge_height + 0.4, cfg.m5_corner_radius, ridge_z, Mode.SUBTRACT)

    # M5Stack upper-half screw bosses and hollow support columns. The columns
    # are open at the rear face so the original long screws can enter from the
    # bottom/back and reach the Tough upper half.
    m5_insert_rear_z = cfg.front_z - cfg.m5_body_insert_depth
    boss_z = m5_insert_rear_z + cfg.m5_boss_height / 2.0
    column_top_z = boss_z - cfg.m5_boss_height / 2.0
    column_bottom_z = cfg.rear_face_z
    column_height = column_top_z - column_bottom_z
    column_z = column_bottom_z + column_height / 2.0
    for x, y in screw_positions(cfg.m5_screw_dx, cfg.m5_screw_dy):
      with Locations((x, y, column_z)):
        Cylinder(radius=cfg.m5_screw_column_diameter / 2.0, height=column_height)
        Cylinder(
          radius=cfg.m5_screw_clearance_diameter / 2.0,
          height=column_height + 0.8,
          mode=Mode.SUBTRACT,
        )
      with Locations((x, y, cfg.rear_face_z + cfg.m5_screw_head_counterbore_depth / 2.0)):
        Cylinder(
          radius=cfg.m5_screw_head_counterbore_diameter / 2.0,
          height=cfg.m5_screw_head_counterbore_depth + 0.2,
          mode=Mode.SUBTRACT,
        )
      with Locations((x, y, boss_z)):
        Cylinder(radius=cfg.m5_boss_diameter / 2.0, height=cfg.m5_boss_height)
        Cylinder(
          radius=cfg.m5_screw_clearance_diameter / 2.0,
          height=cfg.m5_boss_height + cfg.front_plate_thickness + 1.0,
          mode=Mode.SUBTRACT,
        )

    if cfg.include_panel_screws:
      # Raymarine-style front mounting holes.
      for x, y in screw_positions(cfg.panel_screw_dx, cfg.panel_screw_dy):
        with Locations((x, y, 0.0)):
          Cylinder(
            radius=cfg.panel_screw_clearance_diameter / 2.0,
            height=cfg.front_plate_thickness + 1.0,
            mode=Mode.SUBTRACT,
          )
        with Locations((x, y, cfg.front_z - cfg.panel_screw_counterbore_depth / 2.0)):
          Cylinder(
            radius=cfg.panel_screw_counterbore_diameter / 2.0,
            height=cfg.panel_screw_counterbore_depth + 0.2,
            mode=Mode.SUBTRACT,
          )

    # Rear cable exits through the back wall.
    for hole in cfg.rear_cable_holes:
      with Locations((hole.x, hole.y, cfg.rear_face_z + cfg.rear_wall_thickness / 2.0)):
        Cylinder(
          radius=hole.diameter / 2.0,
          height=cfg.rear_wall_thickness + 1.0,
          mode=Mode.SUBTRACT,
        )

  return part.part


def main() -> None:
  cfg = EnclosureConfig()
  out_dir = Path(__file__).resolve().parent / "out"
  out_dir.mkdir(parents=True, exist_ok=True)
  part = build_enclosure(cfg)
  export_step(part, out_dir / "m5stack_i70_enclosure.step")
  export_stl(part, out_dir / "m5stack_i70_enclosure.stl")
  print(f"Wrote {out_dir / 'm5stack_i70_enclosure.step'}")
  print(f"Wrote {out_dir / 'm5stack_i70_enclosure.stl'}")


if __name__ == "__main__":
  main()
