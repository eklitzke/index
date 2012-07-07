# -*- python -*-
{
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
        'src/index_reader.cc',
        'src/index.pb.cc',
        'src/shard_reader.cc',
        'src/sstable_reader.cc',
        'src/util.cc',
    ],
  },
  'targets': [
    {
      'type': 'executable',
      'target_name': 'cindex',
      'sources': [
        'src/index_writer.cc',
        'src/posting_list.cc',
        'src/shard_writer.cc',
        'src/sstable_writer.cc',
        'src/cindex.cc',
      ],
    },
    {
      'type': 'executable',
        'target_name': 'csearch',
        'sources': [
            'src/csearch.cc',
        ],
    },
    {
      'type': 'executable',
        'target_name': 'crpcserver',
        'sources': [
            'src/rpcserver.cc',
            'src/crpcserver.cc',
        ],
    },
  ],
}
