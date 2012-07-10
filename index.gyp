# -*- python -*-
{
    'variables': {
        'reader_sources': [
            'src/context.cc',
            'src/integer_index_reader.cc',
            'src/mmap.cc',
            'src/ngram_index_reader.cc',
            'src/search_results.cc',
            'src/sstable_reader.cc',
        ],
        'writer_sources': [
            'src/index_writer.cc',
            'src/ngram_index_writer.cc',
            'src/sstable_writer.cc',
        ],
        'rpc_sources': [
            'src/rpcserver.cc',
        ],
    },
    'target_defaults': {
        'cflags': ['-pedantic', '-Wall', '-std=c++11', '-O0', '-g',
                   '-I$${HOME}/code/re2',
               ],
        'defines': [
            'USE_SNAPPY',
            'USE_THREADS',
            'USE_MADV_RANDOM',
        ],
        'libraries': [
            '-pthread',
            '-lboost_filesystem',
            '-lboost_system',
            '-lboost_program_options',
            '-lprotobuf',
            '-lsnappy',
            '/usr/lib64/libtcmalloc.so.4',
            '../libre2.a',
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
            'target_name': 'cindex',
            'sources': [
                '<@(writer_sources)',
                'src/cindex.cc',
            ],
        },
        {
            'type': 'executable',
            'target_name': 'csearch',
            'sources': [
                '<@(reader_sources)',
                'src/csearch.cc',
            ],
        },
        {
            'type': 'executable',
            'target_name': 'rpcserver',
            'sources': [
                '<@(reader_sources)',
                '<@(rpc_sources)',
                'src/crpcserver.cc',
            ],
        },
    ],
}
