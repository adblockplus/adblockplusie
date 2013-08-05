"use strict";

function addListener(element, eventType, callback, propagate)
{
  if (element.addEventListener)
  {
    element.addEventListener(eventType, callback, propagate);
  }
  else
  {
    element.attachEvent("on" + eventType, callback);
  }
}

(function()
{
  function init()
  {
    var manageButton = document.getElementById("manageExceptions");
    addListener(manageButton, "click", toggleManage, false);
  }
  
  function toggleManage(ev)
  {
    var exceptions = document.getElementById("exceptions");
    
    if (exceptions.getAttribute("class"))
      exceptions.removeAttribute("class");
    else
      exceptions.setAttribute("class", "visible");
    
    // IE6-only
    if (exceptions.getAttribute("className"))
      exceptions.removeAttribute("className");
    else
      exceptions.setAttribute("className", "visible");
  }
  
  init();
})();
