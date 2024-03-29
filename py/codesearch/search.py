from tornado import escape
from tornado import web
from codesearch import handler_meta
from codesearch import search_rpc

_rpc_pool = search_rpc.SearchRpcPool.instance()

class SearchBase(handler_meta.RequestHandler):

    abstract = True
    default_limit = 10

    def initialize(self, *args, **kwargs):
        super(SearchBase, self).initialize(*args, **kwargs)
        self.rpc_client = _rpc_pool.acquire()
        self.rpc_client_released = False

    def ensure_released(self):
        if not self.rpc_client_released:
            _rpc_pool.release(self.rpc_client)
            self.rpc_client = None
            self.rpc_client_released = True

    def search_callback(self, rpc_container, results):
        self.ensure_released()
        self.escaped_query = escape.xhtml_escape(self.query)

        def highlight(text):
            text = escape.xhtml_escape(text)
            text = text.replace(
                self.escaped_query,
                '<span class="search-highlight">' + self.escaped_query +
                '</span>')
            return text

        try:
            search_results = results.results[:self.limit]
            overflowed = len(results.results) > self.limit
            self.env.update({
                'highlight': highlight,
                'offset': self.offset,
                'escaped_query': self.escaped_query,
                'num_results': len(search_results),
                'show_more': self.limit < self.default_limit * 10,
                'overflowed': overflowed,
                'csearch_time': rpc_container.time_elapsed
            })
            env_search_results = []
            for result in search_results:
                # group the search results by sets of consecutive
                # lines
                data = {'context': result,
                        'grouped_lines': []
                }
                is_start = True
                last_line = 0
                group = []
                for r in result.lines:
                    if not is_start and r.line_num != last_line + 1:
                        data['grouped_lines'].append(group[:])
                        del group[:]
                        group.append(r)
                    else:
                        group.append(r)
                    is_start = False
                    last_line = r.line_num
                assert group
                data['grouped_lines'].append(group)
                env_search_results.append(data)
            self.env['search_results'] = env_search_results
            self.render_search()
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

    def render_search(self):
        raise NotImplementedError()

    @web.asynchronous
    def get(self):
        self.query = self.get_argument(self.query_param_name,
                                       '', strip=False).encode('utf-8')
        if self.query:
            limit = int(self.get_argument('limit', self.default_limit))
            limit = max(limit, 10) # at least 10 results, please
            if limit > self.default_limit * 10:
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
        else:
            self.render_search()


class HomeHandler(SearchBase):

    path = '/'
    query_param_name = 'q'

    def render_search(self):
        if self.env.get('search_results'):
            search_results = self.render_string(
                'search_results.html', **self.env)
            self.env['raw_search_html'] = search_results
        else:
            self.env['raw_search_html'] = ''
        self.render('search.html')


class SearchHandler(SearchBase):

    path = '/api/search'
    query_param_name = 'query'

    def render_search(self):
        self.render('search_results.html')
