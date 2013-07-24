#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "uv.h"
#include "http_parser.h"
// posix only
#include <sys/resource.h> // setrlimit, getrlimit
#include <string>
#include <iostream>
#include <sstream>


static uv_loop_t* uv_loop;
static http_parser_settings req_parser_settings;
static int request_num = 1;
// http://serverfault.com/questions/145907/does-mac-os-x-throttle-the-rate-of-socket-creation
//static int req_num = 16000;
static int req_num = 100;

#define CHECK(status, msg) \
  if (status != 0) { \
    fprintf(stderr, "%s: %s\n", msg, uv_err_name(status)); \
    exit(1); \
  }
#define UVERR(err, msg) fprintf(stderr, "%s: %s\n", msg, uv_strerror(err))
#define LOG_ERROR(msg) puts(msg);
#define LOG(msg) puts(msg);
#define LOGF(fmt, params...) printf(fmt "\n", params);

struct client_t {
  client_t() :
    body() {}
  http_parser parser;
  int request_num;
  uv_tcp_t tcp;
  uv_connect_t connect_req;
  uv_shutdown_t shutdown_req;
  uv_write_t write_req;
  std::stringstream body;
};

uv_buf_t alloc_buffer(uv_handle_t *handle, size_t suggested_size) {
    return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void on_close(uv_handle_t* handle) {
  client_t* client = (client_t*) handle->data;
  LOG("on close")
  //client->tcp.data = NULL;
  delete client;
}

void on_read(uv_stream_t *tcp, ssize_t nread, uv_buf_t buf) {
    size_t parsed;
    client_t* client = (client_t*) tcp->data;
    LOGF("on read: %ld",nread);
    LOGF("on read buf.size: %ld",buf.len);
    if (nread > 0) {
        http_parser * parser = &client->parser;
        /*if (parser->http_errno == HPE_PAUSED) {
             LOG("PAUSED");
             return ;
        }*/
        parsed = http_parser_execute(parser, &req_parser_settings, buf.base, nread);
        if (parser->upgrade) {
            LOG("We do not support upgrades yet")
        }
        else if (parsed != nread) {
          LOGF("parsed incomplete data::%ld/%ld bytes parsed\n", parsed, nread);
          LOGF("\n*** %s ***\n\n",
              http_errno_description(HTTP_PARSER_ERRNO(parser)));
        }
    } else {
        if (nread != UV_EOF) {
          UVERR(nread, "read");
        }
    }
    // if not keepalive
    //if (!http_should_keep_alive(parser)) {
        //if (!uv_is_closing((uv_handle_t*)tcp))
            //uv_close((uv_handle_t*)tcp, on_close);
    //}
    free(buf.base);
}

void after_write(uv_write_t* req, int status) {
    LOG("after write")
    CHECK(status, "write");
}

void on_connect(uv_connect_t *req, int status) {
    client_t *client = (client_t*)req->handle->data;

    if (status == -1) {
        fprintf(stderr, "connect failed error %s\n", uv_err_name(status));
        uv_close((uv_handle_t*)req->handle, on_close);
        return;
    }
  
    client->request_num++;

    LOGF("[ %5d ] new connection", request_num);

    uv_buf_t resbuf;
    std::string res =
         "GET /hello HTTP/1.1\r\n"
         "Host: 0.0.0.0=8000\r\n"
         "User-Agent: webclient.c\r\n"
         "Keep-Alive: 100\r\n"
         "Connection: keep-alive\r\n"
         "\r\n";
    resbuf.base = (char *)res.c_str();
    resbuf.len = res.size();

    int rr = uv_read_start(req->handle, alloc_buffer, on_read);
    CHECK(rr, "bind");
    
    int r = uv_write(&client->write_req,
            req->handle,
            &resbuf,
            1,
            after_write);
    CHECK(r, "bind");
}

int on_message_begin(http_parser* /*parser*/) {
  printf("\n***MESSAGE BEGIN***\n");
  return 0;
}

int on_headers_complete(http_parser* /*parser*/) {
  printf("\n***HEADERS COMPLETE***\n");
  return 0;
}

int on_url(http_parser* /*parser*/, const char* at, size_t length) {
  printf("Url: %.*s\n", (int)length, at);
  return 0;
}

int on_header_field(http_parser* /*parser*/, const char* at, size_t length) {
  printf("Header field: %.*s\n", (int)length, at);
  return 0;
}

int on_header_value(http_parser* /*parser*/, const char* at, size_t length) {
  printf("Header value: %.*s\n", (int)length, at);
  return 0;
}

int on_body(http_parser* parser, const char* at, size_t length) {
  printf("Body: %d\n", (int)length);
  client_t *client = (client_t*)parser->data;
  if (at && client)
  {
      client->body << std::string(at,length);
  }

  //fprintf("Body: %.*s\n", (int)length, at);
  /*if (http_body_is_final(parser)) {
      LOG("body is final!")
      std::cerr << "on-body len: " << length << "\n";
  } else {
      LOG("body not final\n")
      std::cerr << "non final on-body len: " << length << "\n";
  }*/

  return 0;
}

int on_message_complete(http_parser* parser) {
  printf("\n***MESSAGE COMPLETE***\n\n");
  client_t *client = (client_t*)parser->data;
  ssize_t total_len = client->body.str().size();
  LOGF("total length parsed: %ld",total_len)
  if (http_should_keep_alive(parser)) {
      printf("\n***SHOULD CLOSE keepalive HERE \n\n");
      uv_stream_t* tcp = (uv_stream_t*)&client->tcp;
      uv_close((uv_handle_t*)tcp, on_close);
  }
  return 0;
}

int main() {
    // mainly for osx, bump up ulimit
    struct rlimit limit;
    getrlimit(RLIMIT_NOFILE,&limit);
    LOGF("current ulimit: %lld",limit.rlim_cur);
    req_parser_settings.on_message_begin = on_message_begin;
    req_parser_settings.on_url = on_url;
    req_parser_settings.on_header_field = on_header_field;
    req_parser_settings.on_header_value = on_header_value;
    req_parser_settings.on_headers_complete = on_headers_complete;
    req_parser_settings.on_body = on_body;
    req_parser_settings.on_message_complete = on_message_complete;

    struct sockaddr_in dest = uv_ip4_addr("127.0.0.1", 8000);

    uv_loop = uv_default_loop();
    for (int i=0;i<req_num;++i) {
        client_t* client = new client_t();
        client->request_num = request_num;
        client->tcp.data = client;
        http_parser_init(&client->parser, HTTP_RESPONSE);
        client->parser.data = client;
        int r = uv_tcp_init(uv_loop, &client->tcp);
        CHECK(r, "bind");
        //r = uv_tcp_keepalive(&client->tcp,1,60);
        //CHECK(r, "bind");
        r = uv_tcp_connect(&client->connect_req, &client->tcp, dest, on_connect);
        CHECK(r, "bind");
        /*
         * This function runs the event loop. It will act differently depending on the
         * specified mode:
         *  - UV_RUN_DEFAULT: Runs the event loop until the reference count drops to
         *    zero. Always returns zero.
         *  - UV_RUN_ONCE: Poll for new events once. Note that this function blocks if
         *    there are no pending events. Returns zero when done (no active handles
         *    or requests left), or non-zero if more events are expected (meaning you
         *    should run the event loop again sometime in the future).
         *  - UV_RUN_NOWAIT: Poll for new events once but don't block if there are no
         *    pending events.
         */
         uv_run(uv_loop,UV_RUN_DEFAULT);
    }
    LOG("listening on port 8000");
}
