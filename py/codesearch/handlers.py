from codesearch import handler_meta

class HomeHandler(handler_meta.RequestHandler):

    path = '/'

    def get(self):
        self.env['title'] = 'linux code search'
        self.env['search_js_url'] = self.static_path('js/search.js')
        self.render('search.html')

class AboutHanlder(handler_meta.RequestHandler):

    path = '/about'

    def get(self):
        self.env['title'] = 'about'
        self.render('about.html')

from codesearch import search  # for side-effects