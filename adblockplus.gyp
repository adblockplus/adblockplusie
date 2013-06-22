{
  'includes': ['defaults.gypi'],

  'variables': {
    'build_type%': 'devbuild',
    'build_version%': '',
    'shared_files': [
      'src/shared/AutoHandle.cpp',
      'src/shared/Communication.cpp',
      'src/shared/Dictionary.cpp',
      'src/shared/Utils.cpp',
    ]
  },

  'target_defaults': {
    'conditions': [
      [
        'build_type=="devbuild"',
        {
          'defines': ['ADBLOCK_PLUS_TEST_MODE', 'ADBLOCKPLUS_TEST_MODE'],
        },
        {
          'defines': ['ADBLOCK_PLUS_PRODUCTION_MODE', 'ADBLOCKPLUS_PRODUCTION_MODE'],
        },
      ],
      [
        'build_version!=""',
        {
          'defines': [
            'IEPLUGIN_VERSION=L"<(build_version)"',
            'VERSIONINFO_VERSION=<!(python -c "import sys; print sys.argv[1].replace(\'.\', \',\')" <(build_version).0)',
            'VERSIONINFO_VERSION_STR=\\"<(build_version).0\\"',
          ],
        }
      ],
    ],
  },

  'targets': [{
    'target_name': 'AdblockPlusEngine',
    'type': 'executable',
    'dependencies': [
      'libadblockplus/libadblockplus.gyp:libadblockplus',
    ],
    'sources': [
      'src/engine/main.cpp',
      'src/engine/Debug.cpp',
      'src/engine/Updater.cpp',
      'src/engine/engine.rc',
      '<@(shared_files)',
    ],
    'libraries': [
      '-ladvapi32',
      '-lole32',
      '-luser32',
      '-lshell32',
      '-lshlwapi',
    ],
    'msbuild_toolset': 'v110_xp',
    'msvs_settings': {
      'VCLinkerTool': {
        'DelayLoadDLLs': ['Shell32.dll'],
      },
    },
  },

  {
    'target_name': 'AdblockPlus',
    'type': 'shared_library',
    'sources': [
      'src/plugin/AdblockPlusClient.cpp',
      'src/plugin/AdblockPlusDomTraverser.cpp',
      'src/plugin/AdblockPlusTab.cpp',
      'src/plugin/Plugin.cpp',
      'src/plugin/PluginChecksum.cpp',
      'src/plugin/PluginClass.cpp',
      'src/plugin/PluginClassThread.cpp',
      'src/plugin/PluginClientBase.cpp',
      'src/plugin/PluginClientFactory.cpp',
      'src/plugin/PluginConfiguration.cpp',
      'src/plugin/PluginDebug.cpp',
      'src/plugin/PluginFilter.cpp',
      'src/plugin/PluginHttpRequest.cpp',
      'src/plugin/PluginIniFile.cpp',
      'src/plugin/PluginIniFileW.cpp',
      'src/plugin/PluginMimeFilterClient.cpp',
      'src/plugin/PluginMutex.cpp',
      'src/plugin/PluginSettings.cpp',
      'src/plugin/PluginSha1.cpp',
      'src/plugin/PluginStdAfx.cpp',
      'src/plugin/PluginSystem.cpp',
      'src/plugin/PluginTabBase.cpp',
      'src/plugin/PluginUserSettings.cpp',
      'src/plugin/PluginUtil.cpp',
      'src/plugin/PluginWbPassThrough.cpp',
      'src/plugin/AdblockPlus.def',
      'src/plugin/AdblockPlus.idl',
      'src/plugin/AdblockPlus.rc',
      'src/plugin/AdblockPlus.rgs',
      '<@(shared_files)',
    ],
    'include_dirs': [
      '$(WindowsSDK_IncludePath)',
      '$(VCInstallDir)atlmfc/include',
      '$(WINDDKDIR)/inc/atl71',
    ],
    'defines': ['PRODUCT_ADBLOCKPLUS'],
    'libraries': [
      '-latlthunk',
      '-lwinhttp',
      '-lshell32',
    ],
    'configurations': {
      # 'libraries' is not allowed under 'configurations' :-(
      'Debug': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': ['atlsd.lib'],
          },
        },
      },
      'Release': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': ['atls.lib'],
          },
        },
      },
    },
    'msvs_settings': {
      'VCLinkerTool': {
        'conditions': [[
          'target_arch=="ia32"', {
            'AdditionalLibraryDirectories': [
              '$(WindowsSDK_LibraryPath_x86)',
              '$(WINDDKDIR)/lib/ATL/i386',
            ],
          }, {
            'AdditionalLibraryDirectories': [
              '$(WindowsSDK_LibraryPath_x64)',
              '$(WINDDKDIR)/lib/ATL/amd64',
            ],
          }
        ]],
        'AdditionalLibraryDirectories': [
          '$(VCInstallDir)atlmfc/lib',
        ],
        'DelayLoadDLLs': ['Shell32.dll'],
      },
    },
  },

  {
    'target_name': 'tests',
    'type': 'executable',
    'dependencies': [
      'libadblockplus/third_party/googletest.gyp:googletest_main',
    ],
    'sources': [
      'test/CommunicationTest.cpp',
      'test/DictionaryTest.cpp',
      '<@(shared_files)',
    ],
    'defines': ['WINVER=0x0501'],
    'link_settings': {
      'libraries': ['-ladvapi32', '-lshell32', '-lole32'],
    },
    'msvs_settings': {
      'VCLinkerTool': {
        'SubSystem': '1',   # Console
        'EntryPointSymbol': 'mainCRTStartup',
      },
    },
  }]
}
