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
        return window.Settings.GetMessage(section, param);
    }
    Prefs.documentation_link = window.Settings.GetDocumentationLink();
    Utils.appLocale = window.Settings.GetAppLocale();
}