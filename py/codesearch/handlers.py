from codesearch import handler_meta

class HomeHandler(handler_meta.RequestHandler):

    path = '/'

    def get(self):
        self.render('search.html')

class AboutHanlder(handler_meta.RequestHandler):

    path = '/about'

    def get(self):
        self.env['js'] = False
        self.render('about.html')

from codesearch import search  # for side-effects