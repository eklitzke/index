import optparse
import tornado.ioloop
from tornado import httpserver

from codesearch import filesystem
from codesearch import handler_meta
from codesearch import handlers  # for side effects!

if __name__ == "__main__":
    parser = optparse.OptionParser()
    parser.add_option('-l', '--local', default=False, action='store_true',
                      help='Run the application entirely locally')
    parser.add_option('-p', '--port', type='int', default=9000,
                      help='The port to bind on')
    parser.add_option('--prod', dest='debug', action='store_false',
                      default=True, help='Do not run in debug mode')
    opts, args = parser.parse_args()
    application = tornado.web.Application(
        handler_meta.get_handlers(),
        static_path=filesystem.get_static_dir(),
        template_path=filesystem.get_template_dir(),
        debug=opts.debug,
        remote=not opts.local)

    if opts.debug:
        application.listen(opts.port)
    else:
        server = httpserver.HTTPServer(application)
        server.bind(opts.port)
        server.start(0)  # Forks multiple sub-processes
    try:
        tornado.ioloop.IOLoop.instance().start()
    except KeyboardInterrupt:
        pass
