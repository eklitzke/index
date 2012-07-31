import os
import datetime
from tornado import web
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

class CodeFileHandler(handler_meta.RequestHandler):
    """This is the handler that reads files out of the vestibule, and
    serves the textual client back to a client.

    The convention is that a URL like /!foo/bar/baz.txt translates
    into the file "foo/bar/baz.txt" within the vestibule.
    """

    path = '/!(.*)'

    # Maximum size is 5 MB
    max_size = 5 << 20

    def head(self, path):
        self.get(path, include_body=False)

    def get(self, path, include_body=True):
        vestibule = self.settings['vestibule']
        realpath = os.path.realpath(os.path.join(vestibule, path))

        # Ensure that the file is within the vestibule. What we're
        # checking here is two things:
        #  * This isn't a symlink outside of the vestibule
        #  * There's no .. trickery going on to escape the vestibule
        if not realpath.startswith(vestibule):
            raise web.HTTPError(
                403, "I'm sorry, Dave. I'm afraid I can't do that")

        # The file is potentially in the vestibule (although at this
        # point, it could still not exit).
        try:
            st = os.stat(realpath)
        except (IOError, OSError):
            # the file doesn't exist
            raise web.HTTPError(404, 'Failed to find file /%s' % (path,))

        # Ensure the file isn't too large
        if st.st_size > self.max_size:
            raise web.HTTPError(
                403, 'File /%s is too large (%d bytes, max_size is %d bytes)'
                % (st.st_size, self.max_size))

        # We only index textual files (i.e., we never index binary),
        # so we always set the content-type to text/plain. Note that
        # this differs a bit from the tornado StaticFileHandler, which
        # will set a content-type based on the file extension.
        self.set_header('Content-Type', 'text/plain; charset=UTF-8')

        # Also note, that while we set last-modified, we do *not* send
        # an etag back (this is handled in the parent class).
        modified = datetime.datetime.fromtimestamp(st.st_mtime)
        self.set_header('Last-Modified', modified)
        self.set_header('Cache-Control', 'public')

        if include_body:
            with open(realpath, 'rb') as f:
                self.write(f.read())
        else:
            assert self.request.method.upper() == 'HEAD'
            self.set_header('Content-Length', st.st_size)

from codesearch import search  # for side-effects