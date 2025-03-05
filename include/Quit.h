#ifndef QUIT_H
#define QUIT_H
#include <csetjmp>


void Quit();  // Declaration of Quit function
static jmp_buf jumpBuffer;
void raise_c(int eno);
#endif // QUIT_H