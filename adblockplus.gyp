{
  'includes': [
    'defaults.gypi',
    'common/common.gypi',
  ],

  'variables': {
    'build_type%': 'devbuild',
    'build_version%': '',
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
    'target_name': 'shared',
    'type': 'static_library',
    'dependencies': [
      'common',
    ],
    'sources': [
      'src/shared/AutoHandle.cpp',
      'src/shared/AutoHandle.h',
      'src/shared/Communication.cpp',
      'src/shared/Communication.h',
      'src/shared/CriticalSection.h',
      'src/shared/Dictionary.cpp',
      'src/shared/Dictionary.h',
      'src/shared/EventWithSetter.cpp',
      'src/shared/EventWithSetter.h',
      'src/shared/Utils.cpp',
      'src/shared/Utils.h',
      'src/shared/Version.h',
      'src/shared/MsHTMLUtils.cpp',
      'src/shared/MsHTMLUtils.h',
    ],
    'include_dirs': [
      '$(ADBLOCKPLUS_ATL)/include',
    ],
  },

  {
    'target_name': 'AdblockPlusEngine',
    'type': 'executable',
    'dependencies': [
      'shared',
      'libadblockplus/libadblockplus.gyp:libadblockplus',
    ],
    'sources': [
      'src/engine/Main.cpp',
      'src/engine/NotificationWindow.cpp',
      'src/engine/NotificationWindow.h',
      'src/engine/Debug.cpp',
      'src/engine/UpdateInstallDialog.cpp',
      'src/engine/Updater.cpp',
      'src/engine/engine.rc',
    ],
    'include_dirs': [
      '$(ADBLOCKPLUS_ATL)/include',
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
        'conditions': [[
          'target_arch=="ia32"', {
            'AdditionalLibraryDirectories': [
              '$(ADBLOCKPLUS_ATL)/lib',
            ],
          }, {
            'AdditionalLibraryDirectories': [
              '$(ADBLOCKPLUS_ATL)/lib/amd64',
            ],
          }
        ]],
      },
    },
  },

  {
    'target_name': 'AdblockPlus',
    'type': 'shared_library',
    'dependencies': [
      'shared',
      'libadblockplus/libadblockplus.gyp:libadblockplus',
    ],
    'sources': [
      'src/plugin/AdblockPlus.def',
      'src/plugin/AdblockPlus.idl',
      'src/plugin/AdblockPlus.rc',
      'src/plugin/AdblockPlus.rgs',
      'src/plugin/AdblockPlusClient.cpp',
      'src/plugin/AdblockPlusClient.h',
      'src/plugin/AdblockPlusDomTraverser.cpp',
      'src/plugin/AdblockPlusDomTraverser.h',
      'src/plugin/AdblockPlusGuids.h',
      'src/plugin/ATL_Deprecate.cpp',
      'src/plugin/ATL_Deprecate.h',
      'src/plugin/Config.h',
      'src/plugin/NotificationMessage.cpp',
      'src/plugin/NotificationMessage.h',
      'src/plugin/Plugin.cpp',
      'src/plugin/Plugin.h',
      'src/plugin/PluginClass.cpp',
      'src/plugin/PluginClass.h',
      'src/plugin/PluginClientBase.cpp',
      'src/plugin/PluginClientBase.h',
      'src/plugin/PluginClientFactory.cpp',
      'src/plugin/PluginClientFactory.h',
      'src/plugin/PluginDebug.cpp',
      'src/plugin/PluginDebug.h',
      'src/plugin/PluginDomTraverserBase.h',
      'src/plugin/PluginErrorCodes.h',
      'src/plugin/PluginFilter.cpp',
      'src/plugin/PluginFilter.h',
      'src/plugin/PluginMimeFilterClient.cpp',
      'src/plugin/PluginMimeFilterClient.h',
      'src/plugin/PluginMutex.cpp',
      'src/plugin/PluginMutex.h',
      'src/plugin/PluginSettings.cpp',
      'src/plugin/PluginSettings.h',
      'src/plugin/PluginStdAfx.cpp',
      'src/plugin/PluginStdAfx.h',
      'src/plugin/PluginSystem.cpp',
      'src/plugin/PluginSystem.h',
      'src/plugin/PluginTabBase.cpp',
      'src/plugin/PluginTabBase.h',
      'src/plugin/PluginUserSettings.cpp',
      'src/plugin/PluginUserSettings.h',
      'src/plugin/PluginUtil.cpp',
      'src/plugin/PluginUtil.h',
      'src/plugin/PluginWbPassThrough.cpp',
      'src/plugin/PluginWbPassThrough.h',
      'src/plugin/Resource.h',
      'src/plugin/passthroughapp/PassthroughObject.h',
      'src/plugin/passthroughapp/ProtocolCF.h',
      'src/plugin/passthroughapp/ProtocolCF.inl',
      'src/plugin/passthroughapp/ProtocolImpl.h',
      'src/plugin/passthroughapp/ProtocolImpl.inl',
      'src/plugin/passthroughapp/SinkPolicy.h',
      'src/plugin/passthroughapp/SinkPolicy.inl',
    ],
    'include_dirs': [
      '$(ADBLOCKPLUS_ATL)/include',
    ],
    # See "Adding Visual Style Support to an Extension, Plug-in, MMC Snap-in or a DLL
    # That Is Brought into a Process" on the link here:
    # http://msdn.microsoft.com/en-us/library/windows/desktop/bb773175%28v=vs.85%29.aspx#using_manifests
    'defines': ['ISOLATION_AWARE_ENABLED'],
    'libraries': [
      '-lwinhttp',
      '-lshell32',
      '-lComctl32',
      '-lGdi32',
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'conditions': [[
          'target_arch=="ia32"', {
            'AdditionalLibraryDirectories': [
              '$(ADBLOCKPLUS_ATL)/lib',
            ],
          }, {
            'AdditionalLibraryDirectories': [
              '$(ADBLOCKPLUS_ATL)/lib/amd64',
            ],
          }
        ]],
        'DelayLoadDLLs': ['Shell32.dll'],
      },
    },
  },

  {
    'target_name': 'tests',
    'type': 'executable',
    'dependencies': [
      'shared',
      'libadblockplus/third_party/googletest.gyp:googletest_main',
    ],
    'sources': [
      'test/CommunicationTest.cpp',
      'test/DictionaryTest.cpp',
      'test/UtilTest.cpp',
      'test/UtilGetQueryStringTest.cpp',
      'test/UtilGetSchemeAndHierarchicalPartTest.cpp',
    ],
    'defines': ['WINVER=0x0501'],
    'link_settings': {
      'libraries': ['-ladvapi32', '-lshell32', '-lole32', '-loleaut32'],
    },
    'msvs_settings': {
      'VCLinkerTool': {
        'SubSystem': '1', # Console
        'EntryPointSymbol': 'mainCRTStartup',
      },
    },
  },

  {
    'target_name': 'tests_plugin',
    'type': 'executable',
    'dependencies': [
      'shared',
      'libadblockplus/libadblockplus.gyp:libadblockplus',
      'libadblockplus/third_party/googletest.gyp:googletest_main',
    ],
    'sources': [
      'src/plugin/PluginDebug.cpp',
      'test/plugin/DebugTest.cpp',
      'src/plugin/PluginUserSettings.cpp',
      'src/plugin/PluginUserSettings.h',
      'test/plugin/UserSettingsTest.cpp',
      'src/plugin/PluginUtil.h',
      'test/plugin/UtilTest.cpp',
      #
      # required only for linking
      #
      'src/plugin/AdblockPlusClient.cpp',
      'src/plugin/AdblockPlusDomTraverser.cpp',
      'src/plugin/ATL_Deprecate.cpp',
      'src/plugin/NotificationMessage.cpp',
      'src/plugin/Plugin.cpp',
      'src/plugin/PluginClientBase.cpp',
      'src/plugin/PluginClientFactory.cpp',
      'src/plugin/PluginClass.cpp',
      'src/plugin/PluginFilter.cpp',
      'src/plugin/PluginMimeFilterClient.cpp',
      'src/plugin/PluginMutex.cpp',
      'src/plugin/PluginSettings.cpp',
      'src/plugin/PluginSystem.cpp',
      'src/plugin/PluginTabBase.cpp',
      'src/plugin/PluginUtil.cpp',
      'src/plugin/PluginWbPassthrough.cpp',
    ],
    'include_dirs': [
      '$(ADBLOCKPLUS_ATL)/include',
    ],
    'defines': [
      'WINVER=0x0501',
      'PRODUCT_ADBLOCKPLUS'
    ],
    'link_settings': {
      'libraries': ['-ladvapi32', '-lshell32', '-lole32', '-lComctl32', '-lGdi32'],
    },
    'msvs_settings': {
      'VCLinkerTool': {
        'SubSystem': '1', # Console
        'EntryPointSymbol': 'mainCRTStartup',
        'conditions': [[
          'target_arch=="ia32"', {
            'AdditionalLibraryDirectories': [
              '$(ADBLOCKPLUS_ATL)/lib',
            ],
          }, {
            'AdditionalLibraryDirectories': [
              '$(ADBLOCKPLUS_ATL)/lib/amd64',
            ],
          }
        ]],
      },
    },
  }]
}
