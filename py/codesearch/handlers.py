from codesearch import handler_meta

class HomeHandler(handler_meta.RequestHandler):

    path = '/'

    def get(self):
        self.render('search.html')

class AboutHandler(handler_meta.RequestHandler):

    path = '/about'

    def get(self):
        self.env['js'] = False
        self.render('about.html')

# for side-effects
from codesearch import file_handlers
from codesearch import search
