import optparse
import os

from tornado import ioloop
from tornado import web

from codesearch import instance_watcher
from codesearch import filesystem
from codesearch import handler_meta
from codesearch import handlers  # for side effects!
from codesearch import index_pb2

if __name__ == "__main__":
    parser = optparse.OptionParser()
    parser.add_option('-l', '--local', default=False, action='store_true',
                      help='Run the application entirely locally')
    parser.add_option('-p', '--port', type='int', default=9000,
                      help='The port to bind on')
    parser.add_option('--index-directory', default='/tmp/index',
                      help='The index directory to use')
    parser.add_option('--prod', dest='debug', action='store_false',
                      default=True, help='Do not run in debug mode')
    parser.add_option('--instances', type='int', default=8,
                      help='How many instances to run in prod')
    opts, args = parser.parse_args()
    settings = {
        'index_directory': opts.index_directory,
        'static_path': filesystem.get_static_dir(),
        'template_path': filesystem.get_template_dir(),
        'debug': opts.debug,
        'run_locally': opts.local
    }
    meta_config = index_pb2.MetaIndexConfig()
    with open(os.path.join(opts.index_directory, 'meta_config')) as f:
        meta_config.ParseFromString(f.read())
    settings['vestibule'] = meta_config.vestibule
    application = web.Application(handler_meta.get_handlers(), **settings)
    try:
        if opts.debug:
            application.listen(opts.port)
            ioloop.IOLoop.instance().start()
        else:
            watcher = instance_watcher.InstanceWatcher(application, opts.port)
            watcher.run(opts.instances)
    except KeyboardInterrupt:
        pass
