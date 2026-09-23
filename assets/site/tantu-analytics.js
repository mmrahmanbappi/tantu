/* Loads Google Analytics 4. Added by tantu when google_analytics is set. */
(function () {
  var id = document.currentScript && document.currentScript.getAttribute("data-id");
  if (!id) return;
  window.dataLayer = window.dataLayer || [];
  window.gtag = function () { window.dataLayer.push(arguments); };
  window.gtag("js", new Date());
  window.gtag("config", id);
})();
