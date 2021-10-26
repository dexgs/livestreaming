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

## Usage

See [USAGE.md](USAGE.md)

## Web Player

An example webpage for playing live streams using [mpegts.js](https://github.com/xqq/mpegts.js)
is provided. To use it, serve the `www` directory with a web server and set the
value of `stream_backend_url` to the address at which `ShaRT` accepts HTTP
connections, i.e. if `ShaRT` is accessible over HTTP at
`http://example.com/streams`, set that as the value of `stream_backend_url`.

## Web API

The following HTTP endpoints are exposed:

- `/api/streams/N` Returns a JSON-encoded list of stream names with length `N`
  sorted by number of viewers.
- `/api/stream/NAME` Returns the number of viewers for `NAME`. Returns `404` if
  there is no stream called `NAME` being published.
