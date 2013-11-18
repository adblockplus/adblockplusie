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
    'build_dir_arch': 'build/<(target_arch)',
    'build_dir_common': 'build/common', 
    
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
	'variables': {
	  # 
	  # We don't really want a default 'locale_id', but we need one to avoid an "undefined variable" error when the ".wxl" rule is invoked.
	  # Note that the action in the rule uses later-phase substitution with ">", which occurs after the rule is merged with the target.
	  # Apparently, though, it's also being evaluated earlier, before 'locale_id' is defined in the target.
	  # Therefore, count this as a workaround for a gyp defect.
	  #
	  'locale_id%': '0',

	  #
	  # We do want a default 'msi_build_phase', because in all but the first MSI build we want the flag "additional"
	  #
	  'msi_build_phase%': 'additional',
	},
    'rules': 
	[ {
	  #
	  # Rule to build a single-language MSI as part of a chain to create a multiple-language MSI
	  # The rule runs a  .cmd file to execute the commands; this chose arises from gyp limitations and defects.
	  #
	  # gyp can only handle a single rule per extension.
	  # Since we have only one ".wxl" file, we need to run all the operations (link MSI, diff to MST, embed MST into MSI) with a single action.
	  # gyp does not have syntax for multi-line actions.
	  # Including a newline as a token doesn't work because of the way gyp "fixes" path names; it treats the newline as a path, prefixes it, and quotes it all.
	  #
	  # Furthermore, there's the issue of overriding the rule for the first MSI, the one that generates the BASE against which transforms are generated.
	  # In order to override the rule, we'd need to duplicate most of this one, particularly all the file name expressions, violating the write-once principle.
	  #
	  'rule_name': 'MSI Build',
	  'extension': 'wxl',
      'message': 'Generating embedded transform for "<(RULE_INPUT_ROOT)"',
	  'inputs': [ 'emb.vbs', '<(base_msi)', '<@(payloads)' ],
	  'outputs': [ '<(build_dir_arch)/adblockplusie-<(RULE_INPUT_ROOT)-<(target_arch).msi', '<(build_dir_arch)/adblockplusie-<(RULE_INPUT_ROOT)-<(target_arch).mst' ],
      'action': 
	  [
	    '..\..\msibuild.cmd >(msi_build_phase) >(locale_id)', '<(RULE_INPUT_PATH)', 
		'<(build_dir_arch)/adblockplusie-<(RULE_INPUT_ROOT)-<(target_arch).msi',
		'<(build_dir_arch)/adblockplusie-<(RULE_INPUT_ROOT)-<(target_arch).mst',
		'<(build_dir_arch)/adblockplusie-BASE-<(target_arch).msi',
		'<(build_dir_arch)/adblockplusie-INTERIM-<(target_arch).msi',
		'<(installer_object_file)', '<(common_object_file)',
	  ]
	} ],
  },

  'targets': 
  [
  #############
  # Compile common WiX source.
  #     All the WiX-linked sources that depend neither on architecture nor configuration.
  #     Principally for user interface.
  #############
  {
    'target_name': 'Installer, common WiX',
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
    'target_name': 'Installer, architecture-specific WiX',
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
    'target_name': 'MSI 0000. de (BASE)',
    'type': 'none',
    'dependencies' : 
    [
      'Installer, architecture-specific WiX',
      'Installer, common WiX',
      'installer-ca'
    ],
	'variables': {
	  'msi_build_phase': 'initial',
	  'locale_id': '7',
	},
	'sources': [ 'src\msi\locale\de.wxl' ],
  },

  #############
  # T1. MSI 1033. en-us
  #############
  {
    'target_name': 'MSI 1033. en-us',
    'type': 'none',
    'dependencies' : [ 'MSI 0000. de (BASE)' ],
	'variables': { 'locale_id': '1033' },
	'sources': [ 'en-us.wxl' ],
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
