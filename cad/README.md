# M5Stack Tough i70-Style Enclosure

Parametric CAD for replacing the lower half of an M5Stack Tough with a larger
rear enclosure and a Raymarine i70-style front plate.

The intended build is:

- Disassemble the M5Stack Tough.
- Keep the upper display/touch half.
- Replace the lower shell with this printed front plate + deeper rear cavity.
- Reuse the original M5Stack sealing gasket between the Tough upper half and
  the printed gasket ridge.
- Route cables out through the rear cable-gland holes.

## Default Dimensions

- Front plate: `110 x 115 mm`, matching the Raymarine i50/i60 front-bezel
  family that matches i70s/p70s styling.
- Overall target depth: `44 mm`.
- M5Stack Tough nominal outer size: `76 x 58 x 41.6 mm`, including the left
  and right mounting ears. The printed interface uses the square `58 x 58 mm`
  Tough body/gasket area rather than the full ear-to-ear width.
- Rear cavity: larger than the original Tough lower half, with `40 mm` depth
  behind the front plate. The default rear box is `96 x 94 mm`, giving an
  internal cavity of roughly `90 x 88 x 37 mm` for an M5Stack Unit 2Relay,
  connector blocks, and cable bend room.

The M5Stack screw spacing is relative to the square `58 x 58 mm` body/gasket
area, not the full ear-to-ear width. Screw spacing, gasket ridge size, and
flush-pocket depth are parametric estimates. Measure the opened Tough shell and adjust
`EnclosureConfig` in `generate_m5stack_i70_enclosure.py` before committing to a
final print.

Dimension references:

- M5Stack Tough product size:
  <https://docs.m5stack.com/en/core/tough>
- Raymarine instrument front-bezel family dimensions:
  <https://www.raymarine.com/en-gb/our-products/marine-instruments/i50-and-i60-series/i50-depth>

## Generate

Build123d currently requires Python 3.10 through 3.13. If your system Python is
newer, create the venv with a supported interpreter.

```bash
python3.12 -m venv /tmp/alarmsolo-cad
/tmp/alarmsolo-cad/bin/pip install -r cad/requirements.txt
/tmp/alarmsolo-cad/bin/python cad/generate_m5stack_i70_enclosure.py
```

With `uv`:

```bash
uv python install 3.12
uv venv --python 3.12 /tmp/alarmsolo-cad
uv pip install --python /tmp/alarmsolo-cad/bin/python -r cad/requirements.txt
/tmp/alarmsolo-cad/bin/python cad/generate_m5stack_i70_enclosure.py
```

Outputs are written to `cad/out/`:

- `m5stack_i70_enclosure.step`
- `m5stack_i70_enclosure.stl`

## Design Notes

- The model uses a rounded i70-style front plate. Front panel mounting holes
  are optional and disabled by default; set `include_panel_screws = True` in
  `EnclosureConfig` to add the four M3 holes and counterbores.
- The M5Stack opening has a shallow front recess so the Tough upper shell can
  sit flush with the front face. The opening follows the approximate `20 mm`
  upper-shell insertion depth and does not continue through the rear wall.
- The gasket ridge projects behind the front plate and surrounds the Tough
  body opening.
- Four internal hollow columns support the Tough upper-half screw bosses. They
  are open at the rear face so long original screws can enter from underneath.
  The default clearance hole is `3.2 mm`, with a `6.2 x 3.0 mm` rear
  counterbore so the screw head can sit inside the rear box. The columns stop
  at the approximate `20 mm` M5Stack upper-shell insertion depth.
- One small rear cable exit is placed low on the back face.
