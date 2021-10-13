#ifndef SRT_SUBSCRIBER_H_
#define SRT_SUBSCRIBER_H_

#include "thirdparty/srt/srt.h"
#include "published_stream.h"
#include "authenticator.h"

void * run_srt_subscriber(void * _d);

#endif
