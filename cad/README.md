# CAD: Legacy Arduino Enclosure Halves

These files describe the previous Arduino Uno prototype enclosure. They are
kept as historical reference only. The current M5Stack Tough plan uses the
standard Tough enclosure plus an external IP-rated junction box for relay,
fuses, and load wiring.

This folder generates two STL enclosure halves for the sail timer build:

- `upper_plate.stl`: lid with OLED + button + switch cutouts, standoffs, and a nesting lip
- `lower_plate.stl`: base with Arduino Uno + relay + speaker + DCDC buck standoffs and full side walls

## Generate

Use the local venv created for CAD work:

```bash
python -m venv /home/pm/dev/sail/.venv-cad
/home/pm/dev/sail/.venv-cad/bin/pip install -r /home/pm/dev/sail/cad/requirements.txt
/home/pm/dev/sail/.venv-cad/bin/python /home/pm/dev/sail/cad/generate_plates.py
```

If you are reusing an older CAD venv and hit a triangulation error, refresh the optional dependency:

```bash
/home/pm/dev/sail/.venv-cad/bin/pip install mapbox_earcut
```

STLs will be written to `cad/out/`.

## Assumptions

These are the defaults baked into `cad/generate_plates.py`:

- Grove module footprints use the Grove **20x20** and **20x40** standard hole patterns.
- Grove OLED is treated as a 20x40 module.
- OLED screen cutout uses panel size `26.7 x 19.26 mm` plus `0.6 mm` total clearance,
  centered within the OLED module footprint.
- Rotary/button/switch cutouts (temporary defaults):
  - Rotary: Ø7 mm, centered
  - Button: Ø6 mm, centered
  - Switch: 8 x 3 mm, centered
- "Sound module" is treated as the Grove Speaker (20x20 footprint).
- Relay module is assumed **40x40** with 4 corner holes (3.5 mm inset).
- DCDC module (DEBO DCDC DOWN 1 / LM2596) is assumed **45x32** with 4 corner holes (3.0 mm inset).
- Arduino Uno R3 hole positions follow a common DXF/SCAD template.
- Plate thickness: 3 mm, post height: 8 mm.
- Enclosure uses a shared footprint sized to the larger of the top-control layout and lower-electronics layout.
- Lower half gets full-height side walls.
- Upper half gets a 6 mm nesting lip with 0.4 mm slip-fit clearance.
- Default internal enclosure height is `22 mm` above the lower plate.
- A right-wall cable opening is included as a `12 x 6 mm` slot near the relay / DCDC area.

If any module has a different footprint or hole pattern, update the sizes
and hole coordinates at the top of `cad/generate_plates.py` and rerun.

## Layout

Upper plate:
- OLED / controls cluster is centered on the lid inside the shared enclosure footprint
- OLED on the left of that cluster
- Button above switch on the right
- Rotary above the button

Lower plate:
- Arduino Uno on the left
- Relay (top-left of module area)
- Speaker (bottom-left of module area)
- DCDC (top-right of module area)
- Cable slot on the right wall, aligned with the power / relay side

All module layouts still use 3 mm margins and 3 mm gaps inside the enclosure footprint.
