#
# Expects command line definition for variable 'target_arch'
#   Must be either 'ia32' or 'x64'
#
# This .gyp file sits in directory 'installer'.
# When gyp translates files locations, base them here.
#
# The solution file from this .gyp source lands in 'installer/build/<(target_arch)'.
# When gyp does not translate file locations, base them here.
#
{
  'includes': [ '../defaults.gypi' ],
  'variables': 
  {
    #
    # Build directories, both common and architecture-specific
    # 
    'build_dir_arch': './build/<(target_arch)',
    'build_dir_common': './build/common', 
    
    #
    # MSI file names.
    # -- The base MSI is a single-language MSI as originally constructed.
    #      This is the one from which all transforms are derived.
    # -- The interim MSI is the working copy of the multilanguage MSI during the build process.
    #      It starts off as a copy of the base MSI.
    #      Transforms are added to the MSI one at a time.
    # -- The final MSI is the ultimate product of the build.
    #      It is simply the last interim MSI, after all the transforms have been embedded.
    #
    'base_msi': '<(build_dir_arch)/adblockplusie-BASE-<(target_arch).msi',
    'interim_msi': '<(build_dir_arch)/adblockplusie-INTERIM-<(target_arch).msi',
    'final_msi': '<(build_dir_arch)/adbblockplusie-multilanguage-<(target_arch).msi',
    
    #
    # WiX installer sources for the compiler, architecture-specific.
    #   The top source is what goes on the command line.
    #   All the sources are inputs.
    # Note that locality sources (.wxl) are not present here because they're handled at link time.
    #
    'installer_source_top_file': 'adblockplusie.wxs',
    'installer_source_files':
    [
      '<(installer_source_top_file)',
      'bho_registry_value.wxi',    
      'dll_class.wxi',
    ],
    'installer_object_file': '<(build_dir_arch)/adblockplusie.wixobj',
    
    #
    # WiX installer sources for the compiler, common to all architectures
    #
    'common_source_files': [ 'custom_WixUI_InstallDir.wxs' ],
    'common_object_file': '<(build_dir_common)/common.wixobj',
    
    #
    # All the assets from the plug-in that are copied into the MSI file. 
    #
    'payloads': [],

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
  # Compile common WiX source.
  #     All the WiX-linked sources that depend neither on architecture nor configuration.
  #     Principally for user interface.
  #############
  {
    'target_name': 'WiX common',
    'type': 'none',
    'actions': 
    [ {
      'action_name': 'WiX compile common',
      'message': 'Compiling common WiX sources',
      'inputs': 
      [
        '<@(common_source_files)'
      ],
      'outputs':
      [
        # List must contain only a single element so that "-out" argument works correctly.
        '<(common_object_file)'
      ], 
      'action':
        [ 'candle -nologo -dNoDefault ', '-out', '<@(_outputs)', '<@(_inputs)' ]
    } ]    
  },
    
  #############
  # Compile installer WiX source.
  #     Platform-specific.
  #############
  {
    'target_name': 'WiX installer',
    'type': 'none',
    'actions':
    [ {
      'action_name': 'Compile WiX installer',
      'message': 'Compilings installer WiX sources',
      'inputs':
      [
        '<@(installer_source_files)'
      ],
      'outputs':
      [
        # List must contain only a single element so that "-out" argument works correctly.
        '<(installer_object_file)'
      ],
      'action':
        [ 'candle -nologo -dNoDefault -dVersion=91.0 -dConfiguration=Release ', '-out', '<@(_outputs)', '<(installer_source_top_file)' ]
    } ]
  },
  
  #############
  # Link WiX objects and payloads, creating base MSI.
  #     Platform-specific.
  #     Generates the reference MSI upon which all transforms are based.
  #############
  {
    'target_name': 'MSI 00. Base',
    'type': 'none',
    'dependencies' : 
    [
      'WiX installer',
      'WiX common',
      'installer-ca'
    ],
    'actions':
    [ {
      'action_name': 'WiX Link',
      'message': 'Linking base MSI',
      'inputs': 
	  [
	    'src\msi\locale\de.wxl',
        '<(payloads)'
      ],
      'outputs':
      [
        '<(base_msi)'
      ],
      'action':
        [ 'light -notidy -nologo -ext WixUIExtension -sval', '-out', '<(base_msi)', '-loc', 'src\msi\locale\de.wxl', '<(installer_object_file)', '<(common_object_file)' ]
    }, {
	  'action_name': 'Copy to interim',
	  'message': 'Copying base MSI to initial version of interim MSI',
	  'inputs': [ '<(base_msi)' ],
	  'outputs': [ '<(interim_msi)' ],
	  'action': [ 'copy', '<(base_msi)', '<(interim_msi)' ]
	} ],
  },

  #############
  # T1. MSI 01. en-us
  #############
  {
    'target_name': 'MSI 01. en',
    'type': 'none',
    'dependencies' : [ 'MSI 00. Base' ],
	'sources':
	[
	  'emb.vbs',
	  'en-us.wxl',
	],
	'actions':
	[ {
      'action_name': 'WiX Link',
      'message': 'Linking en-us MSI',
      'inputs': 
	  [
	    'en-us.wxl',
        '<(payloads)'
      ],
      'outputs':
      [
        '<(build_dir_arch)/adblockplusie-en-us-<(target_arch).msi'
      ],
      'action': [ 'light -notidy -nologo -ext WixUIExtension -sval', '-out', '<@(_outputs)', '-loc', 'en-us.wxl', '<(installer_object_file)', '<(common_object_file)' ]
	}, {
	  'action_name': 'Generate',
	  'message': 'Generating en-us transform',
	  'inputs': [ '<(base_msi)', '<(build_dir_arch)/adblockplusie-en-us-<(target_arch).msi' ],
	  'outputs': [ '<(build_dir_arch)/adblockplusie-en-us-<(target_arch).mst' ],
	  'action': [ 'msitran -g', '<@(_inputs)', '<(_outputs)' ]
	}, {
	  'action_name': 'Embed',
	  'message': 'Embedding en-us MST into interim MSI',
	  'inputs': [ '<(build_dir_arch)/adblockplusie-en-us-<(target_arch).mst' ],
	  'outputs': [ '<(interim_msi)' ],
	  'action': [ 'cscript ..\..\emb.vbs 1033', '<(interim_msi)', '<(_inputs)' ]
	} ]
  },

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
