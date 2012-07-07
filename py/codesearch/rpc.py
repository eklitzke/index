import socket
import struct
import tornado.ioloop
import tornado.iostream
from codesearch import index_pb2

_size_fmt = '!Q'
assert(struct.calcsize(_size_fmt) == 8)

class _incrementer(object):

    def __init__(self):
        self.val = 0

    def inc(self):
        ret = self.val
        self.val += 1
        return ret

incrementer = _incrementer()


class RpcClient(object):

    def __init__(self, host, port, io_loop=None):
        self.host = host
        self.port = port
        self.io_loop = io_loop or tornado.ioloop.IOLoop()

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        self.stream = tornado.iostream.IOStream(s)
        self.stream.connect((host, port))

    def send(self, protobuf):
        protobuf_type = type(protobuf)
        rpc_request = index_pb2.RPCRequest()
        rpc_request.request_num = incrementer.inc()
        if protobuf_type is index_pb2.SearchQueryRequest:
            rpc_request.search_query.MergeFrom(protobuf)
        else:
            raise ValueError('Unknown protobuf %r' % (protobuf_type,))
        serialized_data = rpc_request.SerializeToString()
        self.stream.write(struct.pack(_size_fmt, len(serialized_data)) +
                          serialized_data)

    def recv(self, callback):
        def unpack_callback(data):
            container_msg = index_pb2.RPCResponse()
            container_msg.MergeFromString(data)
            if container_msg.HasField("search_response"):
                callback(container_msg.search_response)
            else:
                raise ValueError('Unable to parse RPC response!')

        def size_callback(data):
            size, = struct.unpack(_size_fmt, data)
            self.stream.read_bytes(size, unpack_callback)

        self.stream.read_bytes(8, size_callback)

    def close(self):
        self.stream.close()

    def __del__(self):
        self.stream.close()
