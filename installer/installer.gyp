{
  'includes': [ '../defaults.gypi' ],

  'variables': 
  {
    # The 'component' variable is required to use 'defaults.gypi'.
	# It's value 'shared_library' is duplicated by the 'type' property of a target.
	# We may want to migrate compiler settings for the CA library at some point and stop using 'defaults.gypi' here
    'component%': '',
  },

  'target_defaults':
  {
    'msvs_cygwin_shell': 0,
  },

  'targets': 
  [
  #############
  # Custom Action library for the installer
  #############
  {
    'target_name': 'installer-ca',
    'type': 'shared_library',
    'component': 'shared_library',
    'sources': 
    [
      #
      # Custom Action
      #
      'src/custom-action/abp_ca.cpp',
      'src/custom-action/abp_ca.def',
      'src/custom-action/abp_ca.rc',
      'src/custom-action/close_application.cpp',
      #
      # Windows Installer library 
      #
      'src/installer-lib/database.cpp', 
      'src/installer-lib/database.h',
      'src/installer-lib/DLL.cpp', 
      'src/installer-lib/DLL.h', 
      'src/installer-lib/interaction.cpp', 
      'src/installer-lib/interaction.h',
      'src/installer-lib/property.cpp', 
      'src/installer-lib/property.h',
      'src/installer-lib/record.cpp', 
      'src/installer-lib/record.h',
      'src/installer-lib/session.cpp', 
      'src/installer-lib/session.h',
    ],
    'include_dirs': 
    [
      'src/installer-lib',
    ],
    'link_settings': 
    {
      'libraries': [ 'user32.lib', 'Shell32.lib', 'advapi32.lib', 'msi.lib', 'Version.lib' ]        
    },
    'msvs_settings': 
    {
      'VCLinkerTool': {}
    }
  },
  ]
}
