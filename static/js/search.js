var S = {};
S.currentSearch = null;
S.lastQuery = null;

$(document).ready(function () {
    var templates = {};
    var prefetchTemplate = (function (name) {
        var url = '/static/mustache/' + name + '?v=';
        url += (new Date()).valueOf();
        $.get(url, function (d) {
            templates[name] = d;
        })
    });
    var renderToElement = (function (template, element, view) {
        element.html(Mustache.render(template, view));
    });
    var renderTemplate = (function (name, element, view) {
        if (templates[name] !== undefined) {
            renderToElement(templates[name], element, view);
        } else {
            var url = '/static/mustache/' + name + '?v=';
            url += (new Date()).valueOf();
            $.get(url, function (d) {
                templates[name] = d;
                renderToElement(d, element, view);
            })
        }
    });

    console.log('ready');
    var $search_results = $('#search_results');
    $('#search_input').keyup(function (e) {
        console.log('----------');
        var searchVal = $('#search_input').val();
        if (searchVal.length < 3) {
            $('#search_status').
                css({'visibility': 'visible'}).
                text('please enter a query of 3+ characters');
            $search_results.empty();
        } else if (searchVal != S.lastQuery) {
            if (S.currentSearch !== null) {
                console.log('aborting current search');
                S.currentSearch.abort();
            }
            S.lastQuery = searchVal;
            $search_results.addClass('grey');
            $('#search_status').
                css({'visibility': 'visible'}).
                text('searching...');
            console.log('dispatching request for "' + searchVal + '"');
            S.currentSearch = $.getJSON(
                '/api/search?' + $.param({'query': searchVal}), function (r) {
                    console.log('GOT RESPONSE ->');
                    console.log(r);
                    S.currentSearch = null;
                    var view = {
                        'csearch_time': r.csearch_time,
                        'search_results': r.results,
                        'num_results': r.results.length,
                        'total_results': r.total_results
                    };
                    $search_results.empty();
                    renderTemplate('search_results.mustache',
                                   $search_results, view);
                    $search_results.removeClass('grey');
                    $('#search_status').css({'visibility': 'hidden'});
                });
            console.log('dispatched value was ');
            console.log(S.currentSearch);
        }
    });
    prefetchTemplate('empty_results.mustache');
    prefetchTemplate('search_results.mustache');
});
