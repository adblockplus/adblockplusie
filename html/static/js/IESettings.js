function init()
{
  try
  {
    initLanguageSettings();
   
    initDomainSettings();
  }
  catch (err)
  {
    //alert("err: " + err);
  }
}

function setElementText(id, key)
{
  var el = document.getElementById(id);
  if (el)
  {
    var text = UserSettings().GetMessage("settings", key);
    if (text)
    {
      el.innerText = text; 
    }
  }
}

function acceptableAdsChange()
{
  var aaCheckbox = document.getElementById("acceptableAdsCheckbox");
  UserSettings().SetAcceptableAdsEnabled(aaCheckbox.checked);  
}

function initLanguageSettings()
{
  setElementText("title", "settings-title");

  setElementText("localeLanguageLabel", "settings-language-label");
  setElementText("localeLanguageDescription", "settings-language-description");

  setElementText("localeBlockingLabel", "settings-blocking-label");
  setElementText("localeBlockingDescription", "settings-blocking-description");

  setElementText("manageExceptions", "settings-exceptions-manage-label");
  setElementText("addDomain", "settings-exceptions-add-label");
  setElementText("removeDomains", "settings-exceptions-remove-label");

  setElementText("localeWorthSharing", "settings-share-label");

  setElementText("acceptableAdsLabel", "settings-acceptable-ads");
   
  var aaCheckbox = document.getElementById("acceptableAdsCheckbox");
  if (aaCheckbox)
  {
    aaCheckbox.checked = UserSettings().IsAcceptableAdsEnabled();
    aaCheckbox.addEventListener("change", acceptableAdsChange, false);
  }

  var optionsLanguage = document.getElementById("language");

  var languageCount = UserSettings().GetLanguageCount();
  for(var i = 0; i < languageCount; i++)
  {
    var el = document.createElement("option");
    el.text = UserSettings().GetLanguageTitleByIndex(i);
    el.value = UserSettings().GetLanguageByIndex(i);
    
    optionsLanguage.add(el, 0);
  }
  
  optionsLanguage.addEventListener("change", function ()
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
  addDomain.addEventListener("click", function()
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
  removeDomains.addEventListener("click", function()
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
