#include "pch.h"
#include "CoreTLS.h"

//쓰레드 ID를 직접 만들어 관리
thread_local uint32 LThreadId = 0;