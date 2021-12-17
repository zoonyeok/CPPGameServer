#pragma once
#include <mutex>

template<typename T>
class LockStack
{
public:
	LockStack() {};
	//복사,대입불가능
	LockStack(const LockStack&) = delete;
	LockStack& operator=(const LockStack&) = delete;

	void Push(T value)
	{
		lock_guard<mutex> lock(_mutex);
		_stack.push(std::move(value));
		_condVar.notify_one(); //대기하고 있는 애를 깨워줌
	}

	// 멀티스레드 환경에서는 Empty()가 큰 의미가 없음
	// Empty -> top -> pop 한 번에
	// 100프로 확률로 데이터를 뽑아오는것이 아니므로 bool을 리턴
	bool TryPop(T& value) 
	{
		lock_guard<mutex> lock(_mutex);
		if (_stack.empty())
			return false;

		value = std::move(_stack.top());
		_stack.pop();
		return true;
	}

	bool Empty()
	{
		lock_guard<mutex> lock(_mutex);
		return _stack.empty();
	}

	void WaitPop(T& value)
	{
		unique_lock<mutex> lock(_mutex);
		// 데이터가 있을 때까지 대기, 조건을 만족하지 않으면 (스택이 empty면) 락을 풀어주고 sleep, 다시 시그널이 오면 일어나서 락을 잡고 실행
		// 무한정으로 대기해야한다면 condition_variable을 사용하여 좀 구현하면 깔끔함
		_condVar.wait(lock, [this] {return _stack.empty() = false; }); 
		value = std::move(_stack.top());
		_stack.pop();
	}

private:
	stack<T> _stack;
	mutex _mutex;
	condition_variable _condVar;
};

template<typename T>
class LockFreeStack
{
	struct Node
	{
		Node(const T& value) 
			: data(value)
			, next(nullptr)
		{

		}

		T data;
		Node* next;
	};

public:

	// 1. 새 노드를 만들고
	// 2. 새 노드의 next = head
	// 3. head = 새 노드
	void Push(const T& value)
	{
		Node* node = new Node(value);
		node->next = _head;

		/*if (_head == node->next)
		{
			_head = node;
			return true;
		}
		else
		{
			node->next = _head;
			return false;
		}
		위 과정이 한 번에 원자적으로 일어남
		*/

		// 이 사이에 스레드를 뺏긴다면?
		// 원래 있던 값이 아닌 다른 값이 될 수도 있음!
		// 계속 뺏긴다면? -> LiveLock !
		// lockfree여도 대기상태가 완전히 없는 경우는 없음
		while (_head.compare_exchange_weak(node->next, node) == false)
		{
			// false라면 node->next = _head;
		}
		//_head = node;
	}

	// 1. head 읽기
	// 2. head->next 읽기
	// 3. head = head->next
	// 4. data 추출해서 반환
	// 5. 추출한 노드를 삭제
	bool TryPop(T& value)
	{
		++_popCount;

		Node* oldHead = _head;

		/*if (_head == oldHead)
		{
			_head = oldHead->next;
			return true;
		}
		else
		{
			oldHead = _head;
			return false;
		}
		*/

		//1,2,3
		while (oldHead && _head.compare_exchange_weak(oldHead, oldHead->next) == false)
		{

		}

		if (oldHead == nullptr)
		{
			--_popCount;
			return false;
		}
			

		//Exception 신경X , 터지면 터지게 냅두고 문제를 해결하는것이 낫다. ,원자로를 만드는 것이 아님
		value = oldHead->data;
		TryDelete(oldHead);
		// 멀티스레드 환경에서 동시에 TryPop()을 할 경우 이미 해당 포인터를 참조해 역참조로 값을 사용할 수도 있기 때문에
		// 그 메모리를 날릴 수 없음 , 알고리즘이 필요함. 잠시 삭제 보류
		// 누군가가 참조를 하지 않을 때까지 기다렸다가 삭제를 유도, 스마트 포인터처럼 PopCount를 추적
		//delete oldHead;

		// C#, java 같이 GC가 있었다면 여기서 끝나는 코드임
		return true;
	} 

	// 1) 데이터를 분리
	// 2) Count 체크
	// 3) 나 혼자면 삭제
	void TryDelete(Node* oldHead)
	{
		// 나 외에 누가 있는가?
		if (_popCount == 1)
		{
			// 나 혼자네?

			// 이왕 혼자인거, 삭제 예약된 다른 데이터들도 삭제해보자
			Node* node = _pendingList.exchange(nullptr);

			if (--_popCount == 0)
			{
				// 끼어든 애가 없음 -> 삭제 진행
				// 이제와서 끼어들어도 어차피 데이터는 분리해둔 상태 ㅋ
				DeleteNodes(node);
			}
			else if (node) // null 체크
			{
				// 누가 끼어들었으니 다시 갖다 놓자
				ChainPendingNodeList(node);
			}

			// 내 데이터는 삭제
			delete oldHead;
		}
		else
		{
			// 누가 있네 ? 그럼 지금 삭제하지 않고, 삭제 예약만
			ChainPendingNode(oldHead);
			--_popCount;
		}
	}

	//  [first][][][][][last] -> [p][][][][]
	//  [p,first][][][][][last][][][][][]
	void ChainPendingNodeList(Node* first, Node* last)
	{
		last->next = _pendingList;
		 
		while (_pendingList.compare_exchange_weak(last->next, first) == false)
		{
		}
	}

	// [node][][][][][][][] + [p][][][][][]
	void ChainPendingNodeList(Node* node)
	{
		// 마지막 노드를 찾은 뒤 ChainPendingNodeList 호출
		Node* last = node;
		while (last->next)
			last = last->next;

		ChainPendingNodeList(node, last);
	}

	// [] + [p][][][]
	void ChainPendingNode(Node* node)
	{
		ChainPendingNodeList(node, node);
	}

	static void DeleteNodes(Node* node)
	{
		while (node)
		{
			Node* next = node->next;
			delete node;
			node = next;
		}
	}


private:
	//스택의 top을 가르킴
	atomic<Node*> _head;

	atomic<uint32> _popCount = 0; // Pop을 실행중인 쓰레드 개수
	atomic<Node*> _pendingList; // 삭제 되어야 할 노드들 (첫번째 노드)
};