{
  'includes': ['libadblockplus/common.gypi', 'libadblockplus/third_party/googletest.gyp'],

  'targets': [{
    'target_name': 'tests',
    'type': 'executable',
    'dependencies': [
      'googletest_main',
    ],
    'sources': [
      'src/shared/AutoHandle.cpp',
      'src/shared/Communication.cpp',
      'src/shared/Dictionary.cpp',
      'src/shared/Utils.cpp',
      'test/CommunicationTest.cpp',
      'test/DictionaryTest.cpp',
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
