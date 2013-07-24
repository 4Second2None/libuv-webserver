{
  'includes': [ 'common.gypi' ],
  'targets': [
    {
      'target_name': 'webserver',
      'type': 'executable',
      'sources': [
        'webserver.cc',
      ],
      'dependencies': [
        './deps/libuv/uv.gyp:libuv',
        './deps/http-parser/http_parser.gyp:http_parser'
      ],
    },
    {
      'target_name': 'webclient',
      'type': 'executable',
      'sources': [
        'webclient.cc',
      ],
      'dependencies': [
        './deps/libuv/uv.gyp:libuv',
        './deps/http-parser/http_parser.gyp:http_parser'
      ],
    }
  ],
}