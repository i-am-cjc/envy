#include <stdio.h>
#include "erow.h"

#define MAX_STACK_SIZE 255

erow stack[MAX_STACK_SIZE];

int top = -1;

int stackEmpty() {
	return top == -1;
}

int stackFull() {
	return top == MAX_STACK_SIZE;
}

erow stackPeek() {
	return stack[top];
}

erow stackPop() {
	return stack[top--];
}

void stackPush(erow *row) {
	erow paste;
    memcpy(&row, &paste, sizeof row);
    stack[++top] = paste;
}

