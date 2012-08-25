import datetime
import email.utils
import gzip
import hashlib
import os
import time

from tornado import web
from codesearch import handler_meta

import pygments
import pygments.util
from pygments import lexers
from pygments import formatters

# You can generate this with: date '+%s'
CACHE_SERIAL = 1345923276
CACHE_SERIAL_TS = datetime.datetime.fromtimestamp(int(CACHE_SERIAL))

class PrettyPrintCache(object):
    """Since highlighting with pygments can be expensive, we maintain
    a filesystem cache. To clear the cache, you can simply delete the
    directory (or the files it contains).
    """

    def __init__(self, cache_dir):
        self.cache_dir = os.path.abspath(cache_dir)
        if not os.path.exists(cache_dir):
            os.makedirs(cache_dir)

    def get_highlighted(self, filename, hl_lines=None):
        """Get the highlighted version of a file."""
        hl_lines = sorted(hl_lines or [])
        st = os.stat(filename)
        key = '%s-%d-%s-%s' % (filename, int(st.st_mtime),
                               CACHE_SERIAL, hl_lines)
        key = os.path.join(self.cache_dir,
                           hashlib.sha1(key).hexdigest() + '.html.gz')
        try:
            with gzip.open(key) as keyfile:
                return keyfile.read()
        except IOError:
            with open(filename) as infile:
                file_data = infile.read()
            try:
                lexer = lexers.guess_lexer_for_filename(filename, file_data)
            except pygments.util.ClassNotFound:
                try:
                    lexer = lexers.guess_lexer(file_data)
                except pygments.util.ClassNotFound:
                    lexer = lexers.TextLexer()
            highlight = pygments.highlight(
                file_data, lexer, formatters.HtmlFormatter(
                    hl_lines=hl_lines, linenos='table', lineanchors='line',
                    anchorlinenos=True))
            with gzip.open(key, 'w') as keyfile:
                keyfile.write(highlight.encode('utf-8'))
            return highlight


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

        # If the CACHE_SERIAL has a value newer than the actual
        # on-disk file, prefer that time instead (for instance, we may
        # have made HTML formatting changes, that need to be picked
        # up).
        modified = max(modified, CACHE_SERIAL_TS)
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
        self.env['filepath'] = path
        realpath = self.get_fullpath(path)
        if realpath is None:
            return

        cache = PrettyPrintCache('/var/codesearch/pp-cache')

        t0 = time.time()
        self.env['formatted_code'] = cache.get_highlighted(realpath)
        self.env['elapsed_ms'] = '%1.1f' % (1000 * (time.time() - t0),)

        self.render('pretty_print.html')
