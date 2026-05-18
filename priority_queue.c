#include <stdio.h>
#include <stdlib.h>

#define MAX_SIZE 100

typedef struct {
    int jobId;
    int priority; // 1 (Highest) to 100 (Lowest)
} Job;

typedef struct {
    Job heap[MAX_SIZE];
    int size;
} PriorityQueue;

// Swap two jobs in the heap
void swap(Job *a, Job *b) {
    Job temp = *a;
    *a = *b;
    *b = temp;
}

