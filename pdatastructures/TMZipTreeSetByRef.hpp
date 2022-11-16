/*
 * Copyright 2017-2019
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
 * <h1> Zip Tree Set meant to be used with PTMs </h1>
 *
 * Paper: https://arxiv.org/pdf/1806.06726.pdf
 * Based on the Java implementation at:
 * https://github.com/haietza/zip-trees/blob/master/ZipTree.java
 *
 * TODO: change the operators for comparison of the keys so that it uses only < and ==
 * TODO: get rid of the lookup and make insert/delete return a boolean that indicates if the key was present or not
 */
template<typename K, typename TM, template <typename> class TMTYPE>
class TMZipTreeSetByRef : public TM::tmbase {

private:
    struct Node : public TM::tmbase {
        TMTYPE<K>        key;
        TMTYPE<int64_t>  rank;
        TMTYPE<Node*>    left {nullptr};
        TMTYPE<Node*>    right {nullptr};
        Node(const K& key) : key{key} {
            rank = randomRank();
        }
        // Gives a random rank using a geometric distribution?
        static int64_t randomRank() {
            static thread_local uint64_t tl_rand_seed {0};
            // A good random seed that is different for each thread is the address of the thread-local variable
            if (tl_rand_seed == 0) tl_rand_seed = (uint64_t)&tl_rand_seed;
            uint64_t r = random64(tl_rand_seed);
            // Probablility of each rank is 1024/2048 = 0.5
            // To prevent runaways, stop if rank reaches 64 (2^64 nodes)
            int64_t heads = 0;
            for (; (r % 2048) < 1024 && heads < 64; heads++) r = random64(r);
            tl_rand_seed = r;
            return heads;
        }
        // An imprecise but fast random number generator
        static uint64_t random64(uint64_t x) {
            x ^= x >> 12; // a
            x ^= x << 25; // b
            x ^= x >> 27; // c
            return x * 2685821657736338717LL;
        }
    };

    alignas(128) TMTYPE<Node*> root {nullptr};

public:
    TMZipTreeSetByRef() { }

    ~TMZipTreeSetByRef() {
        TM::updateTx([&] () {
            clear();
        });
    }

    static std::string className() { return TM::className() + "-ZipTree"; }

    /*
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(K key, const int tid=0) {
        bool retval = false;
        TM::updateTx([&] () {
            if (recursiveFind(key, root.pload())) return;
            Node* node = TM::template tmNew<Node>(key);
            iterativeInsert(node);
            retval = true;
        });
        return retval;
    }

    /*
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(K key, const int tid=0) {
        bool retval = false;
        TM::updateTx([&] () {
            Node* delNode = recursiveFind(key, root.pload());
            retval = iterativeDelete(key);
            if (delNode != nullptr) TM::template tmDelete<Node>(delNode);
        });
        return retval;
    }

    /*
     * Returns true if it finds a node with a matching key
     */
    bool contains(K key, const int tid=0) {
        bool retval = false;
        TM::readTx([&] () {
            retval = (recursiveFind(key, root.pload()) != nullptr);
        });
        return retval;
    }

    // Used only for benchmarks
    bool addAll(K** keys, const int size, const int tid=0) {
        for (int i = 0; i < size; i++) add(*keys[i]);
        return true;
    }

    int rangeQuery(const K &lo, const K &hi, K *const resultKeys) {
        printf("rangeQuery() not implemented\n");
        return 0;
    }

private:
    // Used internally. Must be called from within a transaction
    void iterativeInsert(Node* x) {
        int64_t rank = x->rank;
        K key = x->key;
        Node* cur = root;
        Node* prev = nullptr;
        while (cur != nullptr && (rank < cur->rank || (rank == cur->rank && key > cur->key))) {
            prev = cur;
            if (key < cur->key) {
                cur = cur->left;
            } else {
                cur = cur->right;
            }
        }
        if (cur == root) {
            root = x;
        } else if (key < prev->key) {
            prev->left = x;
        } else {
            prev->right = x;
        }
        if (cur == nullptr) return;
        if (key < cur->key) {
            x->right = cur;
        } else {
            x->left = cur;
        }
        prev = x;
        while (cur != nullptr) {
            Node* fix = prev;
            if (cur->key < key) {
                while (cur != nullptr && cur->key <= key) {
                    prev = cur;
                    cur = cur->right;
                }
            } else {
                while (cur != nullptr && cur->key >= key) {
                    prev = cur;
                    cur = cur->left;
                }
            }
            if (fix->key > key || (fix == x && prev->key > key)) {
                fix->left = cur;
            } else {
                fix->right = cur;
            }
        }
    }

    // Used internally. Must be called from within a transaction
    bool iterativeDelete(K key) {
        Node* cur = root;
        Node* prev = nullptr;
        if (cur == nullptr) return false;
        while (key != cur->key) {
            prev = cur;
            if (key < cur->key) {
                cur = cur->left;
            } else {
                cur = cur->right;
            }
            if (cur == nullptr) return false;
        }
        Node* left = cur->left;
        Node* right = cur->right;
        if (left == nullptr) {
            cur = right;
        } else if (right == nullptr) {
            cur = left;
        } else if (left->rank >= right->rank) {
            cur = left;
        } else {
            cur = right;
        }
        if (root->key == key) {
            root = cur;
        } else if (key < prev->key) {
            prev->left = cur;
        } else {
            prev->right = cur;
        }
        while (left != nullptr && right != nullptr) {
            if (left->rank.pload() >= right->rank.pload()) {
                while (left != nullptr && left->rank.pload() >= right->rank.pload()) {
                    prev = left;
                    left = left->right;
                }
                prev->right = right;
            } else {
                while (right != nullptr && left->rank.pload() < right->rank.pload()) {
                    prev = right;
                    right = right->left;
                }
                prev->left = left;
            }
        }
        return true;
    }

    // Used internally. Must be called from within a transaction
    Node* recursiveFind(K key, Node* node) {
        if (node == nullptr) return nullptr;
        if (node->key == key) return node;
        if (key < node->key) {
            Node* nleft = node->left;
            if (nleft == nullptr) return nullptr;
            if (key == nleft->key) {
                return nleft;
            } else {
                return recursiveFind(key, nleft);
            }
        } else {
            Node* nright = node->right;
            if (nright == nullptr) return nullptr;
            if (key == nright->key) {
                return nright;
            } else {
                return recursiveFind(key, nright);
            }
        }
    }

    // Internal: Recursively clears the given subtree. Frees the given node.
    void clearNode(Node* n) {
        if (n == nullptr) return;
        clearNode(n->left);
        clearNode(n->right);
        TM::tmDelete(n);
    }

    // Clears the entire tree, starting from the root
    void clear() {
        clearNode(root);
        root = nullptr;
    }
};

