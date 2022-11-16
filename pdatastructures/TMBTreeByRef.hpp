/*
 * B-tree set (C++)
 *
 * Copyright (c) 2018 Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/btree-set
 *
 * Modified by Andreia Correia
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>


template <typename E, typename TM, template <typename> class TMTYPE>
class TMBTreeByRef : public TM::tmbase {

    private: static const int MAXKEYS { 16 };

	private: class Node;  // Forward declaration

	/*---- Fields ----*/

	private: TMTYPE<Node*>   root {nullptr};
	private: TMTYPE<int32_t> minKeys;  // At least 1, equal to degree-1
	private: TMTYPE<int32_t> maxKeys;  // At least 3, odd number, equal to minKeys*2+1



	/*---- Constructors ----*/

	// The degree is the minimum number of children each non-root internal node must have.
	public: explicit TMBTreeByRef(std::int32_t degree=8) :
			minKeys(degree - 1),
			maxKeys(degree <= UINT32_MAX / 2 ? degree * 2 - 1 : 0) {  // Avoid overflow
		if (degree < 2)
			throw std::domain_error("Degree must be at least 2");
		if (degree > UINT32_MAX / 2)  // In other words, need maxChildren <= UINT32_MAX
			throw std::domain_error("Degree too large");
		clear();
	}

    ~TMBTreeByRef() {
        deleteAll(root);
    }


	/*---- Methods ----*/

	public: void deleteAll(Node* node){
		if(!node->isLeaf()){
            for(int i=0;i<node->length.pload();i++){
                deleteAll(node->children[i]);
            }
		}
		TM::template tmDelete<Node>(node);
	}


	public: void clear() {
		if(root!=nullptr){
			deleteAll(root);
		}
		root = TM::template tmNew<Node>(maxKeys, true);
	}


	using SearchResult = std::pair<bool,std::int32_t>;

	public: bool seqContains(const E &val) const {
		// Walk down the tree
		Node* node = root.pload();
		while (true) {
			SearchResult sr = node->search(val);
			if (sr.first)
				return true;
			else if (node->isLeaf())
				return false;
			else  // Internal node
				node = node->children[sr.second];
		}
	}


	public: bool insert(const E& val) {
		// Special preprocessing to split root node
		if (root.pload()->length.pload() == maxKeys) {
			Node* child = root;
			root = TM::template tmNew<Node>(maxKeys, false);  // Increment tree height
			root.pload()->children[0] = child;
			root.pload()->splitChild(minKeys, maxKeys, 0);
		}

		// Walk down the tree
		Node* node = root;
		//std::cout<<root<<" root insert\n";
		//std::cout<<val<<" Key insert\n";
		while (true) {
			// Search for index in current node
			assert(node->length < maxKeys);
			assert(node == root || node->length >= minKeys);

			SearchResult sr = node->search(val);
			if (sr.first)
				return false;  // Key already exists in tree
			std::int32_t index = sr.second;

			//std::cout<<node<<" insert\n";
			//std::cout<<node->length<<" insert length\n";
			//std::cout<<index<<" insert index\n";
			if (node->isLeaf()) {  // Simple insertion into leaf
				for(int i=node->length;i>=index+1;i--){
					//std::cout<<i<<" i\n";
					//std::cout<<(node->length-1)<<" node->length-1\n";
					//std::cout<<(index+1)<<" index+1\n";
					node->keys[i]=node->keys[i-1];
				}
				node->keys[index] = val;
				node->length = node->length+1;
				return true;  // Successfully inserted

			} else {  // Handle internal node
				Node* child = node->children[index];
				if (child->length == maxKeys) {  // Split child node
					node->splitChild(minKeys, maxKeys, index);
					E middleKey = node->keys[index];
					if (val == middleKey)
						return false;  // Key already exists in tree
					else if (val > middleKey)
						child = node->children[index + 1];
				}
				node = child;
			}
		}
	}


	// returns 1 when successul removal
	public: std::size_t erase(const E& val) {
		// Walk down the tree
		bool found;
		std::int32_t index;
		{
			SearchResult sr = root.pload()->search(val);
			found = sr.first;
			index = sr.second;
		}
		Node* node = root;
		//std::cout<<val<<" key erase\n";
		while (true) {
			assert(node->length <= maxKeys);
			assert(node == root || node->length > minKeys);
			//std::cout<<node<<" erase\n";
			if (node->isLeaf()) {
				if (found) {  // Simple removal from leaf
					node->removeKey(index);
					return 1;
				} else
					return 0;

			} else {  // Internal node
				if (found) {  // Key is stored at current node
					Node* left  = node->children[index + 0];
					Node* right = node->children[index + 1];
					assert(left != nullptr && right != nullptr);
					if (left->length > minKeys) {  // Replace key with predecessor
						node->keys[index] = left->removeMax(minKeys);
						return 1;
					} else if (right->length > minKeys) {  // Replace key with successor
						node->keys[index] = right->removeMin(minKeys);
						return 1;
					} else {  // Merge key and right node into left node, then recurse
						node->mergeChildren(minKeys, index);
						if (node == root && root.pload()->length==0) {
							assert(root.pload()->length+1 == 1);
							Node* next = root.pload()->children[0];
							TM::template tmDelete<Node>(root);
							root = next; //TODO:delete root
							//root->children[0] = nullptr;
						}
						node = left;
						index = minKeys;  // Index known due to merging; no need to search
					}

				} else {  // Key might be found in some child
					Node* child = node->ensureChildRemove(minKeys, index);
					if (node == root && root.pload()->length==0) {
						assert(root.pload()->length +1 == 1);
						Node* next = root.pload()->children[0];
						TM::template tmDelete<Node>(root);
						root = next; //TODO:delete root
						//root->children[0] = nullptr;
					}
					node = child;
					SearchResult sr = node->search(val);
					found = sr.first;
					index = sr.second;
				}
			}
		}
	}

	// For debugging
	public: void printStructure(Node* node) const {
	    if (node == nullptr) return;
	    printf("%p keys = [ ", node);
	    for (int i = 0; i < node->length; i++) printf("%d ", node->keys[i]);
	    printf("]\n");
	    if (node->isLeaf()) return;
	    for (int i = 0; i < node->length+1; i++) printStructure(node->children[i]);
	}


	/*---- Helper class: B-tree node ----*/

	private: class Node : public TM::tmbase {

		/*-- Fields --*/

		public: TMTYPE<int32_t> length {0};
        // Size is in the range [0, maxKeys] for root node, [minKeys, maxKeys] for all other nodes.
        public: TMTYPE<E>       keys[MAXKEYS];
		// If leaf then size is 0, otherwise if internal node then size always equals keys.size()+1.
		public: TMTYPE<Node*>   children[MAXKEYS+1];


		/*-- Constructor --*/

		// Note: Once created, a node's structure never changes between a leaf and internal node.
		public: Node(std::uint32_t maxKeys, bool leaf) {
			assert(maxKeys >= 3 && maxKeys % 2 == 1);
			assert(maxKeys <= MAXKEYS);
			for (int i=0; i < maxKeys+1; i++) children[i] = nullptr;
		}

		/*-- Methods for getting info --*/

		public: bool getLength() const {
			return length;
		}

		public: bool isLeaf() const {
			return (children[0]==nullptr);
		}

		// Searches this node's keys vector and returns (true, i) if obj equals keys[i],
		// otherwise returns (false, i) if children[i] should be explored. For simplicity,
		// the implementation uses linear search. It's possible to replace it with binary search for speed.
		public: SearchResult search(const E &val) const {
			std::int32_t i = 0;
			while (i < length) {
				const E& elem = keys[i];
				if (val == elem) {
					assert(i < length);
					return SearchResult(true, i);  // Key found
				} else if (val > elem)
					i++;
				else  // val < elem
					break;
			}
			assert(i <= length);
			return SearchResult(false, i);  // Not found, caller should recurse on child
		}


		/*-- Methods for insertion --*/

		// For the child node at the given index, this moves the right half of keys and children to a new node,
		// and adds the middle key and new child to this node. The left half of child's data is not moved.
		public: void splitChild(std::size_t minKeys, std::int32_t maxKeys, std::size_t index) {
			assert(!this->isLeaf() && index <= this->length && this->length < maxKeys);
			Node* left = this->children[index];
			Node* right = TM::template tmNew<Node>(maxKeys,left->isLeaf());

			// Handle keys
			int j=0;
			for(int i=minKeys + 1;i<left->length;i++){
				right->keys[j] = left->keys[i];
				j++;
			}

			//add right node to this
			for(int i=length+1; i>=index+2;i--){
				this->children[i]= this->children[i-1];
			}
			this->children[index+1] = right;
			for(int i=length; i>=index+1;i--){
				this->keys[i]= this->keys[i-1];
			}
			this->keys[index] = left->keys[minKeys];
			this->length = this->length+1;

			if(!left->isLeaf()){
				j=0;
				for(int i= minKeys + 1;i<left->length+1;i++){
					right->children[j] = left->children[i];
					j++;
				}
			}

			right->length = left->length-minKeys-1;
			left->length = minKeys;
		}


		/*-- Methods for removal --*/

		// Performs modifications to ensure that this node's child at the given index has at least
		// minKeys+1 keys in preparation for a single removal. The child may gain a key and subchild
		// from its sibling, or it may be merged with a sibling, or nothing needs to be done.
		// A reference to the appropriate child is returned, which is helpful if the old child no longer exists.
		public: Node* ensureChildRemove(std::int32_t minKeys, std::uint32_t index) {
			// Preliminaries
			assert(!this->isLeaf() && index < this->length+1);
			Node* child = this->children[index];
			if (child->length > minKeys)  // Already satisfies the condition
				return child;
			assert(child->length == minKeys);

			// Get siblings
			Node* left = index >= 1 ? this->children[index - 1].pload() : nullptr;
			Node* right = index < this->length ? this->children[index + 1].pload() : nullptr;
			bool internal = !child->isLeaf();
			assert(left != nullptr || right != nullptr);  // At least one sibling exists because degree >= 2
			assert(left  == nullptr || left ->isLeaf() != internal);  // Sibling must be same type (internal/leaf) as child
			assert(right == nullptr || right->isLeaf() != internal);  // Sibling must be same type (internal/leaf) as child

			if (left != nullptr && left->length > minKeys) {  // Steal rightmost item from left sibling
				//std::cout<<"Passou left\n";
				if (internal) {
					for(int i=child->length+1;i>=1;i--){
						child->children[i] = child->children[i-1];
					}
					child->children[0] = left->children[left->length];
				}
				for(int i=child->length;i>=1;i--){
					child->keys[i] = child->keys[i-1];
				}
				child->keys[0] = this->keys[index - 1];
				this->keys[index-1] = left->keys[left->length-1];
				left->length = left->length-1;
				child->length = child->length+1;
				return child;
			} else if (right != nullptr && right->length > minKeys) {  // Steal leftmost item from right sibling
				//std::cout<<"Passou right\n";
				if (internal) {
					child->children[child->length+1] = right->children[0];
					for(int i=0;i<right->length;i++){
						right->children[i] = right->children[i+1];
					}
				}
				child->keys[child->length] = this->keys[index];
				this->keys[index] = right->removeKey(0);
				child->length = child->length+1;
				return child;
			} else if (left != nullptr) {  // Merge child into left sibling
				this->mergeChildren(minKeys, index - 1);
				return left;  // This is the only case where the return value is different
			} else if (right != nullptr) {  // Merge right sibling into child
				this->mergeChildren(minKeys, index);
				return child;
			} else
				throw std::logic_error("Impossible condition");
		}


		// Merges the child node at index+1 into the child node at index,
		// assuming the current node is not empty and both children have minKeys.
		public: void mergeChildren(std::int32_t minKeys, std::uint32_t index) {
			assert(!this->isLeaf() && index < this->length);
			Node* left  = this->children[index];
			Node* right = this->children[index+1];
			assert(left->length == minKeys && right->length == minKeys);
			//std::cout<<"Passou\n";
			if (!left->isLeaf()){
				for(int i=0;i<right->length+1;i++){
					left->children[left->length+1+i] = right->children[i];
				}
			}
			left->keys[left->length] = this->keys[index];
			for(int i=0;i<right->length;i++){
				left->keys[left->length+1+i] = right->keys[i];
			}
			left->length = left->length + right->length+1;
			// remove key(index)
			for(int i=index;i<length-1;i++){
				this->keys[i]=this->keys[i+1];
			}
			// remove children (index+1)
			TM::template tmDelete<Node>(children[index+1]);
			for(int i=index+1;i<length;i++){
				this->children[i]=this->children[i+1];
			}
			this->children[length]=nullptr;
			length = length-1;
		}


		// Removes and returns the minimum key among the whole subtree rooted at this node.
		// Requires this node to be preprocessed to have at least minKeys+1 keys.
		public: E removeMin(std::int32_t minKeys) {
			for (Node* node = this; ; ) {
				assert(node->length > minKeys);
				if (node->isLeaf()){
					E ret = node->keys[0];
					for(int i=0;i<node->length-1;i++){
						node->keys[i]=node->keys[i+1];
					}
					node->length = node->length-1;
					return ret;
				}else{
					node = node->ensureChildRemove(minKeys, 0);
				}

			}
		}


		// Removes and returns the maximum key among the whole subtree rooted at this node.
		// Requires this node to be preprocessed to have at least minKeys+1 keys.
		public: E removeMax(std::int32_t minKeys) {
			for (Node *node = this; ; ) {
				assert(node->length > minKeys);
				if (node->isLeaf()){
					node->length = node->length-1;
					return node->keys[node->length];
				}else{
					node = node->ensureChildRemove(minKeys, node->length);
				}
			}
		}


		// Removes and returns this node's key at the given index.
		public: E removeKey(std::uint32_t index) {
			E ret = this->keys[index];
			for(int i=index;i < this->length-1;i++){
				this->keys[i] = this->keys[i+1];
			}
			length = length-1;
			return ret;
		}
	};

public:
    // Inserts a key only if it's not already present
    bool add(E key, const int tid=0) {
        bool retval = false;
        TM::template updateTx([&] () {
            retval = insert(key);
        });
        return retval;
    }

    // Returns true only if the key was present
    bool remove(E key, const int tid=0) {
        bool retval = false;
        TM::template updateTx([&] () {
            retval = (erase(key) == 1);
        });
        return retval;
    }

    bool contains(E key, const int tid=0) {
        bool retval = false;
        TM::template readTx([&] () {
            retval = seqContains(key);
        });
        return retval;
    }

    void addAll(E** keys, uint64_t size, const int tid=0) {
        for (uint64_t i = 0; i < size; i++) add(*keys[i], tid);
    }

    uint64_t traversal(E key, uint64_t numKeys) {
        printf("traversal() not implemented\n");
        return 0;
    }

    int rangeQuery(const E &lo, const E &hi, E *const resultKeys) {
        printf("rangeQuery() not implemented\n");
        return 0;
    }

    static std::string className() { return TM::className() + "-BTree"; }

};
