# Copyright 2018 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
{
  'variables': {
    'common_libs': [
      '-lEGL',
      '-lGLESv2',
      '-lWPEFrameworkCore',
      '-lWPEFrameworkDefinitions',
      '-lWPEFrameworkTracing',
      '-lWPEFrameworkCryptalgo',
      '-lWPEFrameworkPlugins',
      '-lWPEFrameworkProtocols',
      '-lcompositorclient',
      '-lglib-2.0',
      '-lgstreamer-1.0',
      '-lgstaudio-1.0',
      '-lgstvideo-1.0',
      '-lgstapp-1.0',
      '-lgobject-2.0',
      '-locdm',
      '-lpthread',

    ],
    'common_linker_flags': [
      '-Wl,--wrap=eglGetDisplay',
    ],
  },
}