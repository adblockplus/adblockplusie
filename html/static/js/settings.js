"use strict";

(function()
{
  function init()
  {
    var manageButton = document.getElementById("manageExceptions");
    manageButton.addEventListener("click", toggleManage, false);
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
