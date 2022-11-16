#ifndef _TM_LINKED_LIST_QUEUE_BY_REF_H_
#define _TM_LINKED_LIST_QUEUE_BY_REF_H_

#include <string>


/**
 * <h1> A Linked List queue (memory unbounded) for usage with STMs and PTMs </h1>
 *
 */
template<typename T, typename TM, template <typename> class TMTYPE>
class TMLinkedListQueueByRef : public TM::tmbase {

private:
    struct Node : public TM::tmbase {
        TMTYPE<T*>    item;
        TMTYPE<Node*> next {nullptr};
        Node(T* userItem) : item{userItem} { }
    };

    alignas(128) TMTYPE<Node*>  head {nullptr};
    alignas(128) TMTYPE<Node*>  tail {nullptr};


public:
    TMLinkedListQueueByRef() {
        TM::template updateTx([&] () {
		    Node* sentinelNode = TM::template tmNew<Node>(nullptr);
		    head = sentinelNode;
		    tail = sentinelNode;
        });
    }


    ~TMLinkedListQueueByRef() {
        TM::template updateTx([&] () {
            while (dequeue() != nullptr); // Drain the queue
            Node* lhead = head;
            TM::tmDelete(lhead);
        });
    }


    static std::string className() { return TM::className() + "-LinkedListQueue"; }


    bool enqueue(T* item) {
        bool retval = false;
        TM::template updateTx([&] () {
            Node* newNode = TM::template tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
            retval = true;
        });
        return retval;
    }


    T* dequeue() {
        T* retval = nullptr;
        TM::template updateTx([&] () {
            Node* lhead = head;
            if (lhead == tail) return;
            head = lhead->next;
            TM::tmDelete(lhead);
            retval = head->item;
        });
        return retval;
    }
};

#endif /* _TM_LINKED_LIST_QUEUE_BY_REF_H_ */
