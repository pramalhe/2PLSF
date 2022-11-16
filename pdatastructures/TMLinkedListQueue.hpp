#ifndef _TM_LINKED_LIST_QUEUE_H_
#define _TM_LINKED_LIST_QUEUE_H_

#include <string>


/**
 * <h1> A Linked List queue (memory unbounded) for usage with STMs and PTMs </h1>
 *
 */
template<typename T, typename TM, template <typename> class TMTYPE>
class TMLinkedListQueue : public TM::tmbase {

private:
    struct Node : public TM::tmbase {
        TMTYPE<T*>    item;
        TMTYPE<Node*> next {nullptr};
        Node(T* userItem) : item{userItem} { }
    };

    alignas(128) TMTYPE<Node*>  head {nullptr};
    alignas(128) TMTYPE<Node*>  tail {nullptr};


public:
    TMLinkedListQueue() {
        TM::template updateTx<bool>([=] () {
		    Node* sentinelNode = TM::template tmNew<Node>(nullptr);
		    head = sentinelNode;
		    tail = sentinelNode;
		    return true;
        });
    }


    ~TMLinkedListQueue() {
        TM::template updateTx<bool>([=] () {
		    while (dequeue() != nullptr); // Drain the queue
		    Node* lhead = head;
		    TM::tmDelete(lhead);
		    return true;
        });
    }


    static std::string className() { return TM::className() + "-LinkedListQueue"; }


    bool enqueue(T* item) {
        return TM::template updateTx<bool>([=] () {
            Node* newNode = TM::template tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    T* dequeue() {
        return TM::template updateTx<T*>([=] () -> T* {
            Node* lhead = head;
            if (lhead == tail) return nullptr;
            head = lhead->next;
            TM::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _TM_LINKED_LIST_QUEUE_H_ */
