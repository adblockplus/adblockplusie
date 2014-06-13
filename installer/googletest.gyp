#
# Mostly a copy of googletest.gyp from the project 'libadblockplus'.
# Copied here because it needed modifications to compile without error.
# Because the original is in another project, changing it there would require testing that projects and all others included by it.
#
{
  'variables': 
  {
    'googletest': '../libadblockplus/third_party/googletest'
  },
  'target_defaults': 
  {
    'configurations': 
	{
      'Debug':
	  {
        'defines': [ 'DEBUG' ],
        'msvs_settings':
		{
          'VCCLCompilerTool':
		  {
            'Optimization': '0',
			'RuntimeLibrary': '1',  # /MTd
          },
        },
      },
      'Release':
	  {
        'msvs_settings': 
		{
          'VCCLCompilerTool':
		  {
            'Optimization': '2',
            'InlineFunctionExpansion': '2',
            'EnableIntrinsicFunctions': 'true',
            'FavorSizeOrSpeed': '0',
            'StringPooling': 'true',
			'RuntimeLibrary': '0',  # /MT
          },
        },
      },
    },
    'msvs_configuration_attributes':
	{
      'OutputDirectory': '<(DEPTH)\\$(ConfigurationName)',
      'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
    },
    'conditions': 
	[[
      'target_arch=="x64"', 
	  {
        'msvs_configuration_platform': 'x64',
      },
    ]]
  },
  'targets': 
  [{
    'target_name': 'googletest',
    'type': 'static_library',
    'sources': [
      '<(googletest)/src/gtest-death-test.cc',
      '<(googletest)/src/gtest-filepath.cc',
      '<(googletest)/src/gtest-port.cc',
      '<(googletest)/src/gtest-printers.cc',
      '<(googletest)/src/gtest-test-part.cc',
      '<(googletest)/src/gtest-typed-test.cc',
      '<(googletest)/src/gtest.cc'
    ],
    'include_dirs': [ '<(googletest)', '<(googletest)/include' ],
    'direct_dependent_settings':
	{
      'include_dirs': [ '<(googletest)', '<(googletest)/include' ]
    },
    'defines': [ '_VARIADIC_MAX=10' ],
    'direct_dependent_settings': 
	{
      'defines': [ '_VARIADIC_MAX=10' ],
    },
  },{
    'target_name': 'googletest_main',
    'type': 'static_library',
    'sources': ['<(googletest)/src/gtest_main.cc'],
    'include_dirs': [ '<(googletest)/include' ],
	'dependencies': [ 'googletest' ],
	'direct_dependent_settings': 
	{
      'defines': [ '_VARIADIC_MAX=10' ],
	  'include_dirs': [ '<(googletest)/include' ],
	}
  }]
}
