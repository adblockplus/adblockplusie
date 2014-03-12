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

function addListener(object, type, listener)
{
  // all IE versions require the use of attachEvent for message event to work
  if (type != "message" && "addEventListener" in object)
    object.addEventListener(type, listener, false);
  else
  {
    object.attachEvent("on" + type, function()
    {
      listener(event);
    });
  }
}

function removeListener(object, type, listener)
{
  // all IE versions require the use of detachEvent for message event to work
  if (type != "message" && "removeEventListener" in object)
    object.removeEventListener(type, listener);
  else
    object.detachEvent("on" + type, listener);
}

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

    var data = JSON.parse(event.data);
    iframe.width = data.width;
    iframe.height = data.height;
    popupMessageReceived = true;
    removeListener(window, "message", popupMessageListener);
  };
  addListener(window, "message", popupMessageListener);

  var listenCount = 0;
  var popupLoadListener = function()
  {
    if (popupMessageReceived)
    {
      iframe.className = "visible";

      var popupCloseListener = function()
      {
        iframe.className = glassPane.className = "";
        removeListener(document, "click", popupCloseListener);
      };
      addListener(document, "click", popupCloseListener);
    }
    else
    {
      // wait up to 5 seconds and close popup if no message received
      if (++listenCount > 20)
      {
        glassPane.className = "";
        removeListener(window, "message", popupMessageListener);
      }
      else
        setTimeout(popupLoadListener, 250);
    }

    removeListener(iframe, "load", popupLoadListener);
  };
  addListener(iframe, "load", popupLoadListener);

  iframe.src = url;
  glassPane.className = "visible";
}

function initSocialLinks(variant)
{
  // Share popup doesn't work in <IE9 so don't show it
  if (/MSIE [6-8]/.test(navigator.appVersion))
    return;

  var networks = ["twitter", "facebook", "gplus"];
  networks.forEach(function(network)
  {
    var link = document.getElementById("share-" + network + "-" + variant);
    addListener(link, "click", function(e)
    {
      e.preventDefault();
      openSharePopup(getDocLink("share-" + network) + "&variant=" + variant);
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
    "title-main": "first-run-title-install",
    "i18n-features-heading": "first-run-features-heading",
    "i18n-feature-betterSurfing": "first-run-feature-betterSurfing",
    "i18n-feature-videoAds": "first-run-feature-videoAds",
    "share-text1": "first-run-share1",
    "share-text2": "first-run-share2",
    "share-donate": "first-run-share2-donate",
    "share2-connection": "first-run-share2-or"
  };

  document.title = AdblockPlus.getMessage("first-run", "first-run-title-install");
  for (var i in mapping)
  {
    var element = document.getElementById(i);
    element.innerText = AdblockPlus.getMessage("first-run", mapping[i]);
  }
}

function init()
{
  initWrappers();
  // Choose a share text variant randomly
  var variant = Math.floor(Math.random() * 2) + 1;
  var classList = document.documentElement.className.split(" ");
  classList.push("share-variant-" + variant);
  document.documentElement.className = classList.join(" ");

  initTranslations();
  initSocialLinks(variant);

  var donateLink = document.getElementById("share-donate");
  donateLink.href = getDocLink("donate") + "&variant=" + variant;
}

addListener(window, "load", init);
