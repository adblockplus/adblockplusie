/*
 * This file is part of Adblock Plus <http://adblockplus.org/>,
 * Copyright (C) 2006-2014 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

var require = AdblockPlus.require;
var Prefs = require("prefs").Prefs;
var Utils = require("utils").Utils;
var Filter = require("filterClasses").Filter;

function openSharePopup(url)
{
  var iframe = document.getElementById("share-popup");
  var glassPane = document.getElementById("glass-pane");
  var popupMessageReceived = false;

  var popupMessageListener = function(event)
  {
    var originFilter = Filter.fromText("||adblockplus.org^");
    if (!originFilter.matches(event.origin, "OTHER", null, null))
      return;

    var data = event.data;
    iframe.width = data.width;
    iframe.height = data.height;
    popupMessageReceived = true;
    window.removeEventListener("message", popupMessageListener);
  };
  window.addEventListener("message", popupMessageListener);

  var listenCount = 0;
  var popupLoadListener = function()
  {
    if (popupMessageReceived)
    {
      iframe.className = "visible";

      var popupCloseListener = function()
      {
        iframe.className = glassPane.className = "";
        document.removeEventListener("click", popupCloseListener);
      };
      document.addEventListener("click", popupCloseListener);
    }
    else
    {
      // wait up to 5 seconds and close popup if no message received
      if (++listenCount > 20)
      {
        glassPane.className = "";
        window.removeEventListener("message", popupMessageListener);
      }
      else
        setTimeout(popupLoadListener, 250);
    }

    iframe.removeEventListener("load", popupLoadListener);
  };
  iframe.addEventListener("load", popupLoadListener);

  iframe.src = url;
  glassPane.className = "visible";
}

function initSocialLinks()
{
  // Share popup doesn't work in <IE9 so don't show it
  if (/MSIE [6-8]/.test(navigator.appVersion))
    return;

  var networks = ["twitter", "facebook", "gplus"];
  networks.forEach(function(network)
  {
    var link = document.getElementById("share-" + network);
    link.addEventListener("click", function(e)
    {
      e.preventDefault();
      openSharePopup(getDocLink("share-" + network));
    });
  });
}

function getDocLink(page)
{
  return Prefs.documentation_link
              .replace(/%LINK%/g, page)
              .replace(/%LANG%/g, Utils.appLocale);
}

function initTranslations()
{
  // Map message ID to HTML element ID
  var mapping = {
    "aa-title": "first-run-aa-title",
    "aa-text": "first-run-aa-text",
    "title-main": AdblockPlus.isUpdate() ? "first-run-title-update" : "first-run-title-install",
    "share-donate": "first-run-share2-donate",
    "share-text": "first-run-share2",
    "share-connection": "first-run-share2-or"
  };

  document.title = AdblockPlus.getMessage("first-run", mapping['title-main']);
  for (var i in mapping)
  {
    var element = document.getElementById(i);
    setElementText(element, AdblockPlus.getMessage("first-run", mapping[i]));
  }
}

function init()
{
  initTranslations();
  initSocialLinks();
  setLinks("aa-text", getDocLink("acceptable_ads_criteria"), "index.html");

  var donateLink = document.getElementById("share-donate");
  donateLink.href = getDocLink("donate");
}

// Inserts i18n strings into matching elements. Any inner HTML already in the 
// element is parsed as JSON and used as parameters to substitute into 
// placeholders in the i18n message.
setElementText = function(element, elementHtml)
{
  function processString(str, element)
  {
    var match = /^(.*?)<(a|strong)>(.*?)<\/\2>(.*)$/.exec(str);
    if (match)
    {
      processString(match[1], element);

      var e = document.createElement(match[2]);
      processString(match[3], e);
      element.appendChild(e);

      processString(match[4], element);
    }
    else
      element.appendChild(document.createTextNode(str));
  }

  while (element.lastChild)
    element.removeChild(element.lastChild);
  processString(elementHtml, element);
}


function setLinks(id)
{
  var element = document.getElementById(id);
  if (!element)
  {
    return;
  }

  var links = element.getElementsByTagName("a");

  for (var i = 0; i < links.length && i < arguments.length - 1; i++)
  {
    var curArg = arguments[i + 1];
    if (typeof curArg == "string")
    {
      links[i].href = curArg;
      links[i].setAttribute("target", "_blank");
    }
    else if (typeof curArg == "function")
    {
      links[i].href = "javascript:void(0);";
      links[i].addEventListener("click", curArg, false);
    }
  }
}