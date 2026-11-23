/*
 * Adds the language switcher, version selector and repository link to every
 * generated page.
 *
 * It is injected from JavaScript instead of a forked HTML_HEADER template:
 * doxygen's header template is version specific, and forking it breaks on
 * every doxygen upgrade. doxygen-awesome-css explicitly advises against
 * changing the generated HTML structure as well.
 *
 * The widget is only shown when the page lives under /<base>/<version>/, so
 * the version chooser at the site root and local previews stay clean.
 */
(function () {
    "use strict";

    var VERSIONS = ["latest", "3.1", "3.0", "2.0"];
    var REPO_URL = "https://github.com/h4tr3d/avcpp";

    function pathSegments() {
        return window.location.pathname.split("/").filter(function (part) {
            return part.length > 0;
        });
    }

    function locateVersion(parts) {
        for (var i = 0; i < parts.length; i += 1) {
            if (VERSIONS.indexOf(parts[i]) !== -1) {
                return i;
            }
        }
        return -1;
    }

    function siteRoot(parts, versionIndex, locale) {
        var root = "/" + parts.slice(0, versionIndex + 1).join("/") + "/";
        return locale === "ru" ? root + "ru/" : root;
    }

    function build() {
        var parts = pathSegments();
        var versionIndex = locateVersion(parts);
        if (versionIndex === -1) {
            return;
        }

        var currentVersion = parts[versionIndex];
        var isRussian = parts[versionIndex + 1] === "ru";

        var nav = document.createElement("div");
        nav.id = "avcpp-site-nav";
        nav.dataset.locale = isRussian ? "ru" : "en";

        var enLink = document.createElement("a");
        enLink.href = siteRoot(parts, versionIndex, "en");
        enLink.textContent = "EN";
        enLink.title = "English documentation";

        var ruLink = document.createElement("a");
        ruLink.href = siteRoot(parts, versionIndex, "ru");
        ruLink.textContent = "RU";
        ruLink.title = "Русская документация";

        var separator = document.createElement("span");
        separator.className = "avcpp-sep";
        separator.textContent = "|";

        var select = document.createElement("select");
        select.title = "Documentation version";
        VERSIONS.forEach(function (version) {
            var option = document.createElement("option");
            option.value = version;
            option.textContent = version;
            option.selected = version === currentVersion;
            select.appendChild(option);
        });
        select.addEventListener("change", function () {
            var base = parts.slice(0, versionIndex).join("/");
            base = base.length > 0 ? "/" + base : "";
            window.location.href = base + "/" + select.value + "/" + (isRussian ? "ru/" : "");
        });

        var github = document.createElement("a");
        github.href = REPO_URL;
        github.textContent = "GitHub";
        github.rel = "noopener";

        [enLink, ruLink, separator, select, github].forEach(function (node) {
            nav.appendChild(node);
        });

        nav.classList.add("avcpp-ready");
        document.body.appendChild(nav);
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", build);
    } else {
        build();
    }
}());
