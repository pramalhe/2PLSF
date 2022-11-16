/*
 * Copyright 2017-2022
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once
#include <string>
#include <cassert>


/**
 * <h1> A Relaxed AVL MAP meant to be used with PTMs </h1>
 *
 * Originally taken from https://github.com/pmem/pmdk/blob/master/src/libpmemobj/ravl.c
 * This was adapted to C++ and added instrumentation.
 *
 * TODO:
 * - Change the node*() methods to be methods in the Node class
 * -
 */
template<typename K, typename V, typename TM, template <typename> class TMTYPE>
class TMRAVLMap : public TM::tmbase {

private:
    enum slot_type_e {
        RAVL_LEFT,
        RAVL_RIGHT,
        MAX_SLOTS,
        RAVL_ROOT
    };

    struct Node : public TM::tmbase {
        TMTYPE<Node*>    slots[MAX_SLOTS];
        TMTYPE<K>        key;
        TMTYPE<Node*>    parent;
        TMTYPE<int64_t>  rank;  /* cannot be greater than height of the subtree */
        TMTYPE<V>        value;
        Node(const K& key, const V& value) : key{key}, value{value} {
            parent = nullptr;
            slots[RAVL_LEFT] = nullptr;
            slots[RAVL_RIGHT] = nullptr;
            rank = 0;
        }
    };

    alignas(128) TMTYPE<Node*> root {nullptr};
    alignas(128) TMTYPE<V> NO_VALUE {};

public:
    TMRAVLMap() {
    }

    // Used by setbench
    TMRAVLMap(V novalue) {
        NO_VALUE = novalue;
    }

    ~TMRAVLMap() {
        TM::template updateTx<bool>([=] () {
            clear();
            return true; // Needed for CX
        });
    }

    static std::string className() { return TM::className() + "-RAVL-Map"; }

    /*
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(K key, V& value) {
        bool ret = TM::template updateTx<bool>([=] () {
            // Walk down the tree and insert the new node into a missing slot
            TMTYPE<Node*>* dstp = (TMTYPE<Node*>*)&root;
            Node *dst = nullptr;
            while (*dstp != nullptr) {
                dst = *dstp;
                K nkey = dst->key;
                if (key == nkey) return false;
                dstp = (TMTYPE<Node*>*)&dst->slots[(key < nkey) ? RAVL_LEFT : RAVL_RIGHT];
            }
            Node* n = TM::template tmNew<Node>(key, value);
            n->parent = dst;
            *dstp = n;
            balance(n);
            return true;
        });
        return ret;
    }

    // Used by setbench
    V insertIfAbsent(K key, V value) {
        return TM::template updateTx<V>([=] () {
            // Walk down the tree and insert the new node into a missing slot
            TMTYPE<Node*>* dstp = (TMTYPE<Node*>*)&root;
            Node *dst = nullptr;
            while (*dstp != nullptr) {
                dst = *dstp;
                K nkey = dst->key;
                if (key == nkey) return dst->value.pload();
                dstp = (TMTYPE<Node*>*)&dst->slots[(key < nkey) ? RAVL_LEFT : RAVL_RIGHT];
            }
            Node* n = TM::template tmNew<Node>(key, value);
            n->parent = dst;
            *dstp = n;
            balance(n);
            return value;
        });
    }

    /*
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(K key) {
        bool ret = TM::template updateTx<bool>([=] () {
            Node* n = root;
            while (n != nullptr) {
                K nkey = n->key;
                if (key == nkey) {
                    nodeRemove(n);
                    return true;
                }
                n = n->slots[(key < nkey) ? RAVL_LEFT : RAVL_RIGHT];
            }
            return false;
        });
        return ret;
    }

    // Used by setbench
    V erase(K key) {
        return TM::template updateTx<V>([=] () {
            Node* n = root;
            while (n != nullptr) {
                K nkey = n->key;
                if (key == nkey) {
                    nodeRemove(n);
                    return n->value.pload();
                }
                n = n->slots[(key < nkey) ? RAVL_LEFT : RAVL_RIGHT];
            }
            return NO_VALUE.pload();
        });
    }

    /*
     * Returns true if it finds a node with a matching key
     */
    inline bool contains(K key) {
        return TM::template readTx<bool>([=] () {
            Node* n = root;
            while (n != nullptr) {
                K nkey = n->key;
                if (key == nkey) return true;
                n = n->slots[(key < nkey) ? RAVL_LEFT : RAVL_RIGHT];
            }
            return false;
        });
    }

