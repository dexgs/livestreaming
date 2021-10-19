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

ShaRT accepts the following command line arguments:

|`--srt-publish-port PORT`| listen for incoming SRT broadcasts on port `PORT`.|
|`--srt-publish-passphrase STRING`| set SRT encryption passphrase for published
    streams to STRING. Broadcasters will have to set the `passphrase` option to
    STRING. (Example: If `ShaRT` was started with `--srt-publish-port 9991` and
    `--srt-publish-passphase a_good_password`, to broadcast to the server from
    the computer on which it is running, you would set the stream url to 
    `srt://127.0.0.1:9991?streamid=stream_name&passphrase=a_good_password`)|
|`--srt-subscribe-port PORT`| listen for incoming SRT stream watchers on port
    `PORT`.|
|`--srt-subscribe-passphrase STRING`| The same as `--srt-publish-passphrase`
    except it applies to connections that are watching a stream rather than
    broadcasting it.|
|`--web-port PORT`| Listen for incoming HTTP connections on port `PORT`.|
|`--max-streams NUMBER`| Allow at most `NUMBER` stream(s) to be published at
    once. Set this value to `0` for no limit.|
|`--max-subscribers NUMBER`| Allow at most `NUMBER` watchers of any given
    stream. Set this value to `0` for no limit.|
|`--max-pending-connections NUMBER`| Allow at most `NUMBER` incoming connections
    at once. Set this value to `0` for no limit.|
|`--auth-command COMMAND`| Command to execute to authenticate connections. It
    will be called as such: `COMMAND TYPE ADDRESS NAME` where `COMMAND` is the
    command to run, `TYPE` is either `PUBLISH` or `SUBSCRIBE` depending on the
    type of connection, `ADDRESS` is the IP address of the incoming connection
    and `NAME` is the name of the stream requested. If `COMMAND` exits with
    status `0`, the connection is accepted. If it exits with status `1`, it is
    rejected. If the command writes anything to standard output, that output
    will replace whatever the initial stream name was. For example, if `COMMAND`
    was `echo -n foo && exit 0`, no matter what the name of the requested stream
    is, it will be set to `foo` by the server. **IMPORTANT:** When using this
    feature, be aware that newline characters are not removed from the end of
    the output of `COMMAND`.|
|`--read-web-ip-from-headers y/n`| If `y` is passed, the server will read the
    value of the `X-Forwarded-For` header on HTTP requests and use that as the
    effective IP address of incoming connections. Use this if you are running
    behind a proxy and still want to use IP addresses with `--auth-command`.|
