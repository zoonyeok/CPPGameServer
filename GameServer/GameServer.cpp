#include "pch.h"
#include <iostream>
#include "CorePch.h"
#include <thread>
#include <mutex>
#include <windows.h>
#include <atomic>
#include "ConcurrentQueue.h"
#include "ConcurrentStack.h"
#include "CoreMacro.h"
#include "ThreadManager.h"

LockQueue<int32> q;
LockStack<int32> s;

void Push()
{
	while (true)
	{
		int32 value = rand() % 100;
		q.Push(value);

		this_thread::sleep_for(10ms);
	}
}

void Pop()
{
	while (true)
	{
		int32 data = 0;
		if (q.TryPop(OUT data))
			cout << data << endl;
	}
}

// 생성자가 알아서 호출, 매니저를 초기화 날리는 부분을 자동으로 실행
CoreGlobal Core;

void ThreadMain()
{
	while (true)
	{
		cout << "Hello ! I am thread..." << LThreadId << endl;
		this_thread::sleep_for(1s);
	}
}


int main()
{
	/*thread t1(Push);
	thread t2(Pop);
	thread t3(Pop);

	t1.join();
	t2.join();
	t3.join();*/


}

