// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_DECODE_TARGET_CREATE_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_DECODE_TARGET_CREATE_H_

#include "starboard/decode_target.h"

namespace starboard {
namespace wpe {
namespace shared {
namespace open_max {

SbDecodeTarget DecodeTargetCreate(
    SbDecodeTargetGraphicsContextProvider* provider,
    SbDecodeTargetFormat format,
    int width,
    int height);

}  // namespace open_max
}  // namespace shared
}  // namespace wpe
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_DECODE_TARGET_CREATE_H_