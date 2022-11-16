/*
 * SkipList set (C++)
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#define SK_MAX_LEVEL   23

template <typename K, typename V, typename TM, template <typename> class TMTYPE>
class TMSkipListMap : public TM::tmbase {

    struct SNode : public TM::tmbase {
        //static const int MAX_LEVEL = 23;
        /*-- Fields --*/
        TMTYPE<K>      key;
        TMTYPE<SNode*> forw[SK_MAX_LEVEL+1];
        TMTYPE<V>      value;

        SNode(int level, K k, V v) {
            for(int i=0;i<level + 1;i++) forw[i] = nullptr;
            key = k;
            value = v;
        }
    };


	/*---- Fields ----*/

    TMTYPE<SNode*>  header;
    TMTYPE<int64_t> level;



	/*---- Constructors ----*/

	// The degree is the minimum number of children each non-root internal node must have.
	public: TMSkipListMap() {
		header = TM::template tmNew<SNode>(SK_MAX_LEVEL, 0, nullptr);
		level = 0;
	}

    ~TMSkipListMap() {
        TM::template tmDelete<SNode>(header);
    }


	/*---- Methods ----*/

    /* Random Value Generator */
    float frand() {
        return (float) rand() / RAND_MAX;
    }

    /* Random Level Generator */
    int random_level() {
        static bool first = true;
        if (first){
            srand((unsigned)time(nullptr));
            first = false;
        }

        int lvl = (int)(log(frand()) / log(1.-0.5f));
        return lvl < SK_MAX_LEVEL ? lvl : SK_MAX_LEVEL;
    }

    public: void display(){
        const SNode* x = header->forw[0];
        while (x != nullptr){
            std::cout << x->key;
            x = x->forw[0];
            if (x != nullptr)
                std::cout << " - ";
        }
        std::cout << std::endl;
    }

	public: bool contains(K key, const int tid=0) const {
	    return TM::template readTx<bool>([this,key] () {
            SNode* x = header;
            for (int i = level;i >= 0;i--){
                while (x->forw[i] != nullptr && x->forw[i].pload()->key < key){
                    x = x->forw[i];
                }
            }
            x = x->forw[0];
            return x != nullptr && x->key == key;
	    });
	}

	public: V get(K key) const {
	    return TM::template readTx<V>([this,key] () {
            SNode* x = header;
            for (int i = level;i >= 0;i--){
                while (x->forw[i] != nullptr && x->forw[i].pload()->key < key){
                    x = x->forw[i];
                }
            }
            x = x->forw[0];
            if (x == nullptr || x->key != key) return (V)nullptr;
            return x->value.pload();
	    });
	}


	public: bool add(K key, V& value, const int tid=0) {
        return TM::template updateTx<bool>([this,key,value] () {
            SNode* x = header;
            SNode* update[SK_MAX_LEVEL + 1];
            for(int i=0;i<SK_MAX_LEVEL + 1;i++){
                update[i] = nullptr;
            }

            for (int i = level;i >= 0;i--){
                while (x->forw[i] != nullptr && x->forw[i]->key < key){
                    x = x->forw[i];
                }
                update[i] = x;
            }

            x = x->forw[0];

            if (x == nullptr || x->key != key){
                int lvl = random_level();
                if (lvl > level) {
                    for (int i = level + 1;i <= lvl;i++){
                        update[i] = header;
                    }
                    level = lvl;
                }
                x = TM::template tmNew<SNode>(lvl, key, value);
                for (int i = 0;i <= lvl;i++){
                    x->forw[i] = update[i]->forw[i];
                    update[i]->forw[i] = x;
                }
                return true;
            }
            return false;
        });
	}


	public: bool remove(K key, const int tid=0) {
        return TM::template updateTx<bool>([this,key] () {
            SNode* x = header;
            SNode* update[SK_MAX_LEVEL + 1];
            for(int i=0;i<SK_MAX_LEVEL + 1;i++){
                update[i] = nullptr;
            }

            for (int i = level;i >= 0;i--){
                while (x->forw[i] != nullptr && x->forw[i].pload()->key < key){
                    x = x->forw[i];
                }
                update[i] = x;
            }

            x = x->forw[0];

            if (x != nullptr && x->key == key){
                for (int i = 0;i <= level;i++){
                    if (update[i]->forw[i] != x) break;
                    update[i]->forw[i] = x->forw[i];
                }
                TM::template tmDelete<SNode>(x);
                while (level.pload() > 0 && header->forw[level] == nullptr){
                    level--;
                }
                return true;
            }
            return false;
        });
	}

public:

    void addAll(K* keys, V* values, uint64_t size, const int tid=0) {
        for (uint64_t i = 0; i < size; i++) add(keys[i], values[i], tid);
    }

    // Range query in the interval [lo;hi]
    int rangeQuery(const K &lo, const K &hi, K *const resultKeys) {
        printf("rangeQuery() not yet working properly. Needs to change the API to pass the max number of keys so we can have separate resultKeys\n");
        int numKeys = TM::template readTx<int>([this,lo,hi,resultKeys] () {
            int numKeys = 0;
            // Find the first key equal or lower than 'lo'
            SNode* n = header;
            for (int i = level;i >= 0;i--) {
                SNode* forwi = n->forw[i];
                while (forwi != nullptr && forwi->key < lo){
                    n = forwi;
                    forwi = n->forw[i];
                }
            }
            n = n->forw[0];
            //printf("lo is %ld, found %ld, hi is %ld\n", lo, n->key.pload(), hi);
            // Traverse until we reach a key higher than 'hi'
            while (n != nullptr) {
                K key = n->key;
                if (key >= hi) break;
                resultKeys[numKeys] = key;
                numKeys++;
                //assert(numKeys != 1001);
                n = n->forw[0];
            }
            return numKeys;
        });
        //printf("numKeys=%d\n", numKeys);
        return numKeys;
    }

    static std::string className() { return TM::className() + "-SkipListMap"; }

};
