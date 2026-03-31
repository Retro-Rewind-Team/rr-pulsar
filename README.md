# Retro Rewind

Retro Rewind is a custom track distribution created by ZPL. It features every retro track from Super Mario Kart to Mario Kart 7, as well as some Mario Kart 8, Mario Kart Tour and Mario Kart Arcade GP tracks. The distribution also features its own Homebrew application, auto updater and custom online server. As of v6.0, the distribution uses [Pulsar](https://github.com/MelgMKW/Pulsar) as its engine.

## Licensing

The majority of the source code here is licensed under the MIT License.

Including [**mkw-sp**](https://github.com/mkw-sp/mkw-sp) ported features such as (see LICENSE_mkw-sp for more information):

- Input Viewer


However, **certain features are licensed under the AGPLv3** (GNU Affero General Public License v3). If you plan to use these features, the following conditions apply:

- You are allowed to use, distribute, modify, and use the features privately, but must comply with these requirements:
    - Disclose the source code of your project
    - Include the license and display a copyright notice
    - ~~Apply the same license to your project~~
        - We **will not enforce a specific license for your project**. You are free to choose any license, as long as all other requirements are met.
    - Specify the changes made to the original feature

Beware that **Network use is distribution**, it means that Users who interact with the licensed material via network are given the right to receive a copy of the source code.

The following features are licensed under AGPLv3:

- Room kick page and feature implementation
- Player count calculation, display and feature implementation
- Extended Team VS, UI changes, feature implementation, and all code associated
- The specific Discord Rich Presence implementation used by Retro Rewind and code associated
- The battle expansion code from Insane Kart Wii
- Worldwide rank system found in Ranking.cpp
- Battle Elimination implementation
- Expanded VR/BR Rating System
- Course Variant Selection Menu

**NOTE:** You can ask to be fully exempted from these licensing constraints, feel free to contact the code author(s).

The original credits for Pulsar can be found in the Pulsar repository.

# Features
Retro Rewind is based on Pulsar. The original feature list from Pulsar can be found in the Pulsar repository.
Retro Rewinds feature list is always documented on the [Retro Rewind Tockdom page](https://wiki.tockdom.com/wiki/Retro_Rewind). The feature list may be behind the actual features available in the repository.

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


