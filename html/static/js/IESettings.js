function init()
{
  try
  {
    initLanguageSettings();
   
    initDomainSettings();
  }
  catch (err)
  {
    alert("err: " + err);
  }
}

function initLanguageSettings()
{
  // Locale substitutions
  var localeLanguageDescription = document.getElementById("localeLanguageDescription");
  var languageDescription = UserSettings().GetMessage("settings-language-description");
  if (languageDescription.length)
  {
    localeLanguageDescription.innerText = languageDescription; 
  }

  var localeBlockingDescription = document.getElementById("localeBlockingDescription");
  var blockingDescription = UserSettings().GetMessage("settings-blocking-description");
  if (blockingDescription.length)
  {
    localeBlockingDescription.innerText = blockingDescription; 
  }

  var optionsLanguage = document.getElementById("language");
  addListener(optionsLanguage, "change", function ()
  {
    UserSettings().SetLanguage(optionsLanguage[optionsLanguage.selectedIndex].value);
  }, false);


  var language = UserSettings().GetLanguage();

  var options = optionsLanguage.options;
  for (var i = 0; i < options.length; i++)
  {
   if (options[i].value == language)
   {
    optionsLanguage.selectedIndex = i;
    break;
   }
  }
}

function initDomainSettings()
{
  var optionsDomain = document.getElementById("domain");

  var domains = UserSettings().GetWhitelistDomains().split(",");

  for(var i = 0; i < domains.length; i++)
  {
    var domain = domains[i];
    
    var el = document.createElement("option");
    el.text = domain;
    optionsDomain.add(el, 0);
  }

  var addDomain = document.getElementById("addDomain");
  addListener(addDomain, "click", function()
  {
    var newDomain = document.getElementById("newDomain");
    
    // Trim whitespaces
    var value = newDomain.value.replace(/^\s+|\s+$/, "");
    
    if (value.length)
    {
      var alreadyExist = false;
      for(var i = 0; i < optionsDomain.length; i++)
      {
        if (optionsDomain[i].text == value)
        {
          alreadyExist = true;
          
          // This will move it to the first element, since below it is inserted to 0 position
          optionsDomain.remove(i);
          
          break;
        }
      } 
 
      if (!alreadyExist)
      {
        UserSettings().AddWhitelistDomain(value);
      }
      
      var el = document.createElement("option");
      el.text = value; 
      optionsDomain.add(el, 0);
      
      newDomain.value = "";
    }
  }, false);
  
  var removeDomains = document.getElementById("removeDomains");
  addListener(removeDomains, "click", function()
  {
    var removeOptions = [];
    var domains = optionsDomain.options;
    for (var i = 0; i < domains.length; i++)
    {
      if (domains[i].selected)
      {
        UserSettings().RemoveWhitelistDomain(domains[i].text);
        removeOptions.push(domains[i].index);
      }
    }
    
    for (var i = removeOptions.length; i--;)
    {
      optionsDomain.remove(removeOptions[i]);
    }
  }, false);
}

window.UserSettings = function()
{
  return window.Settings;
}

