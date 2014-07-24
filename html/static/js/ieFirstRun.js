var AdblockPlus = (function()
{
  var scopes =
  {
    prefs:
    {
      Prefs: {documentation_link: ""}
    },
    utils:
    {
      Utils: {appLocale: ""}
    },
    filterClasses:
    {
      Filter:
      {
        fromText: function(param)
        {
          return {matches: function(param) {return true;}}
        }
      }
    }
  };

  var result = 
  {
    require: function(module)
    {
      return scopes[module];
    }
  };

  window.addEventListener("load", function()
  {
    result.getMessage = function(section, param) 
    {
      return Settings.GetMessage(section, param);
    }
    scopes.prefs.Prefs.documentation_link = Settings.GetDocumentationLink();
    scopes.utils.Utils.appLocale = Settings.GetAppLocale();
  }, false);

  return result;
})();