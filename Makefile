.PHONY: all
all:
	make -C build

.PHONY: proto
proto:
	protoc -Isrc --cpp_out=src src/index.proto
