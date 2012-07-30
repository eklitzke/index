.PHONY: cxx
cxx:
	make -C build

.PHONY: proto
proto:
	protoc -Isrc --cpp_out=src src/index.proto
	protoc -Isrc --python_out=py/codesearch src/index.proto

.PHONY: css
css:
	./scripts/make_css

# Make sure *everything* is built. Since this forces the .proto files
# to be built, this will essentially trigger a full rebuild.
.PHONY: full
full: proto cxx css
