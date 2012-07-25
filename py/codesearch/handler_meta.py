import json
import tornado.web

_handlers = []

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
            'js': True,
            'prod': self.in_prod,
            'title': 'codesear.ch',
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

def get_handlers():
    return _handlers
