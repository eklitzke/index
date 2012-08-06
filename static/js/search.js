var S = {};
S.currentSearch = null;
S.lastLimit = null;
S.lastQuery = null;
S.nextOffset = 0;
S.decodeHash = (function ()  {
    if (window.location.hash) {
        var q = window.location.hash.substr(2);
        if (q) {
            $('#search_input').val(q);
            S.search(q);
        }
    }
});
S.search = (function (searchVal, limit) {
    limit = parseInt(limit || 20);
    console.log('search, val = ' + searchVal + ', limit = ' + limit);
    history.replaceState({}, "codesear.ch", '/#!' + encodeURI(searchVal));

    var $searchResults = $('#search_results');
    if (searchVal.length <= 0) {
        $searchResults.empty();
        S.lastQuery = "";
    } else if (limit != S.lastLimit || searchVal != S.lastQuery) {
        if (S.currentSearch !== null) {
            console.log('aborting current search');
            S.currentSearch.abort();
        }
        S.lastQuery = searchVal;
        S.lastLimit = limit;
        $searchResults.addClass('grey');
        $('#search_status').
            css({'visibility': 'visible'}).
            text('searching...');
        var params = {'query': searchVal};
        if (limit) {
            params['limit'] = limit;
        }
        S.currentSearch = $.get(
            '/api/search?' + $.param(params), function (r) {
                S.currentSearch = null;
                $searchResults.empty();
                $searchResults.html(r);
                $searchResults.removeClass('grey');
                $('#search_status').css({'visibility': 'hidden'});
            });
    }
});

$(document).ready(function () {
    console.log('ready');
    $('#search_input').keyup(function (e) {
        console.log('----------');
        var searchVal = $('#search_input').val();
        S.search(searchVal);
    });
    S.decodeHash();
    document.getElementById('search_input').focus();
});
