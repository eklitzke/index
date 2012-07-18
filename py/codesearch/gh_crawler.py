import functools
import json
import re
import sys
import tornado.httpclient
import tornado.ioloop
import urllib
#from lxml.html import tostring, html5parser
from BeautifulSoup import BeautifulSoup

class URLFetcher(object):

    _instance = None

    def __init__(self, ioloop=None):
        self.ioloop = ioloop or tornado.ioloop.IOLoop.instance()
        self.http_client = tornado.httpclient.AsyncHTTPClient(self.ioloop)
        self.remaining = 0

    @classmethod
    def instance(cls):
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance

    def fetch(self, url, callback):
        self.remaining += 1
        self.http_client.fetch(
            url, functools.partial(self.fetch_callback, callback))

    def fetch_callback(self, real_callback, response):
        self.remaining -= 1
        if self.remaining == 0:
            self.ioloop.stop()
        real_callback(response)

class LanguagesDetector(object):

    def __init__(self):
        self._languages = set()

    def _parse_languages_html(self, response):
        languages_re = re.compile('href="/languages/(.*?)"')
        for line in response.buffer:
            m = languages_re.search(line)
            if m:
                name = urllib.unquote(m.groups()[0])
                self._languages.add(name)

    def fetch_languages(self):
        """Returns a list of GH languages."""
        URLFetcher.instance().fetch('https://github.com/languages/',
                                    self._parse_languages_html)

    @property
    def languages(self):
        assert self._languages
        return self._languages

class LanguageFinder(object):

    def __init__(self, language):
        self.language = language
        self.page = 0
        self.max_page = 0
        self._projects = []
        self.base_url = 'https://github.com/languages/'
        self.base_url += urllib.quote(self.language).replace('/', '%2F')
        self.base_url += '/most_watched'

    def get_top_projects(self, max_page=10):
        self.max_page = max_page
        self.loop()

    def _parse_response(self, response):
        print >> sys.stderr, response.effective_url
        if response.code != 200:
            print >> sys.stderr, 'WARNING: got %d on %s, retrying' % (
                response.code, response.effective_url)
            self.loop()
        else:
            project_re = re.compile('href="(.*?)"')
            soup = BeautifulSoup(response.body)
            for project in soup.findAll('td', **{'class': 'title'}):
                for child in project.findChildren():
                    for p in project_re.search(str(child)).groups():
                        self._projects.append(p)
            self.page += 1
            self.loop()

    def loop(self):
        if self.page >= self.max_page:
            return
        url = self.base_url
        if self.page > 0:
            url += '?page=%d' % (self.page,)
        URLFetcher.instance().fetch(url, self._parse_response)

    @property
    def projects(self):
        assert self._projects
        assert self.page >= self.max_page
        return self._projects


if __name__ == '__main__':
    ioloop = tornado.ioloop.IOLoop.instance()
    language_detector = LanguagesDetector()
    language_detector.fetch_languages()
    ioloop.start()

    language_finders = []
    for language in language_detector.languages:
        finder = LanguageFinder(language)
        finder.get_top_projects()
        language_finders.append(finder)
    ioloop.start()

    results = {}
    for finder in language_finders:
        results[finder.language] = finder.projects

    json.dump(results, sys.stdout)
    print
