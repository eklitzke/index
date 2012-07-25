from codesearch import rpc
from codesearch import index_pb2

# N.B. there's a circular reference while pools/clients are alive, but
# that's OK because pools are never collected, and clients destroy
# their circular references when finished.

class SearchRpcClient(rpc.RpcClient):

    _instance = None

    def __init__(self, host='127.0.0.1', port=9900, io_loop=None, pool=None):
        super(SearchRpcClient, self).__init__(host, port, io_loop)
        self.pool = pool
        self.pending_callbacks = {}

    @classmethod
    def instance(cls, host='127.0.0.1', port=9900):
        if cls._instance is None:
            cls._instance = cls(host, port)
            return cls._instance

    def search(self, query, cb, limit=40, offset=0):
        request = index_pb2.SearchQueryRequest()
        request.query = query
        request.limit = limit
        request.offset = offset
        self.send(request)
        self.recv(cb)

    def break_reference(self):
        """Break the (potentially) circular reference to the pool."""
        self.pool = None

    def onclose(self):
        if self.pool is not None:
            self.pool.remove(self)
            self.break_reference()

class SearchRpcPool(object):
    """A bounded pool for search RPC clients."""

    _instance = None

    def __init__(self, max_size=16):
        self.max_size = max_size
        self.idle = set()
        self.busy = set()

    @classmethod
    def instance(cls):
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance

    def remove(self, client):
        try:
            self.idle.remove(client)
        except KeyError:
            pass

    def acquire(self):
        try:
            client = self.idle.pop()
        except KeyError:
            client = SearchRpcClient(pool=self)
        self.busy.add(client)
        return client

    def release(self, client):
        if client not in self.busy:
            return
        self.busy.remove(client)
        if (not client.closed() and
            len(self.idle) + len(self.busy) < self.max_size):
            self.idle.add(client)
        else:
            client.break_reference()