    inline V get(K key) {
        return TM::template readTx<V>([=] () {
            Node* n = root;
            while (n != nullptr) {
                K nkey = n->key;
                if (key == nkey) return n->value.pload();
                n = n->slots[(key < nkey) ? RAVL_LEFT : RAVL_RIGHT];
            }
            return (V)nullptr;
        });
    }

    // Used only for benchmarks
    bool addAll(K* keys, V* values, uint64_t size, const int tid=0) {
        for (uint64_t i = 0; i < size; i++) add(keys[i], values[i]);
        return true;
    }

    // TODO: check if this is finding the key before or the key after when there is no exact match
    int rangeQuery(const K &lo, const K &hi, K *const resultKeys) {
        return TM::template readTx<int>([=] () {
            int numKeys = 0;
            Node* n = root;
            // Find the first key equal or lower than 'lo'
            while (n != nullptr) {
                //printf("lo is %ld, descending in key %ld\n", lo, n->key.pload());
                if (lo == n->key) break;
                Node* next = n->slots[(lo < n->key) ? RAVL_LEFT : RAVL_RIGHT];
                if (next == nullptr) break;
                n = next;
            }
            //printf("lo is %ld, found %ld\n", lo, n->key.pload());
            // Traverse until we reach a key higher than 'hi'
            while (n != nullptr) {
                K key = n->key;
                if (key >= hi) break;
                resultKeys[numKeys] = key;
                numKeys++;
                //assert(numKeys != 1001);
                n = nodeSuccessor(n);
            }
            //printf("numKeys=%d\n", numKeys);
            return numKeys;
        });
    }


private:
    // Internal: Recursively clears the given subtree. Frees the given node.
    void clearNode(Node* n) {
        if (n == nullptr) return;
        clearNode(n->slots[RAVL_LEFT]);
        clearNode(n->slots[RAVL_RIGHT]);
        TM::tmDelete(n);
    }

    // Clears the entire tree, starting from the root
    void clear() {
        clearNode(root);
        root = nullptr;
    }

    // Internal: returns the opposite slot type, cannot be called for root type
    inline slot_type_e slotOpposite(slot_type_e t) {
        assert(t != RAVL_ROOT);
        return t == RAVL_LEFT ? RAVL_RIGHT : RAVL_LEFT;
    }

    // Internal: returns the type of the given node: left child, right child or root
    slot_type_e slotType(Node* n) {
        if (n->parent == nullptr) return RAVL_ROOT;
        return n->parent->slots[RAVL_LEFT] == n ? RAVL_LEFT : RAVL_RIGHT;
    }

    // Internal: returns the sibling of the given node, nullptr if the node is root (has no parent)
    Node* nodeSibling(Node* n) {
        slot_type_e t = slotType(n);
        if (t == RAVL_ROOT) return nullptr;
        return n->parent->slots[t == RAVL_LEFT ? RAVL_RIGHT : RAVL_LEFT];
    }

    // Internal: returns the pointer to the memory location in  which the given node resides
    TMTYPE<Node*>* nodeRef(Node* n) {
        slot_type_e t = slotType(n);
        return (TMTYPE<Node*>*)(t == RAVL_ROOT ? &root : &(n->parent->slots[t]));
    }

    // Internal: performs a rotation around a given node:
    // The node swaps place with its parent. If 'node' is right child, parent becomes
    // the left child of node, otherwise parent becomes right child of node.
    void rotate(Node* n) {
        assert(n->parent != nullptr);
        Node* p = n->parent;
        TMTYPE<Node*>* pref = nodeRef(p);
        slot_type_e t = slotType(n);
        slot_type_e t_opposite = slotOpposite(t);
        n->parent = p->parent;
        p->parent = n;
        *pref = n;
        if ((p->slots[t] = n->slots[t_opposite]) != nullptr) p->slots[t]->parent = p;
        n->slots[t_opposite] = p;
    }

    // (internal) returns the rank of the node:
    // For the purpose of balancing, nullptr nodes have rank -1.
    int64_t nodeRank(Node* n) {
        return n == nullptr ? -1 : n->rank.pload();
    }

