from codesearch import handler_meta

class StaticHandler(handler_meta.RequestHandler):

    abstract = True

    def get(self):
        if getattr(self, 'no_js', False):
            self.env['js'] = False
        self.render(self.template_path)


class AboutHandler(StaticHandler):

    path = '/about'
    template_path = 'about.html'
    no_js = True


class ContactHandler(StaticHandler):

    path = '/contact'
    template_path = 'contact.html'
    no_js = True


# for side-effects
from codesearch import file_handlers
from codesearch import search
