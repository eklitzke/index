from tornado import escape
from tornado import web
from codesearch import handler_meta
from codesearch import search_rpc

_rpc_pool = search_rpc.SearchRpcPool.instance()

class SearchHandler(handler_meta.RequestHandler):

    path = '/api/search'

    def initialize(self, *args, **kwargs):
        super(SearchHandler, self).initialize(*args, **kwargs)
        self.rpc_client = _rpc_pool.acquire()
        self.rpc_client_released = False

    def ensure_released(self):
        if not self.rpc_client_released:
            _rpc_pool.release(self.rpc_client)
            self.rpc_client = None
            self.rpc_client_released = True

    def format_string(self, s):
        s = escape.xhtml_escape(s)
        s = s.replace(self.escaped_query, '<em>' + self.escaped_query + '</em>')
        s = s.replace(' ', '&nbsp;')
        s = s.replace('\t', '&nbsp;' * 8)
        return s

    def search_callback(self, rpc_container, results):
        self.ensure_released()
        self.escaped_query = escape.xhtml_escape(self.query)

        def highlight(text):
            text = escape.xhtml_escape(text)
            text = text.replace(' ', '&nbsp;')
            text = text.replace('\t', '&nbsp;' * 4)
            text = text.replace(
                self.escaped_query,
                '<span class="highlight">' + self.escaped_query + '</span>')
            return text

        try:
            search_results = results.results[:self.limit]
            overflowed = len(results.results) > self.limit
            self.env.update({
                'highlight': highlight,
                'offset': self.offset,
                'escaped_query': self.escaped_query,
                'search_results': [],
                'num_results': len(search_results),
                'show_more': self.limit < 400,
                'overflowed': overflowed,
                'csearch_time': rpc_container.time_elapsed
            })
            for result in search_results:
                self.env['search_results'].append(result)
            self.render('search_results.html')
            #self.finish()
        except IOError:
            # A common reason that this might happen is that the
            # client closed its connection while waiting for our
            # response. This is especially common for this handler,
            # because the way the JavaScript code works is it cancels
            # its XHRRequests when it has a new one to make and old
            # ones are pending. For example, let's say a use is typing
            # the word "void". The frontend will issue a request for
            # "voi" followed by a request for "void". If the request
            # for "voi" hasn't finished by the time the user types the
            # 'd' and the frontend is ready to query "void", the
            # "void" request will be cancelled. That causes this
            # IOError.
            pass

    @web.asynchronous
    def get(self):
        self.query = self.get_argument('query', '', strip=False).encode('utf-8')
        limit = int(self.get_argument('limit', 40))
        limit = max(limit, 20) # at least 20 results, please
        if limit > 400:
            raise web.HTTPError(403)
            return
        self.limit = limit
        self.offset = int(self.get_argument('offset', 0))
        try:
            self.rpc_client.search(
                self.query, self.search_callback, self.limit + 1, self.offset)
        except IOError:
            if self.rpc_client is not None:
                self.rpc_client.close()
                self.ensure_released()
            self.finish()
