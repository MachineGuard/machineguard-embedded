#pragma once
#include "Node.h"
namespace mg {
struct HttpHeaders {int status=0;size_t bodyLength=0;};
bool parseHttpHeaders(const char* text,size_t length,HttpHeaders& output);
}
