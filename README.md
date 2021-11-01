# :poop: ShaRT

ShaRT is a live-streaming server which ingests over SRT and broadcasts over
SRT and HTTP licensed under the GPLv3.
It is the suck-ccessor to
[diarrhea](https://git.extremelycorporate.ca/chili-b/diarrhea)

## Dependencies

ShaRT depends on [SRT](https://github.com/Haivision/srt) version `1.4.4` and
[PicoHTTPParser](https://github.com/h2o/picohttpparser) against which it links
statically. It also depends on `openssl`, `libcrypto` and `libpthread` being
available on the host system. You also need `cmake` installed in order to
compile SRT.

## Building

Clone the repository
(pass `--recurse-submodules` or run 
`git submodule update --init --recursive` in the project directory after
cloning) and run `make all` in its directory. Make sure you have `cmake`
installed (required to build SRT). An executable will be output at `bin/ShaRT`.

The following values can be defined at compile time:

- `WEB_LISTEN_BACKLOG` : `int` - Connection backlog size for HTTP
- `SRT_LISTEN_BACKLOG` : `int` - Connection backlog size for SRT
- `MAP_SIZE` : `int` - Number of "buckets" in the hashmap which stores stream
  information. You should set this to *at least* double the maximum number of
  streams you expect to handle to get the best possible performance.
- `UNLISTED_STREAM_NAME_PREFIX` : `string` - Streams published under a name
  beginning with this string will not be reported by the web API
  (default value is `_`).
- `MAX_PACKETS_IN_FLIGHT` : `int`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_FC)
- `SEND_BUFFER_SIZE` : `int`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_SNDBUF)
- `RECV_BUFFER_SIZE` : `int`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_RCVBUF)
- `OVERHEAD_BW_PERCENT` : `int`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_OHEADBW)
- `LATENCY_MS` : `int`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_LATENCY)
- `ENABLE_TIMESTAMPS` : `bool`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_TSBPDMODE)
- `ENABLE_REPEATED_LOSS_REPORTS` : `bool`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_NAKREPORT)
- `ENABLE_DRIFT_TRACER` : `bool`
  [SRT docs](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#SRTO_DRIFTTRACER)
- `WEB_SEND_BUFFER_SIZE` : `int` - Like `SEND_BUFFER_SIZE`, but for HTTP
  connections.

## Docker

A configuration to run ShaRT under Docker is provided. By default, it includes an instance of
[Lighttpd](https://www.lighttpd.net/) to serve the web interface.

### Build and run container WITH Lighttpd

Run `HOSTNAME=your-hostname docker-compose build && docker-compose up` where
`your-hostname` is replaced with the hostname at which the web interface will be reachable
by an end-user (NOT the address at which it is reachable on the local machine).

The web player will be accessible at `your-hostname:3000` and ShaRT will listen for
SRT publishers on port `9991` and SRT subscribers on port `1234`. To change these
settings as well as others, modify the relevant values in `docker-compose.yml`.

### Build and run container WITHOUT Lighttpd

Run `docker-compose build shart && docker-compose up shart`

ShaRT will listen for SRT publishers on port `9991` and SRT subscribers on port `1234`.
To change these settings as well as others, modify the relevant values in `docker-compose.yml`.

## Usage

See [USAGE.md](USAGE.md)

## Notes

- Connections for watching a stream are handled in non-blocking mode and traffic
  is managed "[leaky bucket](https://en.wikipedia.org/wiki/Leaky_bucket)"
  style, i.e. if ShaRT can't broadcast the stream to connected clients in time
  with the rate at which data arrives, it will drop connections until it can.
- Stream names may not contain the following characters:
  `$`, `(`, `)`, `[`, `]`, `<`, `>`, `|`, `\n` (newline), `\`, `&`, `*`, `#`,
  `~`, `!`, `` ` ``, `;`, `'`, `"`.

## Web Player

An example webpage for playing live streams using
[mpegts.js](https://github.com/xqq/mpegts.js)
is provided. To use it, do the following:

1. Serve the `www` directory with a web server.
2. Set the values of `web_url` and `srt_url` in `www/config.js` to the URLs at
  which ShaRT is configured to accept HTTP and SRT subscriber connections
  respectively. You must do this because ShaRT itself is served separately from
  any web interface to which it is attached.

Here is an example NGINX configuration for serving the sample web interface as
well as proxying HTTP connections to ShaRT (Assuming ShaRT serves HTTP
connections over the default port, 8071):

```nginx
location /stream {
  alias /path/to/ShaRT/www;
}

location /stream_backend/ {
  proxy_http_version 1.1;
  proxy_pass http://127.0.0.1:8071/;
  proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
}
```

## Web API

The following HTTP endpoints are exposed:

- `/api/streams/N` Returns a JSON-encoded list of stream names with length `N`
  sorted by number of viewers. If `N` is `0`, the full stream list will be
  returned.
- `/api/stream/NAME` Returns the number of viewers for `NAME`. Returns `404` if
  there is no stream called `NAME` being published.
