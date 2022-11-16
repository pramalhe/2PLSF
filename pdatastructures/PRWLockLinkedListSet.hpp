#pragma once
#include <string>
#include <shared_mutex>


/**
 * <h1> A Linked List Set meant to be used with PTMs </h1>
 */
template<typename K>
class PRWLockLinkedListSet {

private:
    struct Node {
        K     key;
        Node* next {nullptr};
        Node(const K& key) : key{key} { }
        Node(){ }
    } __attribute__((aligned(128)));

    alignas(128) Node*  head {nullptr};
    alignas(128) Node*  tail {nullptr};
    alignas(128) std::shared_mutex rwlock {};


public:
    PRWLockLinkedListSet() {
        //rwlock.lock();
        Node* lhead = new Node();
        Node* ltail = new Node();
        head = lhead;
        head->next = ltail;
        tail = ltail;
        //rwlock.unlock();
    }

    ~PRWLockLinkedListSet() {
        //rwlock.lock();
        // Delete all the nodes in the list
        Node* prev = head;
        Node* node = prev->next;
        while (node != tail) {
            delete prev;
            prev = node;
            node = node->next;
        }
        delete prev;
        delete tail;
        //rwlock.unlock();
    }

    static std::string className() { return "PRWLock-LinkedListSet"; }

    /*
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(K key) {
        bool retval = false;
        rwlock.lock();
        Node *prev, *node;
        find(key, prev, node);
        retval = !(node != tail && key == node->key);
        if (!retval) {
            rwlock.unlock();
            return retval;
        }
        Node* newNode = new Node(key);
        prev->next = newNode;
        newNode->next = node;
        rwlock.unlock();
        return retval;
    }

    /*
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(K key) {
        bool retval = false;
        rwlock.lock();
        Node *prev, *node;
        find(key, prev, node);
        retval = (node != tail && key == node->key);
        if (!retval) {
            rwlock.unlock();
            return retval;
        }
        prev->next = node->next;
        delete node;
        rwlock.unlock();
        return retval;
    }

    /*
     * Returns true if it finds a node with a matching key
     */
    bool contains(K key) {
        bool retval = false;
        rwlock.lock_shared();
        Node *prev, *node;
        find(key, prev, node);
        retval = (node != tail && key == node->key);
        rwlock.unlock_shared();
        return retval;
    }

    // TODO: check if this is finding the key before or the key after when there is no exact match
    int rangeQuery(const K &lo, const K &hi, K *const resultKeys) {
        printf("TODO: implement rangeQuery in PRWLockLinkedListSet\n");
        return 0;
    }

    void find(const K& lkey, Node*& prev, Node*& node) {
        Node* ltail = tail;
        for (prev = head; (node = prev->next) != ltail; prev = node) {
            if ( !(node->key < lkey) ) break;
        }
    }

    // Used only for benchmarks
    bool addAll(K** keys, const int size) {
        for (int i = 0; i < size; i++) add(*keys[i]);
        return true;
    }
};

