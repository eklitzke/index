# -*- python -*-
{
  'target_defaults': {
    'cflags': ['-pedantic', '-Wall', '-std=c++11', '-O3',
               '-I$${HOME}/code/leveldb/include',
               '-I$${HOME}/code/re2',
    ],
    'libraries': [
      '-pthread',
      '-lboost_system',
      '-lboost_program_options',
      '-lprotobuf',
      '-lsnappy',
      '../libleveldb.a',
      '../libre2.a',
    ],
    'sources': [
        'src/index_reader.cc',
        'src/index_writer.cc',
        'src/index.pb.cc',
        'src/posting_list.cc',
        'src/shard_reader.cc',
        'src/shard_writer.cc',
        'src/util.cc',
    ],
  },
  'targets': [
    {
      'type': 'executable',
      'target_name': 'cindex',
      'cflags': ['-g'],
      'libraries': [
        '-lboost_filesystem',
      ],
      'sources': [
        'src/cindex.cc',
      ],
    },
    {
      'type': 'executable',
        'target_name': 'csearch',
        'cflags': ['-g'],
        'sources': [
          'src/csearch.cc',
        ],
    },
  ],
}
