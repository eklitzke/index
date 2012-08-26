var $window = $(window);
var $document = $(document);
var $body = $('body');
var $searchInput = $('#search_input');
var $searchResults = $('#search_results');
var $searchStatus = $('search_status');

var S = {};

S.dummySearch = {
    'params': {'query': null, 'limit': null},
    'abort': function() {}
};

S.currentSearch = S.dummySearch;

S.parseQueryString = function(query) {
    if (query.charAt(0) === '?') {
        query = query.slice(1);
    }
    var q = {};
    query.split('&').forEach(function(kvpair) {
        var pair = kvpair.split('=').map(decodeURIComponent);
        var key = pair[0], value = pair[1];
        if (q.hasOwnProperty(key)) {
            if (q[key] instanceof Array) {
                q[key].push(value);
            } else {
                q[key] = [q[key], value];
            }
        } else {
            q[key] = value;
        }
    }, this);
    return q;
};

// borrowed from underscore.js
S.debounce = function(func, wait, immediate) {
    var timeout;
    return function() {
        var context = this, args = arguments;
        var later = function() {
            timeout = null;
            if (!immediate) func.apply(context, args);
        };
        if (immediate && !timeout) func.apply(context, args);
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
};

S.search = function(searchVal, limit) {
    limit = parseInt(limit || 10, 10);
    console.log('search: query = %o, limit = %o', searchVal, limit);
    var params = {'query': searchVal, 'limit': limit};
    var xhr = $.get('/api/search?' + $.param(params));
    xhr.params = params;
    return xhr;
};

S.ui = {};

S.ui.loadState = function(state, doSearch) {
    if (state && state.q) {
        $searchInput
            .val(state.q);
        if (doSearch) {
            $searchInput.triggerSearch();
        }
    }
};

S.ui.loadQueryString = function(skipSearch) {
    S.ui.loadState(S.parseQueryString(location.search), false);
};

S.ui.renderResults = function(html, searchVal) {
    $searchResults
        .html(html || '')
        .removeClass('grey');
    $searchStatus.css({'visibility': 'hidden'});
    if (searchVal) {
        $window.trigger('ui:updateURL', [searchVal]);
    }
};

S.ui.search = function(searchVal) {
    if (S.currentSearch.params.query === searchVal) {
        return;
    }

    S.currentSearch.abort();

    if (searchVal) {
        $searchResults.addClass('grey');
        $searchStatus
            .css({'visibility': 'visible'})
            .text('searching...');

        S.currentSearch = S.search(searchVal).done(function(html) {
            S.ui.renderResults(html, searchVal);
        });
    } else {
        S.currentSearch = S.dummySearch;
        S.ui.renderResults();
    }
};

S.ui.updateURL = function(searchVal) {
    var search = '?' + $.param({'q': searchVal});
    var action = (location.search === search) ? 'replaceState' : 'pushState';
    history[action]({'q': searchVal}, "codesear.ch", location.pathname + search);
};

// extend jQuery

$.fn.triggerSearch = function() {
    return this.trigger('ui:search', [this.val().trim()]);
};


//
// main
//

// dom events

$window.on('popstate', function(e) {
    S.ui.loadState(e.originalEvent.state, true);
});

$searchInput.keyup(function(e) {
    $(this).triggerSearch();
});

// synthetic events

$window.on('ui:search', S.debounce(function(e, searchVal) {
    S.ui.search(searchVal);
}, 100));

$window.on('ui:updateURL', S.debounce(function(e, query) {
    S.ui.updateURL(query);
}, 500));

S.ui.loadQueryString();