    // Internal: returns the rank different between parent node p and its child n
    // Every rank difference must be positive. Either of these can be nullptr.
    int64_t nodeRankDifferenceParent(Node* p, Node* n) {
        return nodeRank(p) - nodeRank(n);
    }

    int64_t nodeRankDifference(Node* n) {
        return nodeRankDifferenceParent(n->parent, n);
    }

    // Internal: checks if a given node is strictly i,j-node
    bool nodeIs_i_j(Node* n, int i, int j) {
        return (nodeRankDifferenceParent(n, n->slots[RAVL_LEFT]) == i &&
                nodeRankDifferenceParent(n, n->slots[RAVL_RIGHT]) == j);
    }

    // Internal: checks if a given node is i,j-node or j,i-node
    bool nodeIs(Node* n, int i, int j) {
        return nodeIs_i_j(n, i, j) || nodeIs_i_j(n, j, i);
    }

    // Promotes a given node by increasing its rank
    void nodePromote(Node* n) {
        n->rank++;
    }

    // Demotes a given node by decreasing its rank
    void nodeDemote(Node* n) {
        assert( n->rank.pload() > 0 );
        n->rank--;
    }

    void balance(Node *n) {
        // Walk up the tree, promoting nodes
        while (n->parent.pload() != nullptr && nodeIs(n->parent, 0, 1)) {
            nodePromote(n->parent);
            n = n->parent;
        }
        // Either the rank rule holds or n is a 0-child whose sibling is an i-child with i > 1.
        Node* s = nodeSibling(n);
        if (!(nodeRankDifference(n) == 0 && nodeRankDifferenceParent(n->parent, s) > 1)) return;
        Node *y = n->parent;
        // if n is a left child, let z be n's right child and vice versa */
        slot_type_e t = slotOpposite(slotType(n));
        Node* z = n->slots[t];
        if (z == nullptr || nodeRankDifference(z) == 2) {
            rotate(n);
            nodeDemote(y);
        } else if (nodeRankDifference(z) == 1) {
            rotate(z);
            rotate(z);
            nodePromote(z);
            nodeDemote(n);
            nodeDemote(y);
        }
    }

    // Internal: returns left-most or right-most node in the subtree
    Node* nodeTypeMost(Node* n, slot_type_e t) {
        Node* nslott = n->slots[t];
        while (nslott != nullptr) {
            n = nslott;
            nslott = n->slots[t];
        }
        return n;
    }

    // Internal: returns the successor or predecessor of the node
    Node* nodeCessor(Node* n, slot_type_e t) {
        // If t child is present, we are looking for t-opposite-most node in t child subtree
        Node* nslott = n->slots[t];
        if (nslott) return nodeTypeMost(nslott, slotOpposite(t));
        // otherwise get the first parent on the t path
        Node* nparent;
        while (true) {
            nparent = n->parent;
            if (nparent == nullptr || n != nparent->slots[t]) break;
            n = nparent;
        }
        return nparent;
    }

    // Internal: returns node's successor. It's the first node larger than n.
    Node* nodeSuccessor(Node* n) {
        return nodeCessor(n, RAVL_RIGHT);
    }

    // Internal: returns node's predecessor. It's the first node smaller than n.
    Node* nodePredecessor(Node* n) {
        return nodeCessor(n, RAVL_LEFT);
    }

    void nodeRemove(Node* n) {
        if (n->slots[RAVL_LEFT] != nullptr && n->slots[RAVL_RIGHT] != nullptr) {
            // If both children are present, remove the successor instead
            Node* s = nodeSuccessor(n);
            n->key = s->key;  // TODO: this won't work for intrusive stuff
            nodeRemove(s);
        } else {
            // Swap n with the child that may exist
            Node* r = n->slots[RAVL_LEFT] != nullptr ? n->slots[RAVL_LEFT] : n->slots[RAVL_RIGHT];
            if (r != nullptr) r->parent = n->parent;
            *nodeRef(n) = r;
            TM::tmDelete(n);
        }
    }

    void nodePrint(Node* n, int level=0) {
        if (n != nullptr) {
            printf("%*s%ld\n",level, "", n->key.pload());
            nodePrint(n->slots[RAVL_LEFT], level+1);
            nodePrint(n->slots[RAVL_RIGHT], level+1);
        }
    }

};
