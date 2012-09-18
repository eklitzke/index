# -*- python -*-
{
    'variables': {
        'common_sources': [
            'src/config.cc',
            'src/context.cc',
            'src/mmap.cc',
        ],
        'reader_sources': [
            'src/file_util.cc',
            'src/integer_index_reader.cc',
            'src/ngram_index_reader.cc',
            'src/ngram_table_reader.cc',
            'src/search_results.cc',
        ],
        'writer_sources': [
            'src/file_util.cc',
            'src/index_writer.cc',
            'src/ngram_counter.cc',
            'src/ngram_index_writer.cc',
            'src/sstable_writer.cc',
        ],
        'rpc_sources': [
            'src/rpcserver.cc',
        ],
    },
    'target_defaults': {
        'cflags': ['-pedantic',
                   '-Wall',
                   '-std=c++11',
                   '-O3',
                   #'-finstrument-functions',
                   '-flto',
                   '-g',
                   # see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43052
                   #'-fno-builtin-memcmp',
                   ],
        'defines': [
            '_GNU_SOURCE',
            #'ENABLE_SLOW_ASSERTS',
            'USE_MADV_RANDOM',
            'USE_THREADS',
        ],
        'libraries': [
            '-pthread',
            '/usr/lib64/libtcmalloc.so.4',  # disable if using valgrind/massif
            '/usr/lib64/libprofiler.so.0',
            '-lboost_filesystem',
            '-lboost_system',
            '-lboost_program_options',
            '-lglog',
            '-lprotobuf',
            '-lsnappy',
            #'../libre2.a',
        ],
        'sources': [
            'src/index.pb.cc',
            'src/util.cc',
        ],
    },
    'targets': [
        {
            'type': 'executable',
            'target_name': 'inspect_shard',
            'sources': [
                'src/index.pb.cc',
                'src/util.cc',
                'src/inspect_shard.cc',
            ],
        },
        {
            'type': 'executable',
            'target_name': 'fsck_sst',
            'sources': [
                'src/index.pb.cc',
                'src/util.cc',
                'src/fsck_sst.cc'
            ],
        },
        {
            'type': 'executable',
            'target_name': 'print_ngram_counts',
            'sources': [
                'src/index.pb.cc',
                'src/util.cc',
                'src/print_ngram_counts.cc',
            ],
        },
        {
            'type': 'executable',
            'target_name': 'bench',
            'sources': [
                'src/config.cc',
                'src/bench.cc',
            ],
        },
        {
            'type': 'executable',
            'target_name': 'cindex',
            'sources': [
                '<@(common_sources)',
                '<@(writer_sources)',
                'src/cindex.cc',
            ],
        },
        {
            'type': 'executable',
            'target_name': 'csearch',
            'sources': [
                '<@(common_sources)',
                '<@(reader_sources)',
                'src/csearch.cc',
            ],
        },
        {
            'type': 'executable',
            'target_name': 'rpcserver',
            'sources': [
                '<@(common_sources)',
                '<@(reader_sources)',
                '<@(rpc_sources)',
                'src/crpcserver.cc',
            ],
        },
    ],
}
