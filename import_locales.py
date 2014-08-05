#!/usr/bin/env python

import codecs
from ConfigParser import SafeConfigParser
import os
import re

from buildtools import localeTools

ie_locale_base_path = "locales"
gecko_locale_base_path = "libadblockplus/adblockplus/chrome/locale"

gecko_locale_mapping = dict(zip(localeTools.langMappingGecko.values(),
                                localeTools.langMappingGecko.keys()))

# We should be using en-US instead of en in IE, see
# https://issues.adblockplus.org/ticket/1177
gecko_locale_mapping["en"] = "en-US"

strings_to_import = {
  "firstRun.properties/firstRun_acceptableAdsHeadline": "first-run/first-run-aa-title",
  "firstRun.properties/firstRun_acceptableAdsExplanation": "first-run/first-run-aa-text",
  "filters.dtd/acceptableAds2.label": "settings/settings-acceptable-ads"
}

def read_gecko_locale_strings(locale):
  locale_strings = {}
  locale_files = set([key.split("/")[0] for key in strings_to_import.keys()])
  for locale_file in locale_files:
    locale_file_path = os.path.join(gecko_locale_base_path, locale, locale_file)
    if os.path.exists(locale_file_path):
      locale_strings[locale_file] = localeTools.readFile(locale_file_path)
    else:
      locale_strings[locale_file] = {}
  return locale_strings

# This is to keep the locale file format largely intact -
# SafeConfigParser.write() puts spaces around equal signs.
def write_ini(config, file):
  for index, section in enumerate(config.sections()):
    if index > 0:
      file.write("\n")
    file.write("[%s]\n" % section)
    items = config.items(section)
    for key, value in items:
      file.write("%s=%s\n" % (key, re.sub(r"\s+", " ", value, flags=re.S)))

def import_locale(ie_locale):
  gecko_locale = gecko_locale_mapping.get(ie_locale, ie_locale)
  gecko_locale_strings = read_gecko_locale_strings(gecko_locale)

  ie_locale_path = "locales/%s.ini" % ie_locale
  config = SafeConfigParser()
  config.optionxform = str
  with codecs.open(ie_locale_path, "r", "utf-8") as ie_locale_file:
    config.readfp(ie_locale_file)

  for source, target in strings_to_import.iteritems():
    source_section, source_key = source.split("/")
    target_section, target_key = target.split("/")
    if source_key in gecko_locale_strings[source_section]:
      value = gecko_locale_strings[source_section][source_key]
      value = re.sub(r"\s*\(&.\)$", "", value).replace("&", "")
      config.set(target_section, target_key, value)

  with codecs.open(ie_locale_path, "w", "utf-8") as ie_locale_file:
    write_ini(config, ie_locale_file)

def import_locales():
  ie_locales = [os.path.splitext(file)[0]
                for file in os.listdir(ie_locale_base_path)
                if os.path.isfile(os.path.join(ie_locale_base_path, file))]
  for ie_locale in ie_locales:
    import_locale(ie_locale)

if __name__ == "__main__":
  import_locales()
