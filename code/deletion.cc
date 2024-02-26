#include <stdint.h>
#include <stack>
#include <queue>
#include <iostream>
#include <cassert>
#include <algorithm> 
#include <iterator> 
#include <set>
#include <fstream>
#include <algorithm>
#include <random>

using namespace std;

#define LEAF 0xAF
#define INTERNAL 0xA1
#define DUPLICATES 0xD0
#define N 24
typedef struct IndexBlock {
    uint8_t nodeType;
    uint8_t numKeys;
    uint32_t keys[N];
    IndexBlock** pointers;
} IndexBlock;

typedef struct DuplicatesBlock {
    uint8_t nodeType;
    uint8_t numKeys;
    uint32_t pointers[N*2];
    DuplicatesBlock* next;
} DuplicatesBlock;

uint32_t numLeaf = 1;
uint32_t numInternal;
uint32_t numDuplicates;
set<uint32_t> keySet;

void Insert(IndexBlock** root, uint32_t key) {
    if(keySet.count(key) == 0) keySet.insert(key);
    IndexBlock* block = *root;
    stack<IndexBlock*> stack;
    while (block->nodeType != LEAF) {
        int i;
        for(i = 0; i < block->numKeys; i++) {
            if (key < block->keys[i]) break;
        }
        stack.push(block);
        block = block->pointers[i];
    }

    bool keyExists = false;
    int index;
    for(index = 0; index < block->numKeys; index++) {
        if(block->keys[index] == key) {
            keyExists = true;
            break;
        }
    }

     if(keyExists) {
        if(block->pointers[index] != nullptr) {
            DuplicatesBlock* db = (DuplicatesBlock*)block->pointers[index];
            if(db->numKeys == 2*N) {
                DuplicatesBlock* newDb = (DuplicatesBlock*)malloc(sizeof(DuplicatesBlock));
                newDb->nodeType = DUPLICATES;
                newDb->numKeys = 1;
                newDb->pointers[0] = key;
                newDb->next = db;
                block->pointers[index] = (IndexBlock*)newDb;
                numDuplicates++;
            } else {
                db->pointers[db->numKeys++] = key;
            }
        } else {
            DuplicatesBlock* newDb = (DuplicatesBlock*)malloc(sizeof(DuplicatesBlock));
            newDb->nodeType = DUPLICATES;
            newDb->numKeys = 2;
            newDb->pointers[0] = key;
            newDb->pointers[1] = key;
            newDb->next = nullptr;
            block->pointers[index] = (IndexBlock*)newDb;
            numDuplicates++;
        }
    } else if(block->numKeys < N) {
        int i;
        for(i = block->numKeys; i > 0; i--){
            if (block->keys[i-1] > key) {
                block->keys[i] = block->keys[i-1];
                block->pointers[i] = block->pointers[i-1];
            } else break;
        }
        block->keys[i] = key;
        block->pointers[i] = nullptr;
        block->numKeys++;
    } else {
        IndexBlock* newBlock = (IndexBlock*)malloc(sizeof(IndexBlock));
        newBlock->pointers = (IndexBlock**)malloc(sizeof(IndexBlock*)*(N+1));
        newBlock->nodeType = block->nodeType;
        numLeaf++;

        uint32_t newKeys[N+1];
        IndexBlock* newPointers[N+1];
        bool added = false;
        int c = 0;
        for(int i = 0; i < block->numKeys; i++){
            if (key < block->keys[i] && !added) {
                newKeys[c] = key;
                newPointers[c++] = nullptr;
                added = true;
            }
            newKeys[c] = block->keys[i];
            newPointers[c++] = block->pointers[i];
        }

        if(!added) {
            newKeys[N] = key;
            newPointers[N] = nullptr;
        }

        int k = ceil((N+1)/2.0f);
        for(int i = 0; i < k; i++) {
            block->keys[i] = newKeys[i];
            block->pointers[i] = newPointers[i];
        }
        block->numKeys = k;

        for(int i = k; i < N+1; i++) {
            newBlock->keys[i-k] = newKeys[i];
            newBlock->pointers[i-k] = newPointers[i];
            newBlock->numKeys++;
        }

        newBlock->pointers[N] = block->pointers[N];
        block->pointers[N] = newBlock;

        IndexBlock* left = block;
        IndexBlock* right = newBlock;
        uint32_t keyToAdd = right->keys[0];
        bool split = true;

        while(stack.size() > 0){
            IndexBlock* parent = stack.top();
            stack.pop();

            if(parent->numKeys < N) {
                int i;
                for(i = parent->numKeys; i > 0; i--){
                    if(parent->keys[i-1] > keyToAdd) {
                        parent->keys[i] = parent->keys[i-1];
                        parent->pointers[i+1] = parent->pointers[i];
                    } else break;
                }
                parent->keys[i] = keyToAdd;
                parent->pointers[i+1] = right;
                parent->numKeys++;
                split = false;
                break;
            } else {
                uint32_t newKeys[N+1];
                IndexBlock* newPointers[N+1];
                bool added = false;
                int c = 0;
                for(int i = 0; i < parent->numKeys; i++){
                    if (key < parent->keys[i] && !added) {
                        newKeys[c] = keyToAdd;
                        newPointers[c++] = right;
                        added = true;
                    }
                    newKeys[c] = parent->keys[i];
                    newPointers[c++] = parent->pointers[i+1];
                }

                if(!added) {
                    newKeys[N] = keyToAdd;
                    newPointers[N] = right;
                }

                IndexBlock* newBlock = (IndexBlock*)malloc(sizeof(IndexBlock));
                newBlock->pointers = (IndexBlock**)malloc(sizeof(IndexBlock*)*(N+1));
                newBlock->nodeType = INTERNAL;
                numInternal++;

                int k = ceil(N/2.0f);
                for(int i = 0; i < k; i++) {
                    parent->keys[i] = newKeys[i];
                    parent->pointers[i+1] = newPointers[i];
                }
                parent->numKeys = k;

                for(int i = k; i < N; i++) {
                    newBlock->keys[i-k] = newKeys[i+1];
                    newBlock->pointers[i-k] = newPointers[i];
                    newBlock->numKeys++;
                }
                newBlock->pointers[newBlock->numKeys] = newPointers[N];

                left = parent;
                right = newBlock;
                keyToAdd = newKeys[k];
            }
        }

        if (split) {
            numInternal++;
            IndexBlock* newRoot = (IndexBlock*)malloc(sizeof(IndexBlock));
            newRoot->pointers = (IndexBlock**)malloc(sizeof(IndexBlock*)*(N+1));
            newRoot->nodeType = INTERNAL;
            newRoot->pointers[0] = left;
            newRoot->pointers[1] = right;
            newRoot->keys[0] = keyToAdd;
            newRoot->numKeys = 1;
            *root = newRoot;
        }

    }

}

