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
    'installer_source_top_file': 'src/msi/adblockplusie.wxs',
    'installer_source_files':
    [
      '<(installer_source_top_file)',
      'src/msi/bho_registry_value.wxi',    
      'src/msi/dll_class.wxi',
    ],
    'installer_object_file': '<(build_dir_arch)/adblockplusie.wixobj',
    
    #
    # WiX installer sources for the compiler, common to all architectures
    #
    'common_source_files': [ 'src/msi/custom_WixUI_InstallDir.wxs' ],
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
      'message': 'Compiling installer WiX sources',
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

  ##################################
  # MSI targets
  # 
  # Building a multiple-language MSI requires embedding a transform for each language into a single MSI database.
  # Each step requires a locale identifier (Microsoft LCID) as a parameter and a WiX localization file (.wxl) as a source.
  # gyp does not support per-source-file parameters, so we're stuck with one project per step.
  # The naming convention for projects:
  # - The token "MSI". Projects appear in dictionary order in the resulting solution file. A common initial token groups them.
  # - The language tag. These are mostly just the two-letter language codes. There are a few sublanguage tags, though.
  # - The LCID of the language in four digit hexadecimal form, zero-padded if necessary.
  #     Note: This supports Traditional Chinese (used in Taiwan) with LCID 0x7C04.
  # Exception: The BASE MSI is named so that it appears first.
  #
  # These steps are arranged as a linked list, starting with the BASE version of the MSI,
  #   which is simply a single-language MSI whose language will be the default language for the final installer.
  # The list is singly-linked by the 'dependencies' element.
  # For sanity (and code audit), the project declarations appear below in the same order as they will appear in the compiled solution file.
  #
  # The naming convention for WiX localization files (.wxl) is a combination of the language ID and the sublanguage ID.
  # It can be thought of as a text representation of the LCID as the Windows API sees it.
  # Mostly these agree with IETF-style two-part identifiers, but they're not always the same.
  # For many languages, there is only a single sublanguage defined; these languages still use a two-part name for consistency.
  # Generic languages, that is, those with no sublanguage ID (it's zero), use names of only a single part.
  # The generic languages currently used are these: ar, de, en, fr, it, ms, nl.
  # We currently don't have generic "es" (Spanish), nor a specific "es-MX" (Spanish - Mexico).
  # 
  # Adding a new language consists of three steps.
  # 1. Create a '.wxl' localization file in 'src\msi\locale'.
  #      This file contains an XML element <String Id="LANG"> that specifies the LCID.
  #      Set the codepage element also, if needed.
  # 2. In the <Package> element inside 'adblockplusie.wxs', the attribute "languages" contains a comma-separated list of all languages supported by the installer.
  #      Add the LCID to this list.
  #      If this isn't done, the build will succeed but the embedded transform will be ignored at run-time.
  # 3. Create a target for the language.
  #      Define the gyp variable 'locale_id' as the LCID.
  #      Add the target to the linked project list by setting the gyp variable 'dependencies' of both the new project and the one following it.
  #
  # Reference: MSDN "Language Identifier Constants and Strings" http://msdn.microsoft.com/en-us/library/dd318693%28v=vs.85%29.aspx
  # Many languages have only a single sublanguage. For these, we use the sublanguage-specific LCID (usually starting with 0x04).
  # For languages with more than one sublanguage (English, German, French, etc.), we use the generic LCID (sublanguage code equals zero).
  # Exception: Spanish is currently es-ES, and the sublanguage-specific LCID is used.
  #
  # We use Alpha-3 codes (from ISO 693-2): fil.
  # Filipino (fil) doesn't have a two-letter code.
  #
  # Warning: The Windows Installer still (as of 2013) does not fully support Unicode.
  # Strings remain encoded by code page specification.
  # Certain languages are Unicode-only, such as Hindi (hi), do not have code page assignments.
  # Such languages might not work if localized, either partially or completely.
  # These languages _need_ testing before publication, not just a wish and a prayer.
  #
  # Warning Continued: The issue is that Win32 ANSI entry points (those ending in "A") will fail for such languages.
  # Wide-character entry points (those ending in "W") should work.
  # The issue is that much of the interior of the installer is opaque, and it's not possible to know if any ANSI calls remain enabled.
  # The WiX user interface code (generally) uses wide characters, but it calls some default error notifications that may not behave correctly.
  #
  # Another warning, the .wxl files are all XML files, whose declared encoding is "utf-8".
  # This encoding may need to be changed for certain files to ensure that character input is correct.
  #
  # Reference: MSDN "Code Page Identifiers" http://msdn.microsoft.com/en-us/library/dd317756%28VS.85%29.aspx
  #
  ##################################
  #############
  # Link WiX objects and payloads, creating base MSI.
  #     Platform-specific.
  #     Generates the reference MSI upon which all transforms are based.
  #############
  {
    'target_name': 'MSI @ de 7 (German) [BASE]',
    'type': 'none',
    'dependencies' : 
    [
      'Installer, architecture-specific WiX',
      'Installer, common WiX',
      'installer-ca'
    ],
	'variables': {
	  # Only define 'msi_build_phase' once as 'initial', here in the BASE target. All others use the default value.
	  'msi_build_phase': 'initial',
	  'locale_id': '7',
	},
	'sources': [ 'src/msi/locale/de.wxl' ],
  },

  #############
  # MSI ar 1 (Arabic - generic)
  #############
  {
    'target_name': 'MSI ar 1 (Arabic - generic)',
    'type': 'none',
    'dependencies' : [ 'MSI @ de 7 (German) [BASE]' ],
	'variables': { 'locale_id': '1' },
	'sources': [ 'src/msi/locale/ar.wxl' ],
  },

  #############
  # MSI bg-BG 1026 (Bulgarian - Bulgaria)
  #############
  {
    'target_name': 'MSI bg-BG 1026 (Bulgarian - Bulgaria)',
    'type': 'none',
    'dependencies' : [ 'MSI ar 1 (Arabic - generic)' ],
	'variables': { 'locale_id': '1026' },
	'sources': [ 'src/msi/locale/bg-BG.wxl' ],
  },

  #############
  # MSI ca-ES 1027 (Catalan - Spain)
  #############
  {
    'target_name': 'MSI ca-ES 1027 (Catalan - Spain)',
    'type': 'none',
    'dependencies' : [ 'MSI bg-BG 1026 (Bulgarian - Bulgaria)' ],
	'variables': { 'locale_id': '1027' },
	'sources': [ 'src/msi/locale/ca-ES.wxl' ],
  },

  #############
  # MSI cs-CZ 1029 (Czech - Czech Republic)
  #############
  {
    'target_name': 'MSI cs-CZ 1029 (Czech - Czech Republic)',
    'type': 'none',
    'dependencies' : [ 'MSI ca-ES 1027 (Catalan - Spain)' ],
	'variables': { 'locale_id': '1029' },
	'sources': [ 'src/msi/locale/cs-CZ.wxl' ],
  },

  #############
  # MSI da-DK 1030 (Danish - Denmark)
  #############
  {
    'target_name': 'MSI da-DK 1030 (Danish - Denmark)',
    'type': 'none',
    'dependencies' : [ 'MSI cs-CZ 1029 (Czech - Czech Republic)' ],
	'variables': { 'locale_id': '1030' },
	'sources': [ 'src/msi/locale/da-DK.wxl' ],
  },

  #############
  # MSI el-GR 1032 (Greek - Greece)
  #############
  {
    'target_name': 'MSI el-GR 1032 (Greek - Greece)',
    'type': 'none',
    'dependencies' : [ 'MSI da-DK 1030 (Danish - Denmark)' ],
	'variables': { 'locale_id': '1032' },
	'sources': [ 'src/msi/locale/el-GR.wxl' ],
  },

  #############
  # MSI en 9 (English - generic)
  #############
  {
    'target_name': 'MSI en 9 (English - generic)',
    'type': 'none',
    'dependencies' : [ 'MSI el-GR 1032 (Greek - Greece)' ],
	'variables': { 'locale_id': '9' },
	'sources': [ 'src/msi/locale/en.wxl' ],
  },

  #############
  # MSI es-ES 1034 (Spanish - Spain)
  #############
  {
    'target_name': 'MSI es-ES 1034 (Spanish - Spain)',
    'type': 'none',
    'dependencies' : [ 'MSI en 9 (English - generic)' ],
	'variables': { 'locale_id': '1034' },
	'sources': [ 'src/msi/locale/es-ES.wxl' ],
  },

  #############
  # MSI et-EE 1061 (Estonian - Estonia)
  #############
  {
    'target_name': 'MSI et-EE 1061 (Estonian - Estonia)',
    'type': 'none',
    'dependencies' : [ 'MSI es-ES 1034 (Spanish - Spain)' ],
	'variables': { 'locale_id': '1061' },
	'sources': [ 'src/msi/locale/et-EE.wxl' ],
  },

  #############
  # MSI fi 1035 (Finnish - Finland)
  #############
  {
    'target_name': 'MSI fi 1035 (Finnish - Finland)',
    'type': 'none',
    'dependencies' : [ 'MSI et-EE 1061 (Estonian - Estonia)' ],
	'variables': { 'locale_id': '1035' },
	'sources': [ 'src/msi/locale/fi-FI.wxl' ],
  },

  #############
  # MSI fil-PH 1124 (Filipino - Philippines)
  #############
  {
    'target_name': 'MSI fil-PH 1124 (Filipino - Philippines)',
    'type': 'none',
    'dependencies' : [ 'MSI fi 1035 (Finnish - Finland)' ],
	'variables': { 'locale_id': '1124' },
	'sources': [ 'src/msi/locale/fil-PH.wxl' ],
  },

  #############
  # MSI fr 12 (French - generic)
  #############
  {
    'target_name': 'MSI fr 12 (French - generic)',
    'type': 'none',
    'dependencies' : [ 'MSI fil-PH 1124 (Filipino - Philippines)' ],
	'variables': { 'locale_id': '12' },
	'sources': [ 'src/msi/locale/fr.wxl' ],
  },

  #############
  # MSI he-IL 1037 (Hebrew - Israel)
  #############
  {
    'target_name': 'MSI he-IL 1037 (Hebrew - Israel)',
    'type': 'none',
    'dependencies' : [ 'MSI fr 12 (French - generic)' ],
	'variables': { 'locale_id': '1037' },
	'sources': [ 'src/msi/locale/he-IL.wxl' ],
  },

  #############
  # MSI hi-IN 1081 (Hindi - India)
  #############
  {
    'target_name': 'MSI hi-IN 1081 (Hindi - India)',
    'type': 'none',
    'dependencies' : [ 'MSI he-IL 1037 (Hebrew - Israel)' ],
	'variables': { 'locale_id': '1081' },
	'sources': [ 'src/msi/locale/hi-IN.wxl' ],
  },

  #############
  # MSI hr-HR 1050 (Croatian - Croatia)
  #############
  {
    'target_name': 'MSI hr-HR 1050 (Croatian - Croatia)',
    'type': 'none',
    'dependencies' : [ 'MSI hi-IN 1081 (Hindi - India)' ],
	'variables': { 'locale_id': '1050' },
	'sources': [ 'src/msi/locale/hr-HR.wxl' ],
  },

  #############
  # MSI hu-HU 1038 (Hungarian - Hungary)
  #############
  {
    'target_name': 'MSI hu-HU 1038 (Hungarian - Hungary)',
    'type': 'none',
    'dependencies' : [ 'MSI hr-HR 1050 (Croatian - Croatia)' ],
	'variables': { 'locale_id': '1038' },
	'sources': [ 'src/msi/locale/hu-HU.wxl' ],
  },

  #############
  # MSI it 16 (Italian - generic)
  #############
  {
    'target_name': 'MSI it 16 (Italian - generic)',
    'type': 'none',
    'dependencies' : [ 'MSI hu-HU 1038 (Hungarian - Hungary)' ],
	'variables': { 'locale_id': '16' },
	'sources': [ 'src/msi/locale/it.wxl' ],
  },

  #############
  # MSI ja-JP 1041 (Japanese - Japan)
  #############
  {
    'target_name': 'MSI ja-JP 1041 (Japanese - Japan)',
    'type': 'none',
    'dependencies' : [ 'MSI it 16 (Italian - generic)' ],
	'variables': { 'locale_id': '1041' },
	'sources': [ 'src/msi/locale/ja-JP.wxl' ],
  },

  #############
  # MSI kn-IN 1099 (Kannada - India)
  #############
  {
    'target_name': 'MSI kn-IN 1099 (Kannada - India)',
    'type': 'none',
    'dependencies' : [ 'MSI ja-JP 1041 (Japanese - Japan)' ],
	'variables': { 'locale_id': '1099' },
	'sources': [ 'src/msi/locale/kn-IN.wxl' ],
  },

  #############
  # MSI mr-IN 1102 (Marathi - India)
  #############
  {
    'target_name': 'MSI mr-IN 1102 (Marathi - India)',
    'type': 'none',
    'dependencies' : [ 'MSI kn-IN 1099 (Kannada - India)' ],
	'variables': { 'locale_id': '1102' },
	'sources': [ 'src/msi/locale/mr-IN.wxl' ],
  },

  #############
  # MSI ms 62 (Malay - generic)
  #############
  {
    'target_name': 'MSI ms 62 (Malay - generic)',
    'type': 'none',
    'dependencies' : [ 'MSI mr-IN 1102 (Marathi - India)' ],
	'variables': { 'locale_id': '62' },
	'sources': [ 'src/msi/locale/ms.wxl' ],
  },

  #############
  # MSI nb-NO 1044 (Norwegian - Bokmål, Norway)
  #   Target name has a vowel change to work around a character encoding problem in gyp/MSVS.
  #############
  {
    'target_name': 'MSI nb-NO 1044 (Norwegian - Bokmal, Norway)',
    'type': 'none',
    'dependencies' : [ 'MSI ms 62 (Malay - generic)' ],
	'variables': { 'locale_id': '1044' },
	'sources': [ 'src/msi/locale/nb-NO.wxl' ],
  },

  #############
  # MSI nl 19 (Dutch - generic)
  #############
  {
    'target_name': 'MSI nl 19 (Dutch - generic)',
    'type': 'none',
    'dependencies' : [ 'MSI nb-NO 1044 (Norwegian - Bokmal, Norway)' ],
	'variables': { 'locale_id': '19' },
	'sources': [ 'src/msi/locale/nl.wxl' ],
  },

  #############
  # MSI nn-NO 2068 (Norwegian - Nynorsk, Norway)
  #############
  {
    'target_name': 'MSI nn-NO 2068 (Norwegian - Nynorsk, Norway)',
    'type': 'none',
    'dependencies' : [ 'MSI nl 19 (Dutch - generic)' ],
	'variables': { 'locale_id': '2068' },
	'sources': [ 'src/msi/locale/nn-NO.wxl' ],
  },

  #############
  # MSI pl-PL 1045 (Polish - Poland)
  #############
  {
    'target_name': 'MSI pl-PL 1045 (Polish - Poland)',
    'type': 'none',
    'dependencies' : [ 'MSI nn-NO 2068 (Norwegian - Nynorsk, Norway)' ],
	'variables': { 'locale_id': '1045' },
	'sources': [ 'src/msi/locale/pl-PL.wxl' ],
  },

  #############
  # MSI pt-BR 1046 (Portuguese - Brazil)
  #############
  {
    'target_name': 'MSI pt-BR 1046 (Portuguese - Brazil)',
    'type': 'none',
    'dependencies' : [ 'MSI pl-PL 1045 (Polish - Poland)' ],
	'variables': { 'locale_id': '1046' },
	'sources': [ 'src/msi/locale/pt-BR.wxl' ],
  },

  #############
  # MSI pt-PT 2070 (Portuguese - Portugal)
  #############
  {
    'target_name': 'MSI pt-PT 2070 (Portuguese - Portugal)',
    'type': 'none',
    'dependencies' : [ 'MSI pt-BR 1046 (Portuguese - Brazil)' ],
	'variables': { 'locale_id': '2070' },
	'sources': [ 'src/msi/locale/pt-PT.wxl' ],
  },

  #############
  # MSI ro-RO 1048 (Romanian - Romania)
  #############
  {
    'target_name': 'MSI ro-RO 1048 (Romanian - Romania)',
    'type': 'none',
    'dependencies' : [ 'MSI pt-PT 2070 (Portuguese - Portugal)' ],
	'variables': { 'locale_id': '1048' },
	'sources': [ 'src/msi/locale/ro-RO.wxl' ],
  },

  #############
  # MSI ru-RU 1049 (Russian - Russia)
  #############
  {
    'target_name': 'MSI ru-RU 1049 (Russian - Russia)',
    'type': 'none',
    'dependencies' : [ 'MSI ro-RO 1048 (Romanian - Romania)' ],
	'variables': { 'locale_id': '1049' },
	'sources': [ 'src/msi/locale/ru-RU.wxl' ],
  },

  #############
  # MSI sk-SK 1051 (Slovak - Slovakia)
  #############
  {
    'target_name': 'MSI sk-SK 1051 (Slovak - Slovakia)',
    'type': 'none',
    'dependencies' : [ 'MSI ru-RU 1049 (Russian - Russia)' ],
	'variables': { 'locale_id': '1051' },
	'sources': [ 'src/msi/locale/sk-SK.wxl' ],
  },

  #############
  # MSI sv-SE 1053 (Swedish - Sweden)
  #############
  {
    'target_name': 'MSI sv-SE 1053 (Swedish - Sweden)',
    'type': 'none',
    'dependencies' : [ 'MSI sk-SK 1051 (Slovak - Slovakia)' ],
	'variables': { 'locale_id': '1053' },
	'sources': [ 'src/msi/locale/sv-SE.wxl' ],
  },

  #############
  # MSI th-TH 1054 (Thai - Thailand)
  #############
  {
    'target_name': 'MSI th-TH 1054 (Thai - Thailand)',
    'type': 'none',
    'dependencies' : [ 'MSI sv-SE 1053 (Swedish - Sweden)' ],
	'variables': { 'locale_id': '1054' },
	'sources': [ 'src/msi/locale/th-TH.wxl' ],
  },

  #############
  # MSI tr-TR 1055 (Turkish - Turkey)
  #############
  {
    'target_name': 'MSI tr-TR 1055 (Turkish - Turkey)',
    'type': 'none',
    'dependencies' : [ 'MSI th-TH 1054 (Thai - Thailand)' ],
	'variables': { 'locale_id': '1055' },
	'sources': [ 'src/msi/locale/tr-TR.wxl' ],
  },

  #############
  # MSI uk-UA 1058 (Ukrainian - Ukraine)
  #############
  {
    'target_name': 'MSI uk-UA 1058 (Ukrainian - Ukraine)',
    'type': 'none',
    'dependencies' : [ 'MSI tr-TR 1055 (Turkish - Turkey)' ],
	'variables': { 'locale_id': '1058' },
	'sources': [ 'src/msi/locale/uk-UA.wxl' ],
  },

  #############
  # MSI ur-PK 1056 (Urdu - Pakistan)
  #############
  {
    'target_name': 'MSI ur-PK 1056 (Urdu - Pakistan)',
    'type': 'none',
    'dependencies' : [ 'MSI uk-UA 1058 (Ukrainian - Ukraine)' ],
	'variables': { 'locale_id': '1056' },
	'sources': [ 'src/msi/locale/ur-PK.wxl' ],
  },

  #####################
  # Note: The locale codes for Chinese differ between the usage in the .NET library and the Windows OS.
  #   Mostly these are the same, but there are some places where LCID's are listed that use the .NET values.
  #   The Windows Installer is a laggard in i18n issues, so we're taking the precautionary approach to use Windows API values for the LCID.
  #   The .NET version has the notion of culture hierarchies, an invariant culture, and a neutral culture.
  #   The .NET neutral culture ID for Traditional Chinese is 0x7C04, but this is not supported in the Windows API.
  #   The .NET neutral culture ID for Simplified Chinese 0x0004, but this is the neutral/invariant LCID for the Windows API.
  #   As a result, we're using sublanguage codes 0x01 and 0x02 for Taiwan and China, respectively, in the LCID's below.
  #####################
  #############
  # MSI zh-CN 2052 (Chinese - China)
  #############
  {
    'target_name': 'MSI zh-CN 2052 (Chinese - China)',
    'type': 'none',
    'dependencies' : [ 'MSI ur-PK 1056 (Urdu - Pakistan)' ],
	'variables': { 'locale_id': '2052' },
	'sources': [ 'src/msi/locale/zh-CN.wxl' ],
  },

  #############
  # MSI zh-TW 1028 (Chinese - Taiwan)
  #############
  {
    'target_name': 'MSI zh-TW 1028 (Chinese - Taiwan)',
    'type': 'none',
    'dependencies' : [ 'MSI zh-CN 2052 (Chinese - China)' ],
	'variables': { 'locale_id': '1028' },
	'sources': [ 'src/msi/locale/zh-TW.wxl' ],
  },

  ##################################
  # END of MSI section
  ##################################

  #############
  # Custom Action DLL for the installer
  #############
  {
    'target_name': 'installer-ca',
    'type': 'shared_library',
	'dependencies':  [ 'installer-library' ],
    'sources': 
    [
      'src/custom-action/abp_ca.cpp',
      'src/custom-action/abp_ca.def',
      'src/custom-action/abp_ca.rc',
      'src/custom-action/close_application.cpp',
      'src/custom-action/close_ie.wxi',
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

  #############
  # Windows Installer library
  #############
  {
    'target_name': 'installer-library',
    'type': 'static_library',
    'sources': 
    [
	  'src/installer-lib/custom-i18n.cpp',
	  'src/installer-lib/custom-i18n.h',
	  'src/installer-lib/custom-i18n.wxi',
      'src/installer-lib/database.cpp', 
      'src/installer-lib/database.h',
      'src/installer-lib/DLL.cpp', 
      'src/installer-lib/DLL.h', 
      'src/installer-lib/handle.h', 
      'src/installer-lib/interaction.cpp', 
      'src/installer-lib/interaction.h',
      'src/installer-lib/process.cpp', 
      'src/installer-lib/process.h',
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
	'direct_dependent_settings':
	{
	  'include_dirs': 
	  [
	    'src/installer-lib',
	  ],
	},
    'link_settings': 
    {
      'libraries': [ 'user32.lib', 'Shell32.lib', 'advapi32.lib', 'msi.lib', 'Version.lib' ]        
    },
    'msvs_settings': 
    {
      'VCLinkerTool': {}
    }
  },
  
  #############
  # Custom actions for library test MSI
  #############
  {
    'target_name': 'installer-library-test-customactions',
    'type': 'shared_library',
    'dependencies':
	[
	  'installer-library',
    ],
	'sources': 
	[
	  'src/installer-lib/test/test-installer-lib-ca.cpp',
      'src/installer-lib/test/test-installer-lib-ca.def',
      'src/installer-lib/test/test-installer-lib-ca.rc',
      'src/installer-lib/test/test-installer-lib-sandbox.cpp',
      'src/installer-lib/test/custom-action-fail.cpp',
	  'src/custom-action/close_application.cpp',	
	],
  },

  #############
  # WiX compile for library test MSI
  #############
  {
    'target_name': 'installer-library-test-wix',
    'type': 'none',
	'sources': 
	[
	  'src/installer-lib/test/test-installer-lib.wxs',
	  'src/installer-lib/custom-i18n.wxi',
    ],
    'actions': 
    [ {
      'action_name': 'WiX compile',
      'message': 'Compiling WiX source',
      'inputs': 
      [
        'src/installer-lib/test/test-installer-lib.wxs'
      ],
      'outputs':
      [
        '<(build_dir_arch)/test-installer-lib.wixobj'
      ],
      'action':
        [ 'candle -nologo -dNoDefault ', '-out', '<@(_outputs)', '<@(_inputs)' ]
    } ]
  },

  #############
  # WiX link for library test MSI
  #############
  {
    'target_name': 'installer-library-test-msi',
    'type': 'none',
    'dependencies':
	[
      'installer-library-test-customactions',
      'installer-library-test-wix',
    ],
	'sources': 
	[
	  '<(build_dir_arch)/test-installer-lib.wixobj',
	],
	'actions':
	[ {
	  'action_name': 'WiX link',
	  'message': 'Linking WiX objects',
	  'linked_inputs':
	  [
        '<(build_dir_arch)/test-installer-lib.wixobj',
	  ],
	  'localization_input':
	  [
		'src/custom-action/close_ie_default.wxl',			# Keep the .WXL file out of 'sources', since otherwise the custom rule will kick in
	  ],
	  'inputs': 
	  [
		'<@(_linked_inputs)',
		'<@(_localization_input)',
		'src/custom-action/close_ie.wxi',
		'<(build_dir_arch)/Debug/installer-library-test-customactions.dll'
	  ],
	  'outputs': 
	  [
	    '<(build_dir_arch)/test-installer-lib.msi'
	  ],
	  'action':
	    # ICE71: The Media table has no entries
		# Suppress ICE71 because the test MSI does not install any files.
	    [
			'light -notidy -nologo -ext WixUIExtension -sice:ICE71',
			'<@(_linked_inputs)',
			'-out', '<(build_dir_arch)/test-installer-lib.msi',
			'-loc', '<@(_localization_input)'
		]
	} ]
  },

  #############
  # Custom Action unit tests
  #############
  {
    'target_name': 'installer-ca-tests',
    'type': 'executable',
    'dependencies':
	[
	  'installer-library',
	  'installer-library-test-msi',			# Some unit tests open the test MSI database
      'googletest.gyp:googletest_main',
    ],
	'sources':
	[
	  'src/installer-lib/test/database_test.cpp',
	  'src/installer-lib/test/process_test.cpp',
	  'src/installer-lib/test/property_test.cpp',
	  'src/installer-lib/test/record_test.cpp',
    ],
    'link_settings':
	{
      'libraries': [],
    },
    'msvs_settings':
	{
      'VCLinkerTool':
	  {
        'SubSystem': '1',   # Console
      },
    },
  },

  ]
}



