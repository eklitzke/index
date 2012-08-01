import datetime
import email.utils
import os
import time

import pygments
from pygments import lexers
from pygments import formatters

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

class FileHandlerBase(handler_meta.RequestHandler):

    path = 'FIXME'

    # Maximum size is 1 MB
    max_size = 1 << 20

    def get_fullpath(self, path):
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
                % (path, st.st_size, self.max_size))

       # Also note, that while we set last-modified, we do *not* send
        # an etag back (this is handled in the parent class). We rely
        # entirely on conditional gets. We truncate milliseconds here
        # because they won't be sent back to the client, and if we
        # keep them it messes up the if-modified-since calculation below.
        modified = datetime.datetime.fromtimestamp(int(st.st_mtime))
        self.set_header('Last-Modified', modified)

        # Check the If-Modified-Since, and don't send the result if the
        # content has not been modified
        ims_value = self.request.headers.get('If-Modified-Since')
        if ims_value is not None:
            date_tuple = email.utils.parsedate(ims_value)
            if_since = datetime.datetime.fromtimestamp(time.mktime(date_tuple))
            if if_since >= modified:
                self.set_status(304)
                return

        return realpath

class RawFileHandler(FileHandlerBase):
    """This is the handler that reads files out of the vestibule, and
    serves the textual client back to a client.

    The convention is that a URL like /!foo/bar/baz.txt translates
    into the file "foo/bar/baz.txt" within the vestibule.
    """

    path = '/:(.*)'

    def head(self, path):
        self.get(path, include_body=False)

    def get(self, path, include_body=True):
        realpath = self.get_fullpath(path)
        if realpath is None:
            return

        # We only index textual files (i.e., we never index binary),
        # so we always set the content-type to text/plain. Note that
        # this differs a bit from the tornado StaticFileHandler, which
        # will set a content-type based on the file extension.
        self.set_header('Content-Type', 'text/plain; charset=UTF-8')

        if include_body:
            try:
                with open(realpath, 'rb') as f:
                    self.write(f.read())
            except IOError:
                raise web.HTTPError(404, 'Cannot open %r' % (path,))
        else:
            assert self.request.method.upper() == 'HEAD'
            self.set_header('Content-Length', st.st_size)

class PrettyPrintHandler(FileHandlerBase):

    path = '/!(.*)'

    def get(self, path):
        self.env['path'] = path
        realpath = self.get_fullpath(path)
        if realpath is None:
            return

        try:
            with open(realpath, 'rb') as f:
                file_data = f.read()
        except IOError:
            raise web.HTTPError(404, 'Cannot open %r' % (path,))

        t0 = time.time()
        lexer = lexers.guess_lexer_for_filename(path, file_data)
        self.env['formatted_code'] = pygments.highlight(
            file_data, lexer, formatters.HtmlFormatter(linenos='table'))
        self.env['elapsed_ms'] = '%1.1f' % (1000 * (time.time() - t0),)

        self.render('pretty_print.html')




from codesearch import search  # for side-effects
