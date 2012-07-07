.PHONY: all
all:
	make -C build

.PHONY: proto
proto:
	protoc -Isrc --cpp_out=src src/index.proto
	protoc -Isrc --python_out=py/codesearch src/index.proto
