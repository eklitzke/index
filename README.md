Code Search
===========

This is an implementation of a code search engine.

Installing
----------

The default (and recommended) location for the codesearch index is at
`/var/codesearch`. A number of files and directories will be created
in this directory, which represent the index contents. If you are
setting up codesearch for the first time, you probably need to
manually create this directory, as typically non-root users are not
allowed to create new directories under `/var`. (You are also free to
use another directory, by passing a command line switch, typically
`--db-path`, to the various codesearch commands.)

To compile, invoke `./run_gyp` and then `make`.

Indexing
--------

Index things like this:

    cindex [--replace] /path/to/source-code
    
Searching
---------

Search from the command line like this: `csearch mysearchterm`.

To run the web component of codesearch, invoke `rpcserver` and then
run `./web` for dev, or `./web_prod` for prod.

SSTables
========

This section describes the format of an SSTable.

An SSTable exists in its own directory. There will be a file named
`config`, in that directory, and one or more files with `.sst` table
extensions. The `config` file will contain various metadata about the
index. In particular, it will hold the key size (see the definition of
`IndexConfig` in `index.proto` for the details).

A `.sst` file starts with a `SSTableHeader` protobuf message,
followed by some padding bytes, and then a sequence of key/offset
pairs. The header holds some data about how large the file should be
(to make it easy to detect truncations), metadata about the key size,
and optionally a range of keys. Generally you can use the binary at
`bin/inspect_shard` to inspect `.sst` files, which primarily works by
inspecting the `SSTableHeader` message.

After the header and alignment bytes, there is a series of key/offset
pairs. For instance, for an `ngrams` index, we might have a sequence
of keys like `aaa`, `aab`, `aac`, etc. (of course, in reality we will
include many values lexicographically smaller than `a`, including
possibly binary data!). In every table, the keys will be
word-aligned. In every case, the keys in the table will be stored
lexicographically, and tables that store numeric keys (like the
`lines` and `files` indexes) will be big-endian encoded, which makes
numeric comparisons equivalent to lexicographic comparisons.

In the key/offset section of the table, an offset is always the size
of a `std::size_t` (typically 64 bits). The offsets store the offset
in the table after the key/offset section; the size of that section is
stored in the `SSTableHeader`.

When looking up a key, which is done by binary search, one seeks to
the appropriate offset into the file. Then a value of size
`std::uint64_t` is read, which contains the size of the message. Then
the message is read immediately following the size value. Typically
the message is compressed with Snappy, and will then be decompressed.

For searching numeric indexes, in particular the `lines` and `files`
indexes, there is metadata in the `SSTableHeader` section that allows
determination of whether a given key will be in the table ahead of
time. This does not apply to the `ngrams` index (because of how
indexing works for ngrams).

