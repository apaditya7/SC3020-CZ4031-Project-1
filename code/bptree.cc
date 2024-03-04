#include "bptree.h"
#include <stack>
#include <queue>
#include <iostream>

using namespace std;

int keyIndexInNode(IndexBlock* node, uint32_t key) {
    for (int i = 0; i < node->numKeys; i++) {
        if (node->keys[i] == key) return i;
    }
    return -1;
}

void BPTree::Insert(Disk* disk, int key, RecordPointer pointer) {
    keySet.insert(key);
    IndexBlock* block = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    stack<IndexBlock*> blockStack;
    stack<RecordPointer> pointerStack;
    pointerStack.push(*rootPointer);
    while (block->nodeType != TYPE_LEAF) {
        int i;
        for(i = 0; i < block->numKeys; i++) {
            if (key < block->keys[i]) break;
        }
        blockStack.push(block);
        pointerStack.push(block->pointers[i]);
        block = (IndexBlock*)disk->ReadBlock(block->pointers[i].blockNumber);
    }
    int index = keyIndexInNode(block, key);

    if(index != -1) {
        OverflowBlock* ob = (OverflowBlock*)disk->ReadBlock(block->pointers[index].blockNumber);
        if(ob->nodeType == TYPE_OVERFLOW) {
            if(ob->numKeys == 2*N) {
                numOverflow++;
                OverflowBlock* newDb = (OverflowBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
                newDb->nodeType = TYPE_OVERFLOW;
                newDb->numKeys = 1;
                newDb->pointers[0] = pointer;
                newDb->next = (RecordPointer){block->pointers[index].blockNumber, 0};
                block->pointers[index] = (RecordPointer){currentFreeBlock, 0};
                disk->WriteBlock(pointerStack.top().blockNumber,(uint8_t*)block);
                pointerStack.pop();
                disk->WriteBlock(currentFreeBlock++, (uint8_t*)newDb);
                free(newDb);
            } else {
                ob->pointers[ob->numKeys++] = pointer;
                disk->WriteBlock(block->pointers[index].blockNumber, (uint8_t*)ob);
            }
        } else {
            numOverflow++;
            OverflowBlock* newDb = (OverflowBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
            newDb->nodeType = TYPE_OVERFLOW;
            newDb->numKeys = 2;
            newDb->pointers[0] = block->pointers[index];
            newDb->pointers[1] = pointer;
            newDb->next = (RecordPointer){0,0};
            block->pointers[index] = (RecordPointer){currentFreeBlock, 0};
            disk->WriteBlock(pointerStack.top().blockNumber,(uint8_t*)block);
            pointerStack.pop();
            disk->WriteBlock(currentFreeBlock++, (uint8_t*)newDb);
            free(newDb);
        }
        free(ob);
        free(block);
    } else if(block->numKeys < N) {
        int i;
        for(i = block->numKeys; i > 0; i--){
            if (block->keys[i-1] > key) {
                block->keys[i] = block->keys[i-1];
                block->pointers[i] = block->pointers[i-1];
            } else break;
        }
        block->keys[i] = key;
        block->pointers[i] = pointer;
        block->numKeys++;
        disk->WriteBlock(pointerStack.top().blockNumber, (uint8_t*)block);
        free(block);
    } else {
        IndexBlock* newBlock = (IndexBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
        newBlock->nodeType = TYPE_LEAF;
        numLeaf++;

        uint32_t newKeys[N+1];
        RecordPointer newPointers[N+1];
        bool added = false;
        int c = 0;
        for(int i = 0; i < block->numKeys; i++){
            if (key < block->keys[i] && !added) {
                newKeys[c] = key;
                newPointers[c++] = pointer;
                added = true;
            }
            newKeys[c] = block->keys[i];
            newPointers[c++] = block->pointers[i];
        }

        if(!added) {
            newKeys[N] = key;
            newPointers[N] = pointer;
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

        RecordPointer left = pointerStack.top();
        pointerStack.pop();
        RecordPointer right = (RecordPointer){currentFreeBlock, 0};
        uint32_t keyToAdd = newBlock->keys[0];
        bool split = true;

        newBlock->pointers[N] = block->pointers[N];
        block->pointers[N] = right;

        disk->WriteBlock(left.blockNumber, (uint8_t*)block);
        disk->WriteBlock(currentFreeBlock++, (uint8_t*)newBlock);
        free(newBlock);
        free(block);

        while(blockStack.size() > 0){
            IndexBlock* parent = blockStack.top();
            blockStack.pop();

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
                disk->WriteBlock(pointerStack.top().blockNumber, (uint8_t*)parent);
                free(parent);
                break;
            } else {
                uint32_t newKeys[N+1];
                RecordPointer newPointers[N+1];
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

                IndexBlock* newBlock = (IndexBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
                newBlock->nodeType = TYPE_INTERNAL;
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

                left = pointerStack.top();
                pointerStack.pop();
                right = (RecordPointer){currentFreeBlock, 0};
                keyToAdd = newKeys[k];

                disk->WriteBlock(left.blockNumber, (uint8_t*)parent);
                disk->WriteBlock(currentFreeBlock++, (uint8_t*)newBlock);
                free(parent);
                free(newBlock);
            }
        }

        if (split) {
            numLevels++;
            numInternal++;
            IndexBlock* newRoot = (IndexBlock*)calloc(BLOCK_SIZE, sizeof(uint8_t));
            newRoot->nodeType = TYPE_INTERNAL;
            newRoot->pointers[0] = left;
            newRoot->pointers[1] = right;
            newRoot->keys[0] = keyToAdd;
            newRoot->numKeys = 1;
            *rootPointer = (RecordPointer){currentFreeBlock, 0};
            disk->WriteBlock(currentFreeBlock++, (uint8_t*)newRoot);
            free(newRoot);
        }
    }

    while(blockStack.size() > 0){
        free(blockStack.top());
        blockStack.pop();
    }
}

IndexedSearchResult BPTree::Search(Disk* disk, int min, int max) {
    uint32_t nInternal = 0;
    uint32_t nLeaf = 0;
    uint32_t nOverflow = 0;

    std::set<int> visitedData;
    uint32_t totalRecords = 0;
    float totalRating = 0;

    IndexBlock *block = (IndexBlock *)disk->ReadBlock(rootPointer->blockNumber);

    while (block->nodeType != TYPE_LEAF) // search the key until leaf node
    {
        int i;
        for (i = 0; i < block->numKeys; i++)
        {
            if (min < block->keys[i])
                break;
        }
        uint32_t nextBlock = block->pointers[i].blockNumber;
        free(block);
        block = (IndexBlock *)disk->ReadBlock(nextBlock);
        nInternal++;
    }

    nLeaf++;

    int keyIndex;
    for (keyIndex = 0; keyIndex < block->numKeys; keyIndex++)
    {
        if (block->keys[keyIndex] >= min)
        {
            break;
        }
    }

    while(block->keys[keyIndex] <= max) {
        OverflowBlock *ob = (OverflowBlock *)disk->ReadBlock(block->pointers[keyIndex].blockNumber);
        if(ob->nodeType == TYPE_OVERFLOW) {
            while (true) // to iterate through overflow blocks
            {
                nOverflow++;
                for (int j = 0; j < ob->numKeys; j++) // to iterate the keys in an overflow block
                {
                    visitedData.insert(ob->pointers[j].blockNumber);
                    DataBlock *temp = (DataBlock *)disk->ReadBlock(ob->pointers[j].blockNumber);
                    totalRating += temp->records[ob->pointers[j].recordIndex].averageRating;
                    totalRecords++;
                    free(temp);
                }
                if (ob->next.blockNumber == 0)
                {
                    break;
                }
                uint32_t nextBlock = ob->next.blockNumber;
                free(ob);
                ob = (OverflowBlock *)disk->ReadBlock(nextBlock); // go to the next overflow block
            }
        } else {
            DataBlock* temp = (DataBlock*)ob;
            visitedData.insert(block->pointers[keyIndex].blockNumber);
            totalRating += temp->records[block->pointers[keyIndex].recordIndex].averageRating;
            totalRecords++;
        }
        free(ob);

        keyIndex++;

        if(keyIndex == block->numKeys) {
            uint32_t nextBlock = block->pointers[N].blockNumber;
            if(block->keys[keyIndex] == max || nextBlock == 0) break;
            free(block);
            block = (IndexBlock*)disk->ReadBlock(nextBlock);
            nLeaf++;
            keyIndex = 0;
        }
    }

    free(block);

    return (IndexedSearchResult) { nInternal, nLeaf, nOverflow, visitedData.size(), totalRecords, totalRating/(float)totalRecords };
}

uint32_t findInOrderSuccessor(Disk* disk, IndexBlock* node, uint32_t key) {
    int keyIndex = 0;
    while (keyIndex < node->numKeys && node->keys[keyIndex] <= key) keyIndex++;
    IndexBlock* current = (IndexBlock*)disk->ReadBlock(node->pointers[keyIndex].blockNumber);
    while (current->nodeType != TYPE_LEAF) {
        RecordPointer nextBlock = current->pointers[0];
        free(current);
        current = (IndexBlock*)disk->ReadBlock(nextBlock.blockNumber);

    }
    uint32_t successor = current->keys[0];
    free(current);
    return successor;
}

bool isEmpty(DataBlock* block) {
    for(int i = 0; i < RECORDS_PER_BLOCK; i++) {
        if(block->records[i].occupied) return false;
    }
    return true;
}

void BPTree::Delete(Disk* disk, uint32_t key) {
    IndexBlock* block = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    stack<IndexBlock*> blockStack;
    stack<RecordPointer> pointerStack;
    pointerStack.push(*rootPointer);
    while (block->nodeType != TYPE_LEAF) {
        int i;
        for(i = 0; i < block->numKeys; i++) {
            if (key < block->keys[i]) break;
        }
        blockStack.push(block);
        pointerStack.push(block->pointers[i]);
        block = (IndexBlock*)disk->ReadBlock(block->pointers[i].blockNumber);
    }

    int keyIndex = keyIndexInNode(block, key);
     
    if (keyIndex == -1) {
        cout << "Key " << key << " not found in the tree." << endl;
        return;
    }

    keySet.erase(key);

    OverflowBlock* ob = (OverflowBlock*) disk->ReadBlock(block->pointers[keyIndex].blockNumber);
    if(ob->nodeType == TYPE_OVERFLOW) {
        do {
            for(int i = 0; i < ob->numKeys; i++) {
                DataBlock* b = (DataBlock*) disk->ReadBlock(ob->pointers[i].blockNumber);
                b->records[ob->pointers[i].recordIndex].occupied = false;
                disk->WriteBlock(ob->pointers[i].blockNumber, (uint8_t*)b);
                if(isEmpty(b)) numDataBlocks--;
                free(b);
                numRecords--;
            }

            OverflowBlock* next = nullptr;
            if(ob->next.blockNumber != 0) {
                next = (OverflowBlock*)disk->ReadBlock(ob->next.blockNumber);
            }
            
            free(ob);
            numOverflow--;
            ob = next;
        } while(ob != nullptr);
    } else {
        DataBlock* db = (DataBlock*) ob;
        db->records[block->pointers[keyIndex].recordIndex].occupied = false;
        disk->WriteBlock(block->pointers[keyIndex].blockNumber, (uint8_t*)db);
        numRecords--;
        if(isEmpty(db)) numDataBlocks--;
        free(db);
    }

    for(int i = keyIndex; i < block->numKeys - 1; i++) {
        block->keys[i] = block->keys[i + 1];
        block->pointers[i] = block->pointers[i + 1];
    }
    block->numKeys--;

    RecordPointer childPointer = pointerStack.top();
    pointerStack.pop();

    disk->WriteBlock(childPointer.blockNumber, (uint8_t*)block);

    if (block->numKeys < ceil(N / 2.0f)) {
        IndexBlock* parent = nullptr;
        IndexBlock* leftSibling = nullptr;
        IndexBlock* rightSibling = nullptr;
        int indexOfBlockInParent = -1;

        if (!blockStack.empty()) {
            parent = blockStack.top();
            RecordPointer parentPointer = pointerStack.top();
            
            for (int i = 0; i <= parent->numKeys; i++) {
                if (parent->pointers[i].blockNumber == childPointer.blockNumber) {
                    indexOfBlockInParent = i;
                    break;
                }
            }
            
            if (indexOfBlockInParent > 0) leftSibling = (IndexBlock*)disk->ReadBlock(parent->pointers[indexOfBlockInParent - 1].blockNumber);
            if (indexOfBlockInParent < parent->numKeys) rightSibling = (IndexBlock*)disk->ReadBlock(parent->pointers[indexOfBlockInParent + 1].blockNumber);

            if (leftSibling != nullptr && leftSibling->numKeys > ceil(N / 2.0f)) {
                for(int i = block->numKeys; i > 0; i--) {
                    block->keys[i] = block->keys[i - 1];
                    block->pointers[i] = block->pointers[i-1];
                }
                block->pointers[1] = block->pointers[0];
                block->keys[0] = leftSibling->keys[leftSibling->numKeys - 1];

                parent->keys[indexOfBlockInParent - 1] = leftSibling->keys[leftSibling->numKeys - 1];
                block->pointers[0] = leftSibling->pointers[leftSibling->numKeys-1];
                leftSibling->numKeys--;
                block->numKeys++;

                disk->WriteBlock(parentPointer.blockNumber, (uint8_t*) parent);
                disk->WriteBlock(childPointer.blockNumber, (uint8_t*) block);
                disk->WriteBlock(parent->pointers[indexOfBlockInParent - 1].blockNumber, (uint8_t*) leftSibling);
            
            } 
            else if (rightSibling != nullptr && rightSibling->numKeys > ceil(N / 2.0f)) {
                block->keys[block->numKeys] = parent->keys[indexOfBlockInParent];
                block->pointers[block->numKeys] = rightSibling->pointers[0];
                
                for (int i = 0; i < rightSibling->numKeys - 1; i++) {
                    rightSibling->keys[i] = rightSibling->keys[i + 1];
                    rightSibling->pointers[i] = rightSibling->pointers[i + 1];
                }

                rightSibling->pointers[rightSibling->numKeys - 1] = rightSibling->pointers[rightSibling->numKeys];
                parent->keys[indexOfBlockInParent] = rightSibling->keys[0];
                block->numKeys++;
                rightSibling->numKeys--;

                disk->WriteBlock(parentPointer.blockNumber, (uint8_t*) parent);
                disk->WriteBlock(childPointer.blockNumber, (uint8_t*) block);
                disk->WriteBlock(parent->pointers[indexOfBlockInParent + 1].blockNumber, (uint8_t*) rightSibling);
            }

            else {
                int siblingIndex = (leftSibling != nullptr) ? indexOfBlockInParent - 1 : indexOfBlockInParent;
                
                IndexBlock* mergeInto = (leftSibling != nullptr) ? leftSibling : block;
                IndexBlock* mergeFrom = (leftSibling != nullptr) ? block : rightSibling;
                RecordPointer mergePointer = (leftSibling != nullptr) ? parent->pointers[indexOfBlockInParent - 1] : childPointer;

                for (int i = 0; i < mergeFrom->numKeys; i++) {
                    mergeInto->keys[mergeInto->numKeys] = mergeFrom->keys[i];
                    mergeInto->pointers[mergeInto->numKeys] = mergeFrom->pointers[i];
                    mergeInto->numKeys++;
                }   

                mergeInto->pointers[N]=mergeFrom->pointers[N];

                for (int i = siblingIndex + 1; i < parent->numKeys; i++) {
                    parent->keys[i - 1] = parent->keys[i];
                    parent->pointers[i] = parent->pointers[i + 1];
                }
                parent->numKeys--;
                numLeaf--;

                disk->WriteBlock(mergePointer.blockNumber, (uint8_t*) mergeInto);
                disk->WriteBlock(parentPointer.blockNumber, (uint8_t*) parent);
            }

            if(leftSibling != nullptr) free(leftSibling);
            if(rightSibling != nullptr) free(rightSibling);

            while (!blockStack.empty()) {
                IndexBlock* parentNode = blockStack.top();
                blockStack.pop();
                RecordPointer parentNodePointer = pointerStack.top();
                pointerStack.pop();

                if (parentNode->numKeys < floor(N / 2.0f)) {
                    IndexBlock* grandParent = !blockStack.empty() ? blockStack.top() : nullptr;
                    RecordPointer grandParentPointer = !pointerStack.empty() ? pointerStack.top() : (RecordPointer){0,0};
                    IndexBlock* leftParentSibling = nullptr;
                    IndexBlock* rightParentSibling = nullptr;
                    int indexOfParentInGrandparent = -1;


                    if (grandParent != nullptr) {
                        for (int i = 0; i <= grandParent->numKeys; ++i) {
                            if (grandParent->pointers[i].blockNumber == parentNodePointer.blockNumber) {
                                indexOfParentInGrandparent = i;
                                if (i > 0) leftParentSibling = (IndexBlock*)disk->ReadBlock(grandParent->pointers[i - 1].blockNumber);
                                if (i < grandParent->numKeys) rightParentSibling = (IndexBlock*)disk->ReadBlock(grandParent->pointers[i + 1].blockNumber);
                                break;
                            }
                        }
                    }

                    if (leftParentSibling != nullptr && leftParentSibling->numKeys > floor(N / 2.0f)) {
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

                        disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*) parentNode);
                        disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                        disk->WriteBlock(grandParent->pointers[indexOfParentInGrandparent - 1].blockNumber, (uint8_t*) leftParentSibling);
                    } 
                    else if (rightParentSibling != nullptr && rightParentSibling->numKeys > floor(N / 2.0f)) {
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

                        disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*) parentNode);
                        disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                        disk->WriteBlock(grandParent->pointers[indexOfParentInGrandparent + 1].blockNumber, (uint8_t*) rightParentSibling);
                    }

                    else {
                        if (leftParentSibling != nullptr && leftParentSibling->numKeys <= floor(N / 2.0f)) {
                            RecordPointer leftPointer = grandParent->pointers[indexOfParentInGrandparent - 1];
                            IndexBlock* successor = (IndexBlock*)disk->ReadBlock(parentNode->pointers[0].blockNumber);
                            while(successor->nodeType != TYPE_LEAF)  {
                                RecordPointer nextBlock = successor->pointers[0];
                                free(successor);
                                successor = (IndexBlock*)disk->ReadBlock(nextBlock.blockNumber);
                            }
                            leftParentSibling->keys[leftParentSibling->numKeys] = successor->keys[0];
                            free(successor);

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

                            disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                            disk->WriteBlock(leftPointer.blockNumber, (uint8_t*) leftParentSibling);
                        } 

                        else if (rightParentSibling != nullptr && rightParentSibling->numKeys <= floor(N / 2.0f)) {
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

                            disk->WriteBlock(grandParentPointer.blockNumber, (uint8_t*) grandParent);
                            disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*) parentNode);
                        }

                    }

                    if(leftParentSibling != nullptr) free(leftParentSibling);
                    if(rightParentSibling != nullptr) free(rightParentSibling);
                }

                int parentUpdate = keyIndexInNode(parentNode, key);
                // Check if the current node is an internal node and contains the key
                if (parentNode->nodeType != TYPE_LEAF && parentUpdate != -1) {
                    
                    // Find the in-order successor of the key
                    uint32_t inOrderSuccessor = findInOrderSuccessor(disk, parentNode, key);

                    // Replace the key in the internal node with the in-order successor
                    parentNode->keys[parentUpdate] = inOrderSuccessor;
                    disk->WriteBlock(parentNodePointer.blockNumber, (uint8_t*)parentNode);
                }

                block = parentNode;
            }
        }
    }

    free(block);

    IndexBlock* root = (IndexBlock*)disk->ReadBlock(rootPointer->blockNumber);
    if (root->numKeys == 0 && root->nodeType != TYPE_LEAF) {
        rootPointer->blockNumber = root->pointers[0].blockNumber;
        numInternal--;
        numLevels--;
    }

    int rootUpdate = keyIndexInNode(root, key);
    if (root->nodeType != TYPE_LEAF && rootUpdate != -1) {
        // Navigate to the right subtree of the deleted key
        IndexBlock* current = (IndexBlock*)disk->ReadBlock(root->pointers[rootUpdate + 1].blockNumber);

        // Find the in-order successor (leftmost key in the right subtree)
        while (current->nodeType != TYPE_LEAF) {
            RecordPointer nextBlock = current->pointers[0];
            free(current);
            current = (IndexBlock*)disk->ReadBlock(nextBlock.blockNumber);
        }

        // Replace the root key with the in-order successor key
        root->keys[rootUpdate] = current->keys[0];
        disk->WriteBlock(rootPointer->blockNumber, (uint8_t*)root);
        free(current);
    }
    free(root);
    
    while (!blockStack.empty()) {
        IndexBlock* currentNode = blockStack.top();
        blockStack.pop();
        RecordPointer nodePointer = pointerStack.top();
        pointerStack.pop();

        // Check if the current node is an internal node and contains the key
        int keyUpdate = keyIndexInNode(currentNode, key);
        if (currentNode->nodeType != TYPE_LEAF && keyUpdate != -1) {
            // Find the in-order successor of the key
            uint32_t inOrderSuccessor = findInOrderSuccessor(disk, currentNode, key);

            // Replace the key in the internal node with the in-order successor
            currentNode->keys[keyUpdate] = inOrderSuccessor;
            disk->WriteBlock(nodePointer.blockNumber,(uint8_t*) currentNode);
        }
        free(currentNode);
    }
}

