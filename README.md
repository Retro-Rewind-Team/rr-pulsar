# Retro Rewind

Retro Rewind is a custom track distribution created by ZPL. It features every retro track from Super Mario Kart to Mario Kart 7, as well as some Mario Kart 8, Mario Kart Tour and Mario Kart Arcade GP tracks. The distribution also features its own Homebrew application, auto updater and custom online server. As of v6.0, the distribution uses [Pulsar](https://github.com/MelgMKW/Pulsar) as its engine.

## Licensing

All source code in this repository is licensed under AGPLv3. This includes all features written by the Retro Rewind Team and any contributors.

We borrow features from mkw-sp under MIT, specifically the Input Viewer. mkw-sp can be found under the MIT license in [mkw-sp](https://github.com/mkw-sp/mkw-sp); see LICENSE_mkw-sp for more information.

Pulsar is licensed under MIT. Pulsar can be found under the MIT license in [Pulsar](https://github.com/MelgMKW/Pulsar); see LICENSE_pulsar for more information.

**NOTE:** You can ask to be fully exempted from these licensing constraints, feel free to contact the code author(s).

# Features
Retro Rewind is based on Pulsar. The original feature list from Pulsar can be found in the Pulsar repository.
Retro Rewind's feature list is always documented on the [Retro Rewind Tockdom page](https://wiki.tockdom.com/wiki/Retro_Rewind). The feature list may be behind the actual features available in the repository.

# Building

A makefile is available at the root of the repository. The `CFLAGS` environment variable can be defined to add additional arguments. The following switches are available to control the WFC domain to connect to and the keys used.

| Switch         | Domain           |
| -------------- | ---------------- |
| -DPROD         | rwfc.net         |
| -DTEST         | zpltest.xyz      |
| None Specified | nwfc.wiinoma.com |

When using nwfc.wiinoma.com for a local testing server, the default key is used, which can be found [here](https://github.com/Retro-Rewind-Team/wfc-patcher-wii/blob/main/misc/private-key-DEFAULT.pem)

`AS`, `CC` and `KAMEK` can both be set in a `.env` file to specify the location of your copy of mwasmeppc, mwcceppc and Kamek respectively.

rr-pulsar is not built to work with WFC servers other than [Retro Rewinds WiiLink fork](https://github.com/Retro-Rewind-Team/wfc-server).


