# Third-party software

Sims 2 DS HD is free software under the GNU General Public License, version 3 ([LICENSE](LICENSE)).
The program is built with the software below; each keeps its own licence, and the notices those
licences ask a binary distribution to carry are quoted here. Every release archive holds this
file, `LICENSE`, `OFL-Nunito.txt` and `LICENSE-SDL_GameControllerDB.txt`. The source of
everything listed is in this repository, its melonDS submodule (`git clone --recursive`) or the
SDL release named in `CMakeLists.txt`.

Nothing of The Sims 2 or of the Nintendo DS's own software is included.

## Built into the program

| Component | What it does here | Licence |
|---|---|---|
| [melonDS](https://github.com/melonDS-emu/melonDS) 1.1 (submodule, plus `patches/melonDS/`) | the DS emulator core | GPLv3 |
| teakra (in melonDS) | the DSi's DSP | MIT (notice below) |
| FatFs (in melonDS) | FAT file systems | 1-clause BSD (notice below) |
| blip_buf (in melonDS) | sound resampling | LGPL 2.1 (`melonDS/src/blip-buf/license.txt`) |
| xxHash (in melonDS) | hashing | BSD 2-Clause (notice below) |
| The DS BIOS replacement by Gilead Kutnick (in melonDS, `FreeBIOS.cpp`) | the DS's BIOS when none is given | BSD-style (notice below) |
| SHA-1 by Steve Reid (in melonDS) | hashing | public domain |
| tiny-AES-c (in melonDS) | AES | The Unlicense (public domain) |
| [SDL2](https://www.libsdl.org/) 2.30 (static) | window, sound, input, controllers | zlib (notice below) |
| HIDAPI (in SDL2) | controllers over HID | used under its GPLv3 option |
| yuv2rgb (in SDL2) | colour conversion | BSD 3-Clause (notice below) |
| fdlibm-derived maths (in SDL2) | maths functions where the system lacks them | Sun's notice (below) |
| [stb_image_write, stb_truetype](https://github.com/nothings/stb) (`src/thirdparty`) | PNG writing, the font's glyphs | public domain (its Alternative B) |
| [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) (`src/thirdparty`) | the file picker | zlib |
| [xBRZ](https://sourceforge.net/projects/xbrz/) 1.8 (`src/thirdparty/xbrz`) | the game pictures' Smooth scaler | GPLv3 (`src/thirdparty/xbrz/LICENSE.txt`) |
| [Nunito](https://github.com/googlefonts/nunito) (`assets/fonts`, embedded) | the HD screens' font | SIL Open Font License 1.1 (`OFL-Nunito.txt`) |
| [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB) (`assets/controllers`, embedded) | controller mappings | zlib (`LICENSE-SDL_GameControllerDB.txt`) |
| [Pokémon FullDex](https://github.com/mamesj/pokemon-fulldex) | the sharper and widescreen 3D (`src/melonds_ext/GPU3D_Wide.*`, `patches/melonDS/`), the sound stretching (`src/audiostretch.h`), the controls (`src/controls.h`), the 5 × 7 font (`src/ui.h`) and the melonDS platform layer (`src/platform.cpp`), adapted | GPLv3 |

The Windows program also links the MinGW-w64 runtime and GCC's runtime libraries statically
(GCC's under the GCC Runtime Library Exception; the MinGW-w64 runtime and winpthreads under the
permissive licences in the MinGW-w64 project's `COPYING` files). The Linux program links GCC's
C++ runtime statically, under the same exception.

## Notices

### teakra

```text
MIT License

Copyright (c) 2018 Weiyi Wang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### FatFs

```text
/*----------------------------------------------------------------------------/
/  FatFs - Generic FAT Filesystem Module  R0.14b                              /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2021, ChaN, all right reserved.
/
/ FatFs module is an open source software. Redistribution and use of FatFs in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/
/----------------------------------------------------------------------------*/
```

### xxHash

```text
Copyright (C) 2012-2023 Yann Collet

BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### The DS BIOS replacement (melonDS `src/FreeBIOS.cpp`)

```text
Custom NDS ARM7/ARM9 BIOS replacement
Copyright (c) 2013, Gilead Kutnick
All rights reserved.

Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

1) Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2) Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

### SDL2

```text
Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>
  
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:
  
1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required. 
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

### yuv2rgb (in SDL2)

```text
Copyright (c) 2016, Adrien Descamps
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of yuv2rgb nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### fdlibm-derived maths functions (in SDL2)

```text
Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

Developed at SunPro, a Sun Microsystems, Inc. business.
Permission to use, copy, modify, and distribute this
software is freely granted, provided that this notice
is preserved.
```
