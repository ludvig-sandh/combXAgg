/// @file queue-stack.h
/// @brief This file provides some very simple definitions used by all the provided
/// stack and queue implementations. More specifically, the Node struct is defined 
/// for all stack and queues that are represented with a linked-list of nodes.
/// Moreover, this file provides error-codes definitions for operations on stacks and queues
/// (i.e., error codes for failing to dequeue an element from a queue, error code for failing
/// to pop an element from a stack, etc.).
#ifndef _QUEUE_STACK_H_
#define _QUEUE_STACK_H_

#include <limits.h>
#include <config_CC.h>

typedef enum NodeType {
    NORMAL,
    LEFT_SENT,
    RIGHT_SENT
} NodeType;

typedef struct Node {
    Object val;
    volatile struct Node *next;
} Node;

typedef struct DequeCASNode {
    Object val;
    NodeType type;
    volatile struct DequeCASNode *right;
    volatile struct DequeCASNode *left;
} DequeCASNode;

typedef struct DequeWINNode {
    Object val;
    int leftBit;
    int rightBit;
    int winner;
    volatile struct DequeWINNode *left;
    volatile struct DequeWINNode *right;
} DequeWINNode;

#define GUARD_VALUE     LONG_MIN
#define EMPTY_QUEUE     (GUARD_VALUE + 1)
#define EMPTY_STACK     (GUARD_VALUE + 1)
#define ENQUEUE_SUCCESS 0
#define ENQUEUE_FAIL    -1
#define PUSH_SUCCESS    0
#define PUSH_FAIL       -1
#define LEFT            -1
#define NONE            0
#define RIGHT           1
#define FALSE           0
#define TRUE            1

#endif