# Retro Rewind

Retro Rewind is a custom track distribution created by ZPL. It features every retro track from Super Mario Kart to Mario Kart 7, as well as some Mario Kart 8, Mario Kart Tour and Mario Kart Arcade GP tracks. The distribution also features its own Homebrew application, auto updater and custom online server. As of v6.0, the distribution uses Pulsar as its engine.

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
- Item Rain code found in ItemRain.cpp
- Worldwide rank system found in Ranking.cpp

**NOTE:** You can ask to be fully exempted from these licensing constraints, feel free to contact the code author(s)

# Pulsar

Pulsar is a Mario Kart Wii Kamek-Based engine to create CT distributions. It comes with its own [software](../main/PulsarPackCreator/Executable) to aid in building custom distributions, and multiple quality of life features:

Core:
- Cup select expansion
- Settings that are directly modifiable in-game, including in friend rooms
- Up to four time-trial modes (150cc, 150cc feather, 200cc and 200cc feather)
- Ghost saving on all tracks and all four modes (on the SD on console and on the NAND on dolphin)
- Support for staff ghosts should the creator make them
- KO mode
- OnlineTT mode
- LEX support
- [XPF support](https://github.com/Gabriela-Orzechowska/LE-CODE-XPF) (from Gabriela)
- [USB GCN Support](https://github.com/Gabriela-Orzechowska/MKW-Cosmos/blob/main/code/System/WUP028.hpp) (from Gabriela)


UI:
- A speedometer that is flush with the game UI
- In-game crediting of track authors
- Between Races Change Combo, which has its own UI along with a randomize button
- Team selection, where the host of a room can manually set the team of each player. Moreover, team VS has been edited to play exactly as normal VS while keeping the coloured minimap icons and the scoreboard after races.
- Boot in wiimmfi directly
- Better ghost replay which allows multi ghost watching and point-of-view switching


Sound:
- BRSAR entry size patch to make all brstms loop normally
- Conditionnal channel switches; the game will only switch channel (on Koopa Cape for example) if the currently playing brstm has at least as many channels as the brsar entry requires.
- BRSTM Volume, much like it works in CTGP by editing byte 0x3F of any BRSTM
- BRSTM expansion
- Optional Music speedUp on final lap (the music is sped up when you cross the line instead of switching to the fast lap version)


Gameplay:
- 200cc support
- Ultra Mini-Turbos
- Mega TCs
- CLF78 and stebler's feather
- Support for custom CC distribution
- COOB (both kHacker35000vr's and Riidefi's versions)


Network:
- Rooms that can only be joined by people on the same pack (including the same version)
- A much faster Host Always Wins where the host selects the next track directly in the race.
- Worldwides that work as on vanilla
- The features that impact gameplay the most (200cc, UMTs, feather, mega TCs) can be turned off in the software when making your distribution. Pulsar can also be used as a base to add your own features. CTTP is such an example. The software only outputs the tracks and a config file, but the code binaries can be modified to fit your needs.

# Building

A makefile is available at the root of the repository. The `CFLAGS` environment variable can be defined to add additional arguments. The following switches are available to control the WFC domain to connect to and the keys used.

| Switch         | Domain           |
| -------------- | ---------------- |
| -DPROD         | rwfc.net         |
| -DTEST         | zpltest.xyz      |
| None Specified | nwfc.wiinoma.com |

When using nwfc.wiinoma.com for a local testing server, the default key is used, which can be found [here](https://github.com/Retro-Rewind-Team/wfc-patcher-wii/blob/main/misc/private-key-DEFAULT.pem)

`CC` and `KAMEK` can both be set in a `.env` file to specify the location of your copy of mwcceppc and Kamek respectively.

Credits:
- Treeki for [Kamek](https://github.com/Treeki/Kamek/tree/master). The engine has been ever so slightly modified to create a new output format which combines the usual 4 binaries. 
