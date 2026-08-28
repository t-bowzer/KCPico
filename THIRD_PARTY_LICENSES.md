# Third-Party Licenses

KeybChord Pico is built with and depends on the following third-party projects.
Their licenses and copyright notices are reproduced below. This document
accompanies any distribution of the firmware (source or binary).

| Component | Copyright / Author | License | Used by |
|-----------|--------------------|---------|---------|
| arduino-pico core (Earle F. Philhower, III) | Arduino & contributors | LGPL-2.1 | Runtime |
| ArduinoCore-API | Arduino LLC & contributors | LGPL-2.1 | Runtime |
| pico-sdk | Raspberry Pi (Trading) Ltd | BSD-3-Clause | Runtime |
| FreeRTOS-Kernel | Amazon.com, Inc. or its affiliates | MIT | Runtime |
| Adafruit TinyUSB Library | Ha Thach for Adafruit Industries | MIT | Runtime (USB host / MSC) |
| TinyUSB core | hathach | MIT | Runtime (USB stack) |
| FatFs | ChaN | FatFs (permissive) | Runtime (filesystem) |
| Pico PIO USB | sekigon-gonnoc | MIT | Runtime (keyboard host) |
| ArduinoJson | Benoit Blanchon | MIT | Runtime |
| GoogleTest | Google Inc. | BSD-3-Clause | Native tests only |
| MIDI Library (forty7) | Francois Best | MIT | Not linked (transitive) |

---

## GNU Lesser General Public License v2.1

The **arduino-pico core** and **ArduinoCore-API** are licensed under the GNU
Lesser General Public License version 2.1. The full text is available at:

- https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html

The complete source for these components is available from the Arduino project:

- https://github.com/earlephilhower/arduino-pico
- https://github.com/arduino/ArduinoCore-API

This project distributes its own source code (this repository) so that the LGPL
requirements to provide source and permit relinking are satisfied.

---

## MIT License components

The following components are distributed under the MIT License:

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
> THE SOFTWARE.

### FreeRTOS-Kernel
Copyright (C) 2017 Amazon.com, Inc. or its affiliates. Licensed under MIT.

### Adafruit TinyUSB Library
Copyright (c) 2019 Ha Thach for Adafruit Industries. Licensed under MIT.

### TinyUSB core
Copyright (c) 2018 hathach (https://github.com/hathach/tinyusb). Licensed under MIT.

### Pico PIO USB
Copyright (c) 2021 sekigon-gonnoc. Licensed under MIT.

### ArduinoJson
Copyright (c) 2014-2026, Benoit BLANCHON. Licensed under MIT.

### MIDI Library (forty7)
Copyright (c) 2013-2018 Francois Best (https://github.com/FortySevenEffects/arduino_midi_library). Licensed under MIT. This library is not linked into the firmware.

---

## BSD-3-Clause components

### pico-sdk
Copyright 2020 Raspberry Pi (Trading) Ltd.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

### GoogleTest
Copyright 2008, Google Inc. All rights reserved. Licensed under BSD-3-Clause.
Used only for the native test suite and not included in the firmware binary.

---

## FatFs

FatFs - Generic FAT Filesystem Module (R0.15), Copyright (C) 2022, ChaN.
All rights reserved.

FatFs module is an open source software. Redistribution and use of FatFs in
source and binary forms, with or without modification, are permitted provided
that the following condition is met:

1. Redistributions of source code must retain the above copyright notice, this
   condition and the following disclaimer.

This software is provided by the copyright holder and contributors "AS IS" and
any warranties related to this software are DISCLAIMED. The copyright owner or
contributors be NOT LIABLE for any damages caused by use of this software.
