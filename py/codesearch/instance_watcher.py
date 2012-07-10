import os
import signal
import sys
import time
import tornado.ioloop

class InstanceWatcher(object):

    def __init__(self, application, port):
        self.application = application
        self.port = port
        self.instances = {}

    def run_child(self, port):
        pid = os.fork()
        if pid:
            self.instances[pid] = port
        else:
            parent = False
            try:
                self.application.listen(port)
                tornado.ioloop.IOLoop.instance().start()
            finally:
                sys.exit(1)

    def run(self, count):
        for x in xrange(count):
            port = self.port + x
            self.run_child(port)

        try:
            while True:
                time.sleep(1)
                pid, status = os.wait()
                port = self.instances.pop(pid)
                self.run_child(port)
        except Exception, e:
            raise
            for pid in self.instances:
                os.kill(pid, signal.SIGTERM)
