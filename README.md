# :poop: ShaRT

ShaRT is a live-streaming server which ingests over SRT and broadcasts over
SRT and HTTP. It is the suck-ccessor to [diarrhea](https://git.extremelycorporate.ca/chili-b/diarrhea)

## Dependencies

ShaRT depends on [SRT](https://github.com/Haivision/srt) version `1.4.4` and
[PicoHTTPParser](https://github.com/h2o/picohttpparser) against which it links
statically. It also depends on `libcrypto` and `libpthread` being available on
the host system.
