if (typeof window.addEventListener != "function")
{
  window.addEventListener = function(type, handler, capture)
  {
    this.attachEvent("on" + type, handler)
  };
}

if (typeof window.removeEventListener != "function")
{
  window.removeEventListener = function(type, handler)
  {
    this.detachEvent("on" + type, handler)
  };
}
