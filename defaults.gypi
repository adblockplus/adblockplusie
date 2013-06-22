{
  'target_defaults': {
    'configurations': {
      'Debug': {
        'defines': [ '_DEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',

            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '3',  # /MDd
              }, {
                'RuntimeLibrary': '1',  # /MTd
              }]
            ],
          },
        },
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'InlineFunctionExpansion': 2, # AnySuitable
            'EnableIntrinsicFunctions': 'true',
            'EnableFunctionLevelLinking': 'true',
            'FavorSizeOrSpeed': 1,      # Speed
            'OmitFramePointers': 'true',
            'Optimization': 2,          # MaxSpeed
            'RuntimeTypeInfo': 'false',
            'StringPooling': 'true',
            'WholeProgramOptimization': 'true',

            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '2',  # /MD
              }, {
                'RuntimeLibrary': '0',  # /MT
              }]
            ],
          },
          'VCLinkerTool': {
            'EnableCOMDATFolding': 2,   # true
            'OptimizeReferences': 2,    # true
            'LinkTimeCodeGeneration': 1,# UseLinkTimeCodeGeneration
          },
        },
      },
    },
    'defines': ['_WINDOWS'],
    'msvs_configuration_attributes': {
      'CharacterSet': '1',  # Unicode
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
        'WarningLevel': 3,  # Level3
      },
      'VCLinkerTool': {
        'SubSystem': '2',   # Windows
        'GenerateDebugInformation': 'true',
      },
      'VCMIDLTool': {
        'TypeLibraryName': '$(TargetName).tlb',
      },
    },
    'conditions': [
      [
        'target_arch=="x64"', {
          'msvs_configuration_platform': 'x64',
          'defines': ['WIN64'],
        }, {
          'msvs_configuration_platform': 'Win32',
          'defines': ['WIN32'],
        }
      ],
    ],
  },
}
