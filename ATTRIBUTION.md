# Attribution

Third-party assets bundled in this repository, and the terms they are used under.

## Icons

### `images/down.png`

The download arrow shown beside the WiFi signal bars while data is being fetched.

- **Icon set:** Ravenna 3D Iconpack (90 icons)
- **Designer:** Double-J Design
- **Source:** <https://www.iconarchive.com/show/ravenna-3d-icons-by-double-j-design.html>
- **License:** [Creative Commons Attribution 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/)

CC BY 4.0 permits use, including commercially, and permits modification, provided
credit is given. This file is that credit. The icon is embedded into the firmware
at build time (see `board_build.embed_files` in `platformio.ini`) rather than
distributed as a separate file.

## Fonts

The `.vlw` fonts under `fonts/` are all **Cabin**, by Pablo Impallari, Rodrigo
Fuenzalida and Igino Marini, embedded into the firmware.

- **Source:** [Cabin on Google Fonts](https://fonts.google.com/specimen/Cabin)
  ([github.com/impallari/Cabin](https://github.com/impallari/Cabin))
- **License:** [SIL Open Font License 1.1](https://openfontlicense.org/) — free
  to use, embed, and modify; embedding imposes no licence on this project.
- Files are named `cabin<N>` for the pixel size each was generated at. See
  [`fonts/README.md`](fonts/README.md) for how they are produced (with
  [vlwconv](https://github.com/jdlr-au/vlwconv), MIT).

## Libraries

Pulled in by PlatformIO at build time and not vendored here; each carries its own
license.

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — FreeBSD / BSD-2-Clause
- [OneButton](https://github.com/mathertel/OneButton) — BSD-3-Clause
- [PNGdec](https://github.com/bitbank2/PNGdec) — Apache-2.0
- [ArduinoJson](https://arduinojson.org/) — MIT

## Data sources

- Current conditions and hourly forecast: [WeatherAPI](https://www.weatherapi.com/),
  used with a personal API key. Their terms apply to your key, not to this code.
- Headlines: Google News RSS. Only the item titles and publication dates are read,
  and only to display them on the device.
