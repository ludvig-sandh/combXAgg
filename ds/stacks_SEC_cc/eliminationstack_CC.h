
#ifndef _ELIMINATION_STACK_H
#define _ELIMINATION_STACK_H

#include <pool_CC.h>

typedef struct EliminationStack {
    volatile void** val;
    int size;
} EliminationStack;

typedef struct EliminationStackLegacy {
    volatile void* val;
    struct EliminationStackLegacy* next;
    int sentinel;
} EliminationStackLegacy;


void Eliminate(volatile void* node0, volatile void* node1);

EliminationStack* EliminationStackAllocate(int h);
int EliminationStackEmpty(EliminationStack* stack);
volatile void* EliminationStackTop(EliminationStack* stack);
volatile void* EliminationStackPop(EliminationStack* stack);
void EliminationStackPush(EliminationStack* stack, volatile void* value);

EliminationStackLegacy* EliminationStackLegacyAllocate(SynchPoolStruct pool_elim);
int EliminationStackLegacyEmpty(EliminationStackLegacy* stack);
volatile void* EliminationStackLegacyTop(EliminationStackLegacy* stack);
volatile void* EliminationStackLegacyPop(EliminationStackLegacy** stack);
void EliminationStackLegacyPush(EliminationStackLegacy** stack, volatile void* value, SynchPoolStruct pool_elim);



#include <ccsynch_CC.h>
#include <stdlib.h>
#define ENQUEUE_SUCCESS 0

#define DEQUEUE_OP INT32_MIN


void Eliminate(volatile void* node_a, volatile void* node_b) {
    CCSynchNode* node0 = (CCSynchNode*)node_a;
    CCSynchNode* node1 = (CCSynchNode*)node_b;

    if (node0->arg_ret != (ArgVal)DEQUEUE_OP) {
        node1->arg_ret = node0->arg_ret;
        synchNonTSOFence();
        node1->completed = true;
        synchNonTSOFence();
        node1->locked = false;

        node0->arg_ret = ENQUEUE_SUCCESS;
        synchNonTSOFence();
        node0->completed = true;
        synchNonTSOFence();
        node0->locked = false;

        synchNonTSOFence();
    } else {
        node0->arg_ret = node1->arg_ret;
        synchNonTSOFence();
        node0->completed = true;
        synchNonTSOFence();
        node0->locked = false;

        node1->arg_ret = ENQUEUE_SUCCESS;
        synchNonTSOFence();
        node1->completed = true;
        synchNonTSOFence();
        node1->locked = false;

        synchNonTSOFence();
    }
}



EliminationStack* EliminationStackAllocate(int h) {
    EliminationStack* stack;
    
    stack = (EliminationStack*)malloc(sizeof(EliminationStack));

    stack->size = 0;
    stack->val = (volatile void**)malloc(h * sizeof(CCSynchElimStruct*));

    return stack;
}

int EliminationStackEmpty(EliminationStack* stack) {
    if (stack->size == 0) return 1;
    return 0;
}

volatile void* EliminationStackTop(EliminationStack* stack) {
    return stack->val[stack->size - 1];
}

volatile void* EliminationStackPop(EliminationStack* stack) {
    stack->size--;
    return stack->val[stack->size];
}

void EliminationStackPush(EliminationStack* stack, volatile void* value) {

    stack->val[stack->size] = value;
    stack->size++;
    
    return;
}



EliminationStackLegacy* EliminationStackLegacyAllocate(SynchPoolStruct pool_elim) {
    EliminationStackLegacy* stack;

    stack = synchAllocObj(&pool_elim);
    stack->sentinel = 1;
    stack->next = NULL;

    return stack;
}

int EliminationStackLegacyEmpty(EliminationStackLegacy* stack) {
    if (stack->sentinel == 1) return 1;
    return 0;
}

volatile void* EliminationStackLegacyTop(EliminationStackLegacy* stack) {
    return stack->val;
}

volatile void* EliminationStackLegacyPop(EliminationStackLegacy** stack) {
    EliminationStackLegacy* ret_node = *stack;

    *stack = ret_node->next;

    return ret_node->val; 
}

void EliminationStackLegacyPush(EliminationStackLegacy** stack, volatile void* value, SynchPoolStruct pool_elim) {
    EliminationStackLegacy* node;
    node = synchAllocObj(&pool_elim);
    node->next = *stack;
    node->sentinel = 0;
    node->val = value;
    *stack = node;
}


#endif