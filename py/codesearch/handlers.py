from codesearch import handler_meta

class HomeHandler(handler_meta.RequestHandler):

    path = '/'

    def get(self):
        self.env['title'] = 'linux code search'
        self.env['search_js_url'] = self.static_path('js/search.js')
        self.render('search.html')

from codesearch import search  # for side-effects