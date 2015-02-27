# This file is part of Adblock Plus <https://adblockplus.org/>,
# Copyright (C) 2006-2015 Eyeo GmbH
#
# Adblock Plus is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# Adblock Plus is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 
{
  'targets': [
    {
      'target_name': 'common',
      'type': 'static_library',
      'include_dirs': [
        'include',
      ],
      'sources': [
        'include/Registry.h',
        'src/Registry.cpp',
        'include/IeVersion.h',
        'src/IeVersion.cpp',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
    },
    {
      'target_name': 'common_tests',
      'type': 'executable',
      'dependencies': [
        'common',
        'libadblockplus/third_party/googletest.gyp:googletest_main',
      ],
      'sources': [
        'test/RegistryTest.cpp',
        'test/IeVersionTest.cpp',
      ],
      'defines': ['WINVER=0x0501'],
      'link_settings': {
        'libraries': ['-ladvapi32', '-lshell32', '-lole32'],
      },
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1', # Console
          'EntryPointSymbol': 'mainCRTStartup',
        },
      },
    },
  ]
}
