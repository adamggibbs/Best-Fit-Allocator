#include <stdlib.h>
#include <stdio.h>

int main (int argc, char **argv){

  printf("\n%s\n\n", "allocate 3 blocks of size 24, 19, 32 to x,y,z respectively.");
  char* x = malloc(24);
  char* y = malloc(19);
  char* z = malloc(32);
  
  printf("x = %p\n", x);
  printf("y = %p\n", y);
  printf("z = %p\n", z);

  printf("\n%s\n\n", "reallocate x to blocks of size 20 and 30 and store in a,b respectively.\n a should result in same block as x and b should allocate a new block.");
  char* a = realloc(x, 20);
  char* b = realloc(x, 30);
  printf("a = %p\n", a);
  printf("b = %p\n", b);

  printf("\n%s\n\n", "After the realloc(x, 30) call, the original block of size 24 is freed and its content are reallocated to a block of size 30.\n This should put the block of size 24 on the free block LL.\n Allocate a block of size 19 (less than 24) using malloc and store it in c.\n c should have the same address that x originally had.");
  char* c = malloc(19);
  printf("c = %p\n", c);

  printf("\n%s\n\n","free blocks c,y,z to push blocks of sizes 24,19,32 onto the free block list");
  free(c);
  free(y);
  free(z);

  printf("\n%s\n\n", "Allocate a block of size 23 using malloc().\n It should take the block previously pointed to by c off the free list and allocate it to d.");
  char* d = malloc(23);
  printf("d = %p\n", d);

  printf("\n%s\n\n", "Allocate a block of size 22 using malloc().\n  The block of size 24 should no longer be on the free block list.\n  So the block of size 32 previously pointed to by z should be allocated.");
  char* e = malloc(22);
  printf("e = %p\n", e);
  
}
