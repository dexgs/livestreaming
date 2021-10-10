# :poop: ShaRT

ShaRT is a live-streaming server which ingests over SRT and broadcasts over
SRT and WebRTC. It is the suck-ccessor to [diarrhea](https://git.extremelycorporate.ca/chili-b/diarrhea)

## Dependencies

ShaRT depends on [SRT](https://github.com/Haivision/srt) version `1.4.4` and 
[libdatachannel](https://github.com/paullouisageneau/libdatachannel) version
`0.15.3`. It links against both statically, but requires their respective
dependencies to be available on the system.
