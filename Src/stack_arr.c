/*
 * stack_arr.c
 *
 *  Created on: Aug 26, 2023
 *      Author: krudenko
 */
#include "stack_arr.h"

bool StackArr_push(stackArr *mystack, int value)
{
	if (mystack->top >= STACK_LENGTH - 1)
	{
		return false;
	}

	mystack->top++;
	mystack->values [mystack->top] = value;
	return true;
}

int StackArr_pop(stackArr *mystack)
{
	if (mystack->top == EMPTY)
	{
		return STACK_EMPTY;
	}

	int result = mystack->values [mystack->top];
	mystack->top--;
	return result;
}


void StackArr_Driver(void)
{
		/* Stack - array-based implementation START */
		stackArr s1, s2, s3;
		s1.top = EMPTY;
		s2.top = EMPTY;
		s3.top = EMPTY;
		StackArr_push(&s1, 56);
		StackArr_push(&s2, 78);
		StackArr_push(&s2, 23);
		StackArr_push(&s2, 988);
		StackArr_push(&s3, 13);

		int t;
		while ((t = StackArr_pop(&s2)) != STACK_EMPTY)
		{
			printf("t = %d\n", t);
		}
		/* Stack - array-based implementation END */
}

