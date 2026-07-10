# Fonts

The firmware draws text with TFT_eSPI **smooth fonts**: anti-aliased `.vlw`
blobs, embedded into the binary (see `board_build.embed_txtfiles` in
`platformio.ini`) and loaded by name with `loadFont()`.

They are all **Cabin** (SIL OFL) — a humanist sans in the Gill Sans / Johnston
tradition — covering printable ASCII only, since every string the firmware draws
is sanitised to that range. See [`../ATTRIBUTION.md`](../ATTRIBUTION.md) for the
licence.

Each file is named `cabin<N>` for the **pixel size** (`vlwconv -s`) it was
generated at, so the name says what the file is. What matters on screen is the
resulting **cap height** — the pixel height of capitals and digits — which is why
the table lists both.

| file        | vlwconv `-s` | cap height | used for                                   |
|-------------|:------------:|:----------:|--------------------------------------------|
| `cabin11`   | 11           | 10         | graph axis labels                          |
| `cabin12`   | 12           | 12         | graph axis labels                          |
| `cabin16`   | 16           | 15         | rotating forecast stat                     |
| `cabin17`   | 17           | 15         | date                                       |
| `cabin21`   | 21           | 19         | temperature, wind, news ticker, brightness |
| `cabin26`   | 26           | 24         | AM/PM superscript (compact clock)          |
| `cabin28`   | 28           | 25         | AM/PM superscript (full clock)             |

## Regenerating

Generated with [vlwconv](https://github.com/jdlr-au/vlwconv) (`freetype-py`, MIT)
from `Cabin[wdth,wght].ttf` ([Cabin on Google Fonts](https://fonts.google.com/specimen/Cabin),
OFL — the default instance).

```sh
pip install freetype-py
git clone https://github.com/jdlr-au/vlwconv
curl -L -o Cabin.ttf \
  "https://raw.githubusercontent.com/google/fonts/main/ofl/cabin/Cabin%5Bwdth%2Cwght%5D.ttf"

for pair in "cabin11 11" "cabin12 12" "cabin16 16" "cabin17 17" \
            "cabin21 21" "cabin26 26" "cabin28 28"; do
  set -- $pair
  rm -f "fonts/$1.vlw"     # vlwconv refuses to overwrite an existing file
  python vlwconv/vlwconv.py -r U+0020-U+007E -s "$2" Cabin.ttf "fonts/$1.vlw"
done
```

Notes for changing sizes or typeface:

- `vlwconv` **will not overwrite** an existing output — `rm` first, or it fails
  silently if you have redirected its output away.
- The `-s` pixel size is nominal; the on-screen size is the cap height. To match
  a target, regenerate and measure the max glyph height over U+0030–U+0039 and
  U+0041–U+005A (the 28-byte-per-glyph table after the 24-byte header), and
  adjust `-s` until it lines up. Then flash and eyeball — a different face's
  proportions (especially its width) may still want a nudge.
- Cabin was chosen over the wider FreeSans because its narrower advance widths
  keep the bottom weather line from crowding; width, not height, was the deciding
  factor.
