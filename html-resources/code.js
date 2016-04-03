(function() {
    var COOKIE_NAME = 'oberon00.synth.codeCssSelIdx';

    // Taken from http://stackoverflow.com/a/24103596/2128694,
    // user Mandeep Janjua {{{

    function createCookie(name, value, days) {
        if (days) {
            var date = new Date();
            date.setTime(date.getTime() + (days * 24 * 60 * 60 * 1000));
            var expires = "; expires=" + date.toGMTString();
        }
        else var expires = "";
        document.cookie = name + "=" + value + expires + "; path=/";
    }

    function readCookie(name) {
        var nameEQ = name + "=";
        var ca = document.cookie.split(';');
        for(var i = 0; i < ca.length; ++i) {
            var c = ca[i];
            while (c.charAt(0) == ' ')
                c = c.substring(1, c.length);
            if (c.indexOf(nameEQ) == 0)
                return c.substring(nameEQ.length, c.length);
        }
        return null;
    }

    // }}}

    function replaceFilenameStem(p, r) {
        var beg = p.lastIndexOf('/');
        beg = beg < 0 ? 0 : beg;
        var end = p.lastIndexOf('.');
        end = end < 0 ? p.length : end;
        return p.substr(0, beg + 1) + r + p.substr(end);
    }

    function updateSelectedStyle() {
        var choosenStyle = sel.options[sel.selectedIndex].value;
        styleLnk.href = replaceFilenameStem(styleLnk.href, choosenStyle);
        console.log('Style ' + choosenStyle + ' selected.');
    }

    var styleLnk = document.getElementById('styleLnk');
    if (!styleLnk) {
        var allLinks = document.getElementsByTagName('link');
        for (var i = 0; i < allLinks.length; ++i) {
            if (allLinks[i].rel == 'stylesheet') {
                styleLnk = allLinks[i];
                break;
            }
        }
    }
    // var curStyle = getStem(getFilename(styleLnk.href));
    // console.log(curStyle);
    // console.log(replaceFilenameStem(styleLnk.href, 'code-simple'));

    var chooserHolder = document.getElementById('chooserHolder');
    if (!chooserHolder) {
        chooserHolder = document.createElement('div');
        chooserHolder.id = 'chooserHolder';
        document.body.insertBefore(chooserHolder, document.body.firstChild);
    }

    chooserHolder.appendChild(document.createTextNode('Style: '));
    var sel = document.createElement('select');

    // ADD STYLES HERE (display-name, filename): //
    sel.add(new Option('colorful', 'code'));
    sel.add(new Option('vs', 'code-simple'));

    sel.onchange = function() {
        updateSelectedStyle();
        createCookie(COOKIE_NAME, sel.selectedIndex, /*days:*/ 7);
    }

    chooserHolder.appendChild(sel);

    var styleIdx = Number(readCookie(COOKIE_NAME));
    if (!isNaN(styleIdx) && styleIdx > 0) {
        sel.selectedIndex = styleIdx;
        updateSelectedStyle();
    }
})();
