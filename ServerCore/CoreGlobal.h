#pragma once
//전역으로 사용할 매니저

extern class ThreadManager* GThreadManager;

// 나중에 여러 매니저가 등장할 수도 있는데
// 순서가 있을 수 있음 : 생성, 소멸
// 그것을 CoreGlobal 내부에서 해줌
class CoreGlobal
{
public:
	CoreGlobal();
	~CoreGlobal();
};