void PrintTree(IndexBlock* root) {
    queue<IndexBlock*> q;
    q.push(root);

    while(q.size() > 0) {
        IndexBlock* block = q.front();
        q.pop();

        if(block->nodeType == INTERNAL) {
            for(int i = 0; i <= block->numKeys; i++){
                q.push(block->pointers[i]);
            }
        } 

        for (int i = 0; i < block->numKeys; i++){
            cout << " " << block->keys[i];
            if(block->nodeType == LEAF && block->pointers[i] != nullptr) {
                DuplicatesBlock* db = (DuplicatesBlock*)block->pointers[i];
                while(db){
                    for(int j = 1; j < db->numKeys; j++){
                        // cout << "-" << db->pointers[j];
                    }
                    db = db->next;
                }
            }
        }
        cout << endl;
    }
}


bool rootKeyNeedsUpdate(IndexBlock* root, uint32_t key) {
    // cout << "Root key " << key << endl;
    // cout << "Root value" << root->keys[0] << endl;

    for (int i = 0; i < root->numKeys; i++) {
        if (root->keys[i] == key) {
            return true;
        }
    }
    return false;
}

int getIndexOfKeyInRoot(IndexBlock* root, uint32_t key) {
    for (int i = 0; i < root->numKeys; i++) {
        if (root->keys[i] == key) {
            return i;
        }
    }
    return -1; // Key not found
}

