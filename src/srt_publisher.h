#ifndef SRT_PUBLISHER_H_
#define SRT_PUBLISHER_H_

#include "srt.h"
#include "published_stream.h"
#include "authenticator.h"

#ifndef MAX_WEB_SEND_FAILS
#define MAX_WEB_SEND_FAILS 20
#endif

void * srt_publisher(void * _d);

#endif
