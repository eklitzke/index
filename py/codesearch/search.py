import functools
import re
import subprocess
from tornado import escape
from tornado import web
from codesearch import handler_meta
from codesearch import filesystem
from codesearch import search_rpc

_filename_re = re.compile('/home/evan/Downloads/')
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

    @staticmethod
    def format_string(s, escaped_query):
        s = escape.xhtml_escape(s)
        s = s.replace(escaped_query, '<em>' + escaped_query + '</em>')
        s = s.replace(' ', '&nbsp;')
        s = s.replace('\t', '&nbsp;' * 8)
        return s

    def search_callback(self, query, results):
        self.ensure_released()
        try:
            escaped_query = escape.xhtml_escape(query).encode('utf-8')
            search_results = results.results[:self.limit - 1]
            overflowed = len(results.results) == self.limit
            json_results = {
                'results': [],
                'num_results': len(search_results),
                'overflowed': overflowed,
                'csearch_time': results.time_elapsed
            }
            for result in search_results:
                val = {
                    'filename': _filename_re.sub('', result.filename),
                    'line_num': result.line_num,
                    'line_text': self.format_string(
                        result.line_text, escaped_query)
                }
                json_results['results'].append(val)
            self.render_json(json_results)
            self.finish()
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
        query = self.get_argument('query', '', strip=False)
        self.limit = self.rpc_client.limit()
        try:
            self.rpc_client.search(
                query, functools.partial(self.search_callback, query))
        except IOError:
            if self.rpc_client is not None:
                self.rpc_client.close()
                self.ensure_released()
            self.finish();
