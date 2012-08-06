import json
import tornado.web

_handlers = []

def whitespace_escape(s):
    s = s.replace(' ', '&nbsp;')
    s = s.replace('\t', '&nbsp;' * 8)
    return s

class RequestHandlerMeta(type(tornado.web.RequestHandler)):
    def __init__(cls, name, bases, cls_dict):
        ret = super(RequestHandlerMeta, cls).__init__(name, bases, cls_dict)
        if 'path' in cls_dict:
            _handlers.append((cls_dict['path'], cls))
            return ret

class RequestHandler(tornado.web.RequestHandler):

    __metaclass__ = RequestHandlerMeta

    def initialize(self):
        super(RequestHandler, self).initialize()
        self.env = {
            'css_url': self.css_url,
            'js': True,
            'prod': self.in_prod,
            'run_locally': self.settings.get('run_locally', False),
            'title': 'codesear.ch',
            'whitespace_escape': whitespace_escape,
        }

    def compute_etag(self):
        """Disable ETags. They're stupid."""
        return None

    @property
    def debug(self):
        return self.settings.get('debug', True)

    @property
    def in_prod(self):
        return not self.debug

    def render(self, template_name):
        super(RequestHandler, self).render(template_name, **self.env)

    def render_json(self, output):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.write(json.dumps(output))

    def render_raw_json(self, output):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.write(output)

    def css_url(self, s):
        if self.in_prod:
            filename = 'css/%s.out.css' % (s,)
        else:
            filename = 'css/%s.css' % (s,)
        return self.static_url(filename)

def get_handlers():
    return _handlers
