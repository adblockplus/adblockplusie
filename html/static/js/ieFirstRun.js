var AdblockPlus = {
	require: function(param)
	{
		if (param == "prefs")
		{
			return {
				Prefs: 
				{
					documentation_link: ""
				}
			}
		}
		if (param == "utils")
		{
			return {
				Utils: 
				{
					appLocale: ""
				}
			}
		}
		if (param == "filterClasses")
		{
			return {
				Filter: 
				{
					fromText: function(param) 
					{ 
						return {
							matches: function(param) {
								return true; 
							} 
						}
					} 
				}
			}
		}
		return {};
	}
}

function initWrappers()
{
	AdblockPlus.getMessage = function(section, param)
	{
		return Settings.GetMessage(section, param);
	}
	Prefs.documentation_link = Settings.GetDocumentationLink();
	Utils.appLocale = Settings.GetAppLocale();
}

window.addEventListener("load", initWrappers, false);
