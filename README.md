# :poop: ShaRT

ShaRT is a live-streaming server which ingests over SRT and broadcasts over
SRT and HTTP. It is the suck-ccessor to [diarrhea](https://git.extremelycorporate.ca/chili-b/diarrhea)

## Dependencies

ShaRT depends on [SRT](https://github.com/Haivision/srt) version `1.4.4` and
[PicoHTTPParser](https://github.com/h2o/picohttpparser) against which it links
statically. It also depends on `libcrypto` and `libpthread` being available on
the host system.

## Building

Clone the repository and run `make all` in its directory. Make sure you have
`cmake` installed (required to build SRT).

## Usage

See [USAGE.md](USAGE.md)

## Web Player

An example webpage for playing live streams using [mpegts.js](https://github.com/xqq/mpegts.js)
is provided. To use it, serve the `www` directory with a web server and set the
value of `stream_backend_url` to the address at which `ShaRT` accepts HTTP
connections, i.e. if `ShaRT` is accessible over HTTP at
`http://example.com/streams`, set that as the value of `stream_backend_url`.
