import hashlib
import json
import os
import pystache
import tornado.web

from codesearch import filesystem

_handlers = []
_hashes = {}

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
            'title': 'codesear.ch',
            'base_css_url': self.static_path('css/base.css'),
            'zepto_url': self.static_path('js/zepto.min.js'),
            'reset_url': self.static_path('css/reset.css'),
            'mustache_url': self.static_path('js/mustache.js'),
        }

    def compute_etag(self):
        """Disable ETags. They're stupid."""
        return None

    def static_path(self, name):
        try:
            hashval = _hashes[name]
        except KeyError:
            with open(filesystem.get_static_path(name)) as r:
                hashval = hashlib.sha1(r.read()).hexdigest()[:6]
            if not self.settings.get('debug'):
                _hashes[name] = hashval
        return os.path.join('static', name) + '?v=' + hashval

    def render(self, template_name):
        with open(filesystem.get_template_path(template_name)) as f:
            contents = f.read()
        self.write(pystache.render(contents, self.env))

    def render_json(self, output):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.write(json.dumps(output))

    def render_raw_json(self, output):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.write(output)

def get_handlers():
    return _handlers