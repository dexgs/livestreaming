# :poop: ShaRT

ShaRT is a live-streaming server which ingests over SRT and broadcasts over
SRT and HTTP licensed under the GPLv3.
It is the suck-ccessor to [diarrhea](https://git.extremelycorporate.ca/chili-b/diarrhea)

## Dependencies

ShaRT depends on [SRT](https://github.com/Haivision/srt) version `1.4.4` and
[PicoHTTPParser](https://github.com/h2o/picohttpparser) against which it links
statically. It also depends on `openssl`, `libcrypto` and `libpthread` being
available on the host system.

## Building

Clone the repository
(pass `--recurse-submodules` or run 
`git submodule update --init --recursive` in the project directory after cloning)
and run `make all` in its directory. Make sure you have `cmake` installed
(required to build SRT). An executable will be output at `bin/ShaRT`.

The following values can be defined at compile time:

- `WEB_LISTEN_BACKLOG` : `int` - Connection backlog size for HTTP
- `SRT_LISTEN_BACKLOG` : `int` - Connection backlog size for SRT
- `MAP_SIZE` : `int` - Number of "buckets" in the hashmap which stores stream information.
  You should set this to *at least* 2x the maximum number of streams you expect to handle.
- `UNLISTED_STREAM_NAME_PREFIX` : `string` - Streams published under a name beginning with
  this string will not be reported by the web API (default value is `_`).

## Usage

See [USAGE.md](USAGE.md)

## Notes

- Connections for watching a stream are handled in non-blocking mode and traffic is managed
"leaky bucket" style, i.e. if ShaRT can't broadcast the stream to connected clients in time
with the rate at which data arrives, it will drop connections until it can.

## Web Player

An example webpage for playing live streams using [mpegts.js](https://github.com/xqq/mpegts.js)
is provided. To use it, serve the `www` directory with a web server and set the
value of `stream_backend_url` to the address at which `ShaRT` accepts HTTP
connections, i.e. if `ShaRT` is accessible over HTTP at
`http://example.com/streams`, set that as the value of `stream_backend_url`.

## Web API

The following HTTP endpoints are exposed:

- `/api/streams/N` Returns a JSON-encoded list of stream names with length `N`
  sorted by number of viewers. If `N` is `0`, the full stream list will be returned.
- `/api/stream/NAME` Returns the number of viewers for `NAME`. Returns `404` if
  there is no stream called `NAME` being published.
