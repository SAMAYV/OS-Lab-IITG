#include <stdio.h>
int main(int argc, char **argv)
{
	int x = 1;
	printf("Hello x = %d\n", x);
	asm("incl %0": "+r"(x));
	/*
	First method to increment the value of x using inline assembly language having one line
	asm("incl %0": "+r"(x));
	1) incl command adds 1 to the 32-bit contents of the variable specified
	2) %0 refers to the first variable passed (which in this case is x)
	3) the ‘+’ sign before r denotes that x acts as both input and output
	*/
	/*
	Second method to increment the value of x using inline assembly language having
	multiple lines (uncomment to use this)
	asm(“mov %1, %0 \n\t”
	“add $1, %0”
	: “=r” (x)
	: “r” (x));
	*/
	printf("Hello x = %d after increment\n", x);
	if(x == 2)
	{
		printf("OK\n");
	}
	else
	{
		printf("ERROR\n");
	}
}