void BPTree::VerifyTree(Disk* disk) {
    cout << endl << "Verifying Tree… ";
    uint32_t nLeaf = 0;
    uint32_t nInternal = 0;
    uint32_t nOverflow = 0;
    set<uint32_t> dataBlocks;
    set<uint32_t> pointers;
    set<uint32_t> internalKeySet;
    set<uint32_t> leafKeySet;

    queue<vData> q;
    q.push((vData){*rootPointer, 5, 2279223});
    bool skip = true; // skip min key amount check for the first time, i.e, for the root

    while(q.size() > 0) {
        vData data = q.front();
        q.pop();

        IndexBlock* block = (IndexBlock*)disk->ReadBlock(data.p.blockNumber);
        bool dontFree = false;
        if(block->nodeType == TYPE_INTERNAL){
            nInternal++;
            ASSERT(block->keys[0] >= data.n1, "[INTERNAL] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT(block->keys[block->numKeys - 1] <= data.n2, "[INTERNAL] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(skip || block->numKeys >= floor(N/2.0f), "[INTERNAL] Expected atleast floor(n/2) keys. Found %d", block->numKeys);
            skip = false;
            uint32_t min = data.n1;
            for(int i = 0; i < block->numKeys; i++) {
                ASSERT(internalKeySet.count(block->keys[i]) == 0, "[INTERNAL] Found duplicate key %d within internal nodes", block->keys[i]);
                internalKeySet.insert(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[INTERNAL] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                q.push((vData){block->pointers[i],min,block->keys[i]-1});
                min = block->keys[i];
            }
            q.push((vData){block->pointers[block->numKeys],min,data.n2});


        } else if(block->nodeType == TYPE_LEAF) {
            ASSERT(nInternal == numInternal, "Expected %d internal nodes, traveresed %d", numInternal, nInternal);
            nLeaf++;
            ASSERT((skip && block->numKeys == 0) || block->keys[0] >= data.n1, "[LEAF] Minimum is %d, found %d", data.n1, block->keys[0]);
            ASSERT((skip && block->numKeys == 0) || block->keys[block->numKeys - 1] <= data.n2, "[LEAF] Maximum is %d, found %d", data.n2, block->keys[block->numKeys - 1]);
            ASSERT(skip || block->numKeys >= floor((N+1)/2.0f), "[LEAF] Expected atleast floor((n+1)/2). Found %d", block->numKeys);
            IndexBlock* nextBlock = (IndexBlock*)disk->ReadBlock(q.front().p.blockNumber);
            ASSERT(skip || block->pointers[N].blockNumber == q.front().p.blockNumber || nextBlock->nodeType == TYPE_OVERFLOW || nextBlock->nodeType == 0 || nextBlock->nodeType == 1, "[LEAF] next (%d) != q.next (%d)", block->pointers[N].blockNumber, q.front().p.blockNumber);
            free(nextBlock);
            skip = false;
            for(int i = 0; i < block->numKeys; i++) {
                ASSERT(leafKeySet.count(block->keys[i]) == 0, "[LEAD] Found duplicate key %d within leaf nodes", block->keys[i]);
                leafKeySet.insert(block->keys[i]);
                if(internalKeySet.count(block->keys[i])) internalKeySet.erase(block->keys[i]);
                if(i >= 1) ASSERT(block->keys[i] > block->keys[i-1], "[LEAF] Node not sorted (%d is after %d)", block->keys[i], block->keys[i-1]);
                q.push((vData){block->pointers[i],block->keys[i],0});
            }

        } else if(block->nodeType == TYPE_OVERFLOW) {
            OverflowBlock* dB = (OverflowBlock*)block;
            nOverflow++;
            ASSERT(dB->numKeys > 0 && (dB->numKeys > 1 || dB->next.blockNumber != 0), "Overflow Block has %d keys!", dB->numKeys);
            for(int i = 0; i < dB->numKeys; i++) {
                q.push((vData){dB->pointers[i],data.n1,data.n2});
            }
            if(dB->next.blockNumber != 0) {
                while(dB->next.blockNumber != 0) {
                    nOverflow++;
                    uint32_t nextBlock = dB->next.blockNumber;
                    free(dB);
                    dB = (OverflowBlock*)disk->ReadBlock(nextBlock);
                    ASSERT(dB->numKeys == N*2, "Inner Overflow Block has %d keys", dB->numKeys);
                    for(int i = 0; i < dB->numKeys; i++) {
                        q.push((vData){dB->pointers[i],data.n1,data.n2});
                    }
                }
                free(dB);
                dontFree = true;
            }
        } else if(block->nodeType == 0 || block->nodeType == 1) {
            dataBlocks.insert(data.p.blockNumber);
            pointers.insert((data.p.blockNumber << 4) + data.p.recordIndex);
            uint32_t i = data.p.recordIndex;
            ASSERT(i >= 0 && i < RECORDS_PER_BLOCK, "Record Index %d is out of range!", i);
            DataBlock* dataBlock = (DataBlock*)block;
            ASSERT(dataBlock->records[i].numVotes == data.n1, "Record Mismatch! Expected %d but found %d", data.n1, dataBlock->records[i].numVotes);
        } else {
            ASSERT(0 == 1, "Invalid Block Type %d", block->nodeType);
        }
        if (!dontFree) free(block);
    }

    ASSERT(nLeaf == numLeaf, "Expected %d leaf nodes, traveresed %d", numLeaf, nLeaf);
    ASSERT(keySet == leafKeySet, "Mismatch of %d keys between expected keys and leaf keys", abs((int)(keySet.size() - leafKeySet.size())));
    ASSERT(internalKeySet.size() == 0, "Found %lu internal keys that do not appear in leaves", internalKeySet.size());
    ASSERT(nOverflow == numOverflow, "Expected %d overflow nodes, traveresed %d", numOverflow, nOverflow);
    ASSERT(dataBlocks.size() == numDataBlocks, "Expected %u data blocks, traveresed %lu", numDataBlocks, dataBlocks.size());
    ASSERT(pointers.size() == numRecords, "Expected %u records, traveresed %lu", numRecords, pointers.size());

    cout << "Passed!" << endl;
}

BPTree::~BPTree() {
    free(rootPointer);
}