#pragma once
#include <mutex>

template<typename T>
class LockStack
{
public:
	LockStack() {};
	//����,���ԺҰ���
	LockStack(const LockStack&) = delete;
	LockStack& operator=(const LockStack&) = delete;

	void Push(T value)
	{
		lock_guard<mutex> lock(_mutex);
		_stack.push(std::move(value));
		_condVar.notify_one(); //����ϰ� �ִ� �ָ� ������
	}

	// ��Ƽ������ ȯ�濡���� Empty()�� ū �ǹ̰� ����
	// Empty -> top -> pop �� ����
	// 100���� Ȯ���� �����͸� �̾ƿ��°��� �ƴϹǷ� bool�� ����
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
		// �����Ͱ� ���� ������ ���, ������ �������� ������ (������ empty��) ���� Ǯ���ְ� sleep, �ٽ� �ñ׳��� ���� �Ͼ�� ���� ��� ����
		// ���������� ����ؾ��Ѵٸ� condition_variable�� ����Ͽ� �� �����ϸ� �����
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

	// 1. �� ��带 �����
	// 2. �� ����� next = head
	// 3. head = �� ���
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
		�� ������ �� ���� ���������� �Ͼ
		*/

		// �� ���̿� �����带 ����ٸ�?
		// ���� �ִ� ���� �ƴ� �ٸ� ���� �� ���� ����!
		// ��� ����ٸ�? -> LiveLock !
		// lockfree���� �����°� ������ ���� ���� ����
		while (_head.compare_exchange_weak(node->next, node) == false)
		{
			// false��� node->next = _head;
		}
		//_head = node;
	}

	// 1. head �б�
	// 2. head->next �б�
	// 3. head = head->next
	// 4. data �����ؼ� ��ȯ
	// 5. ������ ��带 ����
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
			

		//Exception �Ű�X , ������ ������ ���ΰ� ������ �ذ��ϴ°��� ����. ,���ڷθ� ����� ���� �ƴ�
		value = oldHead->data;
		TryDelete(oldHead);
		// ��Ƽ������ ȯ�濡�� ���ÿ� TryPop()�� �� ��� �̹� �ش� �����͸� ������ �������� ���� ����� ���� �ֱ� ������
		// �� �޸𸮸� ���� �� ���� , �˰����� �ʿ���. ��� ���� ����
		// �������� ������ ���� ���� ������ ��ٷȴٰ� ������ ����, ����Ʈ ������ó�� PopCount�� ����
		//delete oldHead;

		// C#, java ���� GC�� �־��ٸ� ���⼭ ������ �ڵ���
		return true;
	} 

	// 1) �����͸� �и�
	// 2) Count üũ
	// 3) �� ȥ�ڸ� ����
	void TryDelete(Node* oldHead)
	{
		// �� �ܿ� ���� �ִ°�?
		if (_popCount == 1)
		{
			// �� ȥ�ڳ�?

			// �̿� ȥ���ΰ�, ���� ����� �ٸ� �����͵鵵 �����غ���
			Node* node = _pendingList.exchange(nullptr);

			if (--_popCount == 0)
			{
				// ����� �ְ� ���� -> ���� ����
				// �����ͼ� ����� ������ �����ʹ� �и��ص� ���� ��
				DeleteNodes(node);
			}
			else if (node) // null üũ
			{
				// ���� ���������� �ٽ� ���� ����
				ChainPendingNodeList(node);
			}

			// �� �����ʹ� ����
			delete oldHead;
		}
		else
		{
			// ���� �ֳ� ? �׷� ���� �������� �ʰ�, ���� ���ุ
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
		// ������ ��带 ã�� �� ChainPendingNodeList ȣ��
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
	//������ top�� ����Ŵ
	atomic<Node*> _head;

	atomic<uint32> _popCount = 0; // Pop�� �������� ������ ����
	atomic<Node*> _pendingList; // ���� �Ǿ�� �� ���� (ù��° ���)
};