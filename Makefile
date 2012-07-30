.PHONY: all
all: css
	make -C build

.PHONY: proto
proto:
	protoc -Isrc --cpp_out=src src/index.proto
	protoc -Isrc --python_out=py/codesearch src/index.proto

.PHONY: css
css:
	./bin/make_css
