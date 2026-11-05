#pragma once
#include "Node.h"
namespace mg {
Outcome classifyResponse(int httpStatus,const char* body,size_t length,const char* deviceCode,const Sample& sent);
}
