#pragma once
#include <string>


/**
 * <h1> A Linked List Set meant to be used with PTMs </h1>
 * This has non-dependent loads on the lookup
 */
template<typename K, typename TM, template <typename> class TMTYPE>
class TMLinkedListSetNDP : public TM::tmbase {

private:
    struct Node : public TM::tmbase {
        K     key;
        TMTYPE<Node*> next {nullptr};
        Node(const K& key) : key{key} { }
        Node(){ }
    } __attribute__((aligned(128)));

    alignas(128) TMTYPE<Node*>  head {nullptr};
    alignas(128) TMTYPE<Node*>  tail {nullptr};


public:
    TMLinkedListSetNDP() {
        TM::template updateTx<bool>([=] () {
            Node* lhead = TM::template tmNew<Node>();
            Node* ltail = TM::template tmNew<Node>();
            head = lhead;
            head->next = ltail;
            tail = ltail;
            return true;
        });
    }

    ~TMLinkedListSetNDP() {
        TM::template updateTx<bool>([=] () {
            // Delete all the nodes in the list
            Node* prev = head;
            Node* node = prev->next;
            while (node != tail) {
                TM::tmDelete(prev);
                prev = node;
                node = node->next;
            }
            TM::tmDelete(prev);
            TM::tmDelete(tail.pload());
            return true;
        });
    }

    static std::string className() { return TM::className() + "-LinkedListSetNDP"; }

    /*
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(K key) {
        return TM::template updateTx<bool>([=] () {
            K lkey = key;
            Node *prev, *node;
            find(lkey, prev, node);
            if (node != tail && lkey == node->key) return false;
            Node* newNode = TM::template tmNew<Node>(lkey);
            prev->next = newNode;
            newNode->next = node;
            return true;
        });
    }

    /*
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(K key) {
        return TM::template updateTx<bool>([=] () {
            K lkey = key;
            Node *prev, *node;
            find(lkey, prev, node);
            if (!(node != tail && lkey == node->key)) return false;
            prev->next = node->next;
            TM::tmDelete(node);
            return true;
        });
    }

    /*
     * Returns true if it finds a node with a matching key
     */
    bool contains(K key) {
        return TM::template readTx<bool>([=] () {
            K lkey = key;
            Node *prev, *node;
            find(lkey, prev, node);
            return (node != tail && lkey == node->key);
        });
    }

    void find(const K& lkey, Node*& prev, Node*& node) {
        Node* ltail = tail;
        Node* pprev = head;
        for (prev = head; (node = prev->next.ndpload()) != ltail; prev = node) {
            if ( !(node->key < lkey) ) break;
            pprev = prev;
        }
        pprev->next.pload();
        prev->next.pload();
        node->next.pload();
    }

    // Used only for benchmarks
    bool addAll(K** keys, const int size) {
        for (int i = 0; i < size; i++) add(*keys[i]);
        return true;
    }
};

