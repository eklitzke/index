var S = {};
S.currentSearch = null;
S.lastQuery = null;
S.nextOffset = 0;
S.decodeHash = (function ()  {
    if (window.location.hash) {
        var q = window.location.hash.substr(2);
        if (q) {
            $('#search_input').val(q);
            S.search(q, 40);
        }
    }
});
S.search = (function (searchVal, limit) {
    limit = parseInt(limit || 40);
    console.log('search, val = ' + searchVal + ', limit = ' + limit);
    history.replaceState({}, "codesear.ch", '/#!' + encodeURI(searchVal));

    var $searchResults = $('#search_results');
    if (searchVal.length <= 0) {
        //$('#search_status').css({'visibility': 'visible'});
        $searchResults.empty();
    } else if (limit || searchVal != S.lastQuery) {
        if (S.currentSearch !== null) {
            console.log('aborting current search');
            S.currentSearch.abort();
        }
        S.lastQuery = searchVal;
        $searchResults.addClass('grey');
        $('#search_status').
            css({'visibility': 'visible'}).
            text('searching...');
        var params = {'query': searchVal};
        if (limit) {
            params['limit'] = limit;
        }
        S.currentSearch = $.getJSON(
            '/api/search?' + $.param(params), function (r) {
                S.currentSearch = null;
                S.nextOffset = r.next_offset;
                $searchResults.empty();
                r['show_more'] = (limit < 400);
                S.renderTemplate('search_results.mustache',
                                 $searchResults, r, function () {
                                     $('#more_link').click(function (e) {
                                         e.preventDefault();
                                         S.search(searchVal, limit + 40)
                                         console.log('lol');
                                     })});
                $searchResults.removeClass('grey');
                $('#search_status').css({'visibility': 'hidden'});
            });
    }
});

S.templates = {};
S.prefetchTemplate = (function (name) {
    var url = '/static/mustache/' + name + '?v=';
    url += (new Date()).valueOf();
    $.get(url, function (d) {
        S.templates[name] = d;
    })
});

S.renderToElement = (function (template, element, view, after) {
    console.info('in render to element');
    console.log(element);
    console.log(view);
    element.html(Mustache.render(template, view));
    if (after != undefined) {
        after();
    }
});

S.renderTemplate = (function (name, element, view, after) {
    if (S.templates[name] !== undefined) {
        S.renderToElement(S.templates[name], element, view, after);
    } else {
        var url = '/static/mustache/' + name + '?v=';
        url += (new Date()).valueOf();
        $.get(url, function (d) {
            S.templates[name] = d;
            S.renderToElement(d, element, view, after);
        })
    }
});

$(document).ready(function () {
    console.log('ready');
    $('#search_input').keyup(function (e) {
        console.log('----------');
        var searchVal = $('#search_input').val();
        S.search(searchVal);
    });
    S.prefetchTemplate('search_results.mustache');
    S.decodeHash();
});