void replaceKeyInRoot(IndexBlock* root, uint32_t oldKey, uint32_t newKey) {
    for (int i = 0; i < root->numKeys; i++) {
        if (root->keys[i] == oldKey) {
            root->keys[i] = newKey;
            return;
        }
    }
}

void replaceKeyInInternalNode(IndexBlock* node, uint32_t oldKey, uint32_t newKey) {
    for (int i = 0; i < node->numKeys; i++) {
        if (node->keys[i] == oldKey) {
            node->keys[i] = newKey;
            break;
        }
    }
}


uint32_t findInOrderSuccessor(IndexBlock* node, uint32_t key) {
    int keyIndex = 0;
    while (keyIndex < node->numKeys && node->keys[keyIndex] <= key) {
        keyIndex++;
    }

    IndexBlock* current = node->pointers[keyIndex];
    while (current->nodeType != LEAF) {
        current = current->pointers[0];
    }
    return current->keys[0];
}


bool keyIsInInternalNode(IndexBlock* node, uint32_t key) {
    if (node->nodeType == LEAF) {
        return false;
    }
    // cout << "checking" << endl;
    for (int i = 0; i < node->numKeys; i++) {
        if (node->keys[i] == key) {
            return true;
        }
    }
    return false;
}


void Delete(IndexBlock** root, uint32_t key) {
    IndexBlock* block = *root;
    stack<IndexBlock*> stack;
    while (block->nodeType != LEAF) {
        int i;
        for(i = 0; i < block->numKeys; i++) {
            if (key < block->keys[i]) break;
        }
        stack.push(block);
        block = block->pointers[i];
    }

    bool keyFound = false;
    int keyIndex;
    for(keyIndex = 0; keyIndex < block->numKeys; keyIndex++) {
        if (block->keys[keyIndex] == key) {
            keyFound = true;
            break;
        }
    }
     

    if (!keyFound) {
        cout << "Key " << key << " not found in the tree." << endl;
        return;
    }

    // cout << "Reached c " << key << endl;

    DuplicatesBlock* db = (DuplicatesBlock*) block->pointers[keyIndex];
    if(db != nullptr && db->nodeType == DUPLICATES) {
        do {
            DuplicatesBlock* next = db->next;
            free(db);
            numDuplicates--;
            db = next;
        } while(db != nullptr);
    }

    for(int i = keyIndex; i < block->numKeys - 1; i++) {
        block->keys[i] = block->keys[i + 1];
        block->pointers[i] = block->pointers[i + 1];
    }
    block->numKeys--;

    // cout << "Reached d " << key << endl;

    if (block->numKeys < ceil(N / 2.0f)) {
    // cout << "Leaf underflow" << endl;
    IndexBlock* parent = nullptr;
    IndexBlock* leftSibling = nullptr;
    IndexBlock* rightSibling = nullptr;
    int indexOfBlockInParent = -1;
    // cout << "Reached d " << key << endl;


    if (!stack.empty()) {
        parent = stack.top();
        // stack.pop();
        // cout << "Parent 1: " << parent->keys[0] << endl;
        
        for (int i = 0; i <= parent->numKeys; i++) {
            if (parent->pointers[i] == block) {
                indexOfBlockInParent = i;
                break;
            }
        }
        
        if (indexOfBlockInParent > 0) {
            leftSibling = parent->pointers[indexOfBlockInParent - 1];
            // cout << "Left Sibling: " << leftSibling->keys[0] << endl;

        }
        if (indexOfBlockInParent < parent->numKeys) {
            rightSibling = parent->pointers[indexOfBlockInParent + 1];
            // cout << "Right Sibling: " << rightSibling->keys[0] << endl;
        }

        if (leftSibling != nullptr && leftSibling->numKeys > ceil(N / 2.0f)) {
            // cout << "Leaf borrowing from left" << endl;
        for(int i = block->numKeys; i > 0; i--) {
            block->keys[i] = block->keys[i - 1];
            block->pointers[i] = block->pointers[i-1];
        }
        // cout << "c 1: "  << endl;
        block->pointers[1] = block->pointers[0];
        // cout << "c 2: " << endl;

        block->keys[0] = leftSibling->keys[leftSibling->numKeys - 1];

        parent->keys[indexOfBlockInParent - 1] = leftSibling->keys[leftSibling->numKeys - 1];

        block->pointers[0] = leftSibling->pointers[leftSibling->numKeys-1];

        leftSibling->numKeys--;

        block->numKeys++;
        
        } 
        else if (rightSibling != nullptr && rightSibling->numKeys > ceil(N / 2.0f)) {
            // cout << "Leaf borrowing from right" << endl;
        block->keys[block->numKeys] = parent->keys[indexOfBlockInParent];

        block->pointers[block->numKeys] = rightSibling->pointers[0];
        // cout << "Value:" << block->keys[block->numKeys]  << endl;
        // cout << "Value:" << parent->keys[indexOfBlockInParent]  << endl;
        

        for (int i = 0; i < rightSibling->numKeys - 1; i++) {
            rightSibling->keys[i] = rightSibling->keys[i + 1];
            rightSibling->pointers[i] = rightSibling->pointers[i + 1];
        }
        rightSibling->pointers[rightSibling->numKeys - 1] = rightSibling->pointers[rightSibling->numKeys];

        parent->keys[indexOfBlockInParent] = rightSibling->keys[0];

        block->numKeys++;
        rightSibling->numKeys--;
    }

    else {
        // cout << "Leaf merging" << endl;
    IndexBlock* sibling = (leftSibling != nullptr) ? leftSibling : rightSibling;
    int siblingIndex = (leftSibling != nullptr) ? indexOfBlockInParent - 1 : indexOfBlockInParent;
    
    IndexBlock* mergeInto = (leftSibling != nullptr) ? leftSibling : block;
    IndexBlock* mergeFrom = (leftSibling != nullptr) ? block : rightSibling;

    for (int i = 0; i < mergeFrom->numKeys; i++) {
        mergeInto->keys[mergeInto->numKeys] = mergeFrom->keys[i];
        mergeInto->pointers[mergeInto->numKeys] = mergeFrom->pointers[i];
        mergeInto->numKeys++;
    }   

    mergeInto->pointers[N]=mergeFrom->pointers[N];
    // cout << "PRRR" << mergeFrom->pointers[N]->keys[0] << endl;


    for (int i = siblingIndex + 1; i < parent->numKeys; i++) {
        parent->keys[i - 1] = parent->keys[i];
        parent->pointers[i] = parent->pointers[i + 1];
    }
    parent->numKeys--;
    numLeaf--;

    if (mergeFrom != mergeInto) {
        free(mergeFrom);
    }

    


    }
    // cout << "Reached 3 " << key << endl;


    while (!stack.empty()) {
    IndexBlock* parentNode = stack.top();
    // cout << "Parent: " << parentNode->keys[0] << endl;
    // cout << "Num: " << unsigned(parentNode->numKeys) << endl;

    stack.pop();

    if (parentNode->numKeys < floor(N / 2.0f)) {
        // cout << "Parent underflow" << endl;
        // cout << "Underflow: " << parentNode->keys[0] << endl;
        IndexBlock* grandParent = !stack.empty() ? stack.top() : nullptr;
        IndexBlock* leftParentSibling = nullptr;
        IndexBlock* rightParentSibling = nullptr;
        int indexOfParentInGrandparent = -1;


        if (grandParent != nullptr) {
            for (int i = 0; i <= grandParent->numKeys; ++i) {
                if (grandParent->pointers[i] == parentNode) {
                    indexOfParentInGrandparent = i;
                    // cout << "indexofp: " << indexOfParentInGrandparent << endl;

                    if (i > 0) leftParentSibling = grandParent->pointers[i - 1];
                    if (i < grandParent->numKeys) rightParentSibling = grandParent->pointers[i + 1];
                    // cout << "indexofp: " << rightParentSibling->keys[0] << endl;
                    break;
                }
            }
        }

        if (leftParentSibling != nullptr && leftParentSibling->numKeys > floor(N / 2.0f)) {
        // cout << "Left parent borrowing: " << parentNode->keys[0] << endl;
        parentNode->pointers[parentNode->numKeys + 1] = parentNode->pointers[parentNode->numKeys]; // Shift the pointer
        for (int i = parentNode->numKeys; i > 0; i--) {
            parentNode->keys[i] = parentNode->keys[i - 1];
            parentNode->pointers[i] = parentNode->pointers[i - 1];
        }
        parentNode->keys[0] = grandParent->keys[indexOfParentInGrandparent - 1];
        grandParent->keys[indexOfParentInGrandparent - 1] = leftParentSibling->keys[leftParentSibling->numKeys - 1];
        parentNode->pointers[0] = leftParentSibling->pointers[leftParentSibling->numKeys];
        parentNode->numKeys++;
        leftParentSibling->numKeys--;
        } 
        else if (rightParentSibling != nullptr && rightParentSibling->numKeys > floor(N / 2.0f)) {
        // cout << "Right parent borrowing: " << rightParentSibling->keys[0] << endl;
        parentNode->keys[parentNode->numKeys] = grandParent->keys[indexOfParentInGrandparent];
        parentNode->pointers[parentNode->numKeys + 1] = rightParentSibling->pointers[0];
        grandParent->keys[indexOfParentInGrandparent] = rightParentSibling->keys[0];
        for (int i = 0; i < rightParentSibling->numKeys - 1; i++) {
            rightParentSibling->keys[i] = rightParentSibling->keys[i + 1];
            rightParentSibling->pointers[i] = rightParentSibling->pointers[i + 1];
        }
        rightParentSibling->pointers[rightParentSibling->numKeys - 1] = rightParentSibling->pointers[rightParentSibling->numKeys];
        parentNode->numKeys++;
        rightParentSibling->numKeys--;
        }

        else{
        // cout << "Parent Merging " << endl;

        if (leftParentSibling != nullptr && leftParentSibling->numKeys <= floor(N / 2.0f)) {
            // cout << "Merging L " << endl;
            IndexBlock* successor = parentNode->pointers[0];
            while(successor->nodeType != LEAF) {
                successor = successor->pointers[0];
            }
            leftParentSibling->keys[leftParentSibling->numKeys] = successor->keys[0];
            for (int i = 0; i < parentNode->numKeys; ++i) {
                leftParentSibling->keys[leftParentSibling->numKeys + 1 + i] = parentNode->keys[i];
                leftParentSibling->pointers[leftParentSibling->numKeys + 1 + i] = parentNode->pointers[i];
            }
            leftParentSibling->pointers[leftParentSibling->numKeys + 1 + parentNode->numKeys] = parentNode->pointers[parentNode->numKeys];
            leftParentSibling->numKeys += (1 + parentNode->numKeys);

            for (int i = indexOfParentInGrandparent; i < grandParent->numKeys; ++i) {
                grandParent->keys[i - 1] = grandParent->keys[i];
                grandParent->pointers[i] = grandParent->pointers[i + 1];
            }
            
            grandParent->numKeys--;
            numInternal--;
        } 

        else if (rightParentSibling != nullptr && rightParentSibling->numKeys <= floor(N / 2.0f)) {
            // cout << "Merging R" << endl;
            parentNode->keys[parentNode->numKeys] = grandParent->keys[indexOfParentInGrandparent];

            for (int i = 0; i < rightParentSibling->numKeys; ++i) {
                parentNode->keys[parentNode->numKeys + 1 + i] = rightParentSibling->keys[i];
                parentNode->pointers[parentNode->numKeys + 1 + i] = rightParentSibling->pointers[i];
            }
            parentNode->pointers[parentNode->numKeys + rightParentSibling->numKeys + 1] = rightParentSibling->pointers[rightParentSibling->numKeys];

            parentNode->numKeys += rightParentSibling->numKeys + 1;

            for (int i = indexOfParentInGrandparent + 1; i < grandParent->numKeys; ++i) {
                grandParent->keys[i - 1] = grandParent->keys[i];
                grandParent->pointers[i] = grandParent->pointers[i + 1];
            }
            grandParent->numKeys--;
            numInternal--;

            free(rightParentSibling);
        }




        }

        
        
    }
    
    
    

    // Check if the current node is an internal node and contains the key
    if (parentNode->nodeType != LEAF && keyIsInInternalNode(parentNode, key)) {
        
        // Find the in-order successor of the key
        uint32_t inOrderSuccessor = findInOrderSuccessor(parentNode,key);

        // cout << "Changing "<< key << "with" << inOrderSuccessor << endl;
        // Replace the key in the internal node with the in-order successor
        replaceKeyInInternalNode(parentNode, key, inOrderSuccessor);
    }


    block = parentNode;
    }

    

    }



    }

    // cout << "Reached 1" << key << endl;
    if (*root != nullptr && (*root)->numKeys == 0 && (*root)->nodeType != LEAF) {
        // cout << "Boring update"  << endl;
        IndexBlock* newRoot = (*root)->pointers[0];
        free(*root);
        *root = newRoot;
        numInternal--;
    }

    // cout << "Reached 1"  << key << endl;

    if (*root != nullptr && (*root)->nodeType != LEAF && rootKeyNeedsUpdate(*root, key)) {
        // Navigate to the right subtree of the deleted key
        // cout << "Rooot" << endl;
        IndexBlock* current = (*root)->pointers[getIndexOfKeyInRoot(*root, key) + 1];

        // Find the in-order successor (leftmost key in the right subtree)
        while (current->nodeType != LEAF) {
            current = current->pointers[0];
        }

        // Replace the root key with the in-order successor key
        replaceKeyInRoot(*root, key, current->keys[0]);
    }
    
    while (!stack.empty()) {
        IndexBlock* currentNode = stack.top();
        // cout << "Change" << endl;
        stack.pop();

        // Check if the current node is an internal node and contains the key
        if (currentNode->nodeType != LEAF && keyIsInInternalNode(currentNode, key)) {
            // Find the in-order successor of the key
            uint32_t inOrderSuccessor = findInOrderSuccessor(currentNode,key);

            // Replace the key in the internal node with the in-order successor
            replaceKeyInInternalNode(currentNode, key, inOrderSuccessor);
        }
    }


    // cout << "Deleted: " << key << endl;
}

