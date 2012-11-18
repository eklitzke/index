CC_PROTO_FILES := src/index.pb.h src/index.pb.cc
ALL_PROTO_FILES := py/codesearch/index_pb2.py $(CC_PROTO_FILES)

.PHONY: cxx
cxx: $(CC_PROTO_FILES)
	make -C build

py/codesearch/index_pb2.py: src/index.proto
	protoc -Isrc --python_out=py/codesearch src/index.proto

src/index.pb.h src/index.pb.cc: src/index.proto
	protoc -Isrc --cpp_out=src src/index.proto

.PHONY: proto
proto: $(ALL_PROTO_FILES)

.PHONY: css
css:
	./scripts/make_css

# Make sure *everything* is built. Since this forces the .proto files
# to be built, this will essentially trigger a full rebuild.
.PHONY: full
full: proto cxx css
