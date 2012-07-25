import optparse
import tornado.ioloop

from codesearch import instance_watcher
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
    parser.add_option('--instances', type='int', default=8,
                      help='How many instances to run in prod')
    opts, args = parser.parse_args()
    application = tornado.web.Application(
        handler_meta.get_handlers(),
        static_path=filesystem.get_static_dir(),
        template_path=filesystem.get_template_dir(),
        debug=opts.debug,
        remote=not opts.local)

    try:
        if opts.debug:
            application.listen(opts.port)
            tornado.ioloop.IOLoop.instance().start()
        else:
            watcher = instance_watcher.InstanceWatcher(application, opts.port)
            watcher.run(opts.instances)
    except KeyboardInterrupt:
        pass
