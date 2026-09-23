/* tantu site search. Runs in the browser, no server needed. */
(function () {
  "use strict";
  var root = document.getElementById("tantu-search");
  if (!root) return;
  var input = root.querySelector("input");
  var out = root.querySelector("ol");
  var status = root.querySelector(".ts-status");
  var index = null;

  function norm(s) {
    s = (s || "").toLowerCase();
    return s.normalize ? s.normalize("NFD").replace(/[\u0300-\u036f]/g, "") : s;
  }

  function run() {
    if (!index) return;
    var raw = input.value.trim();
    var q = norm(raw);
    if (history.replaceState) history.replaceState(null, "", raw ? "?q=" + encodeURIComponent(raw) : location.pathname);
    out.textContent = "";
    if (!q) { status.textContent = ""; return; }
    var terms = q.split(/\s+/);
    var results = [];
    for (var i = 0; i < index.length; i++) {
      var d = index[i], score = 0;
      for (var k = 0; k < terms.length; k++) {
        var inTitle = d._t.indexOf(terms[k]) >= 0, inBody = d._b.indexOf(terms[k]) >= 0;
        if (!inTitle && !inBody) { score = -1; break; }
        score += inTitle ? 10 : 1;
      }
      if (score > 0) results.push([score, d]);
    }
    results.sort(function (a, b) { return b[0] - a[0]; });
    status.textContent = results.length
      ? results.length + (results.length === 1 ? " result" : " results")
      : "No results for \u201c" + raw + "\u201d.";
    for (var j = 0; j < results.length && j < 30; j++) {
      var li = document.createElement("li");
      var a = document.createElement("a");
      a.href = results[j][1].u;
      a.textContent = results[j][1].t;
      var p = document.createElement("p");
      p.textContent = results[j][1].d;
      li.appendChild(a);
      li.appendChild(p);
      out.appendChild(li);
    }
  }

  var q = new URLSearchParams(location.search).get("q");
  if (q) input.value = q;
  input.addEventListener("input", run);
  root.querySelector("form").addEventListener("submit", function (e) { e.preventDefault(); run(); });

  fetch(root.getAttribute("data-index"))
    .then(function (r) { return r.json(); })
    .then(function (data) {
      index = data.map(function (d) {
        d._t = norm(d.t);
        d._b = norm([d.d, d.g, d.x].join(" "));
        return d;
      });
      run();
    })
    .catch(function () { status.textContent = "Search is not available right now."; });
})();