#define ASSERT(cond, msg, args...) assert((cond) || !fprintf(stderr, (msg "\n"), args))

typedef struct vData {
    IndexBlock* p;
    uint32_t n1;
    uint32_t n2;
} vData;

void VerifyTree(IndexBlock* root, uint32_t globalMin, uint32_t globalMax) {
    // cout << "Verifying Tree… ";
    uint32_t nLeaf = 0;
    uint32_t nInternal = 0;
    uint32_t nDuplicates = 0;
    set<uint32_t> internalKeySet;
    set<uint32_t> leafKeySet;

    queue<vData> q;
    q.push((vData){root, globalMin, globalMax});
    bool skip = true; // skip min key amount check for the first time, i.e, for the root

    while(q.size() > 0) {
        vData data = q.front();
        q.pop();

        IndexBlock* block = data.p;
        if(block->nodeType == INTERNAL){
            nInternal++;
            ASSERT(block->keys[0] >= data.n1, "[INTERNAL] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT(block->keys[block->numKeys - 1] <= data.n2, "[INTERNAL] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(skip || block->numKeys >= floor(N/2.0f), "[INTERNAL] Expected atleast floor(n/2) keys. Found %d", block->numKeys);
            uint32_t min = data.n1;
            for(int i = 0; i < block->numKeys; i++) {
                ASSERT(internalKeySet.count(block->keys[i]) == 0, "[INTERNAL] Found duplicate key %d within internal nodes", block->keys[i]);
                internalKeySet.insert(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[INTERNAL] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                q.push((vData){block->pointers[i],min,block->keys[i]-1});
                min = block->keys[i];
            }
            q.push((vData){block->pointers[block->numKeys],min,data.n2});


        } else if(block->nodeType == LEAF) {
            ASSERT(nInternal == numInternal, "Expected %d internal nodes, traveresed %d", numInternal, nInternal);
            nLeaf++;
            ASSERT((skip && block->numKeys == 0) || block->keys[0] >= data.n1, "[LEAF] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT((skip && block->numKeys == 0) || block->keys[block->numKeys - 1] <= data.n2, "[LEAF] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(skip || block->numKeys >= floor((N+1)/2.0f), "[LEAF] Expected atleast floor((n+1)/2). Found %d", block->numKeys);
            ASSERT(skip || block->pointers[N] == q.front().p || (numDuplicates > 0 && q.front().p->nodeType == DUPLICATES) || (numDuplicates == 0 && q.size() == 0), "[LEAF] next (%lx) != q.next (%lx)", (uintptr_t)block->pointers[N], (uintptr_t)q.front().p);
            skip = false;
            for(int i = 0; i < block->numKeys; i++) {
                ASSERT(leafKeySet.count(block->keys[i]) == 0, "[LEAD] Found duplicate key %d within leaf nodes", block->keys[i]);
                leafKeySet.insert(block->keys[i]);
                if(internalKeySet.count(block->keys[i])) internalKeySet.erase(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[LEAF] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                if(block->pointers[i] != nullptr) {
                    q.push((vData){block->pointers[i],block->keys[i],0});
                }
            }

        } else if(block->nodeType == DUPLICATES) {
            DuplicatesBlock* dB = (DuplicatesBlock*)block;
            nDuplicates++;
            ASSERT(dB->numKeys > 0 && (dB->numKeys > 1 || dB->next != nullptr), "Duplicate Block has %d keys!", dB->numKeys);
            while(dB->next) {
                nDuplicates++;
                dB = dB->next;
                ASSERT(dB->numKeys == N*2, "Inner Duplicate Block has %d keys", dB->numKeys);
            }
        } else if(block->nodeType == 0 || block->nodeType == 1) {

        } else {
            ASSERT(0 == 1, "Invalid Block Type %d", block->nodeType);
        }
    }
    ASSERT(nLeaf == numLeaf, "Expected %d leaf nodes, traveresed %d", numLeaf, nLeaf);
    ASSERT(internalKeySet.size() == 0, "Found %lu internal keys that do not appear in leaves", internalKeySet.size());
    ASSERT(keySet == leafKeySet, "Mismatch of %d keys between expected keys and leaf keys", abs((int)(keySet.size() - leafKeySet.size())));
    ASSERT(nDuplicates == numDuplicates, "Expected %d duplicate nodes, traveresed %d", numDuplicates, nDuplicates);
    // cout << "Passed!" << endl;
}

int main() {
    auto rng = std::default_random_engine {};

    for(int i = 11; i < 100; i++) {
        numInternal = 0;
        numLeaf = 1;
        numDuplicates = 0;
        cout << "Seed " << i << endl;
        rng.seed(i);

        IndexBlock* root = (IndexBlock*)malloc(sizeof(IndexBlock));
        root->pointers = (IndexBlock**)malloc(sizeof(IndexBlock*)*(N+1));
        root->nodeType = LEAF;
        root->numKeys = 0;

        ifstream tsv("data.tsv");
        int numRecords = 0;
        string line;
        getline(tsv, line); // consume the header line
            
        while(getline(tsv, line)) {
            if(line.empty()) continue; // ignore empty lines
            size_t tabPos1 = line.find("\t");
            size_t tabPos2 = line.substr(tabPos1+1).find("\t");
            uint32_t insert = std::stoi(line.substr(tabPos1 + tabPos2 + 2));
            Insert(&root, insert);
        }
        cout << "Tree Built" << endl;
        VerifyTree(root, 5, 2279223);
        
        std::vector<uint32_t> output;
        std::copy(keySet.begin(), keySet.end(), std::back_inserter(output));

        std::shuffle(std::begin(output), std::end(output), rng);
        cout << "Random deletion from " << output[0] << " to " << output[output.size()-1] << endl;

        int k = 0;
        for(uint32_t rit : output) {
            k++;
            Delete(&root, rit);
            keySet.erase(rit);
            if(keySet.size() > 0 && k % 50 == 0) {
                VerifyTree(root, *keySet.begin(), *keySet.rbegin());
            }
            // if(keySet.size() > 0) {
            //     VerifyTree(root, *keySet.begin(), *keySet.rbegin());
            // } else {
            //     VerifyTree(root, 0, 0);
            // }
        }
    }
}
