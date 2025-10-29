#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

void *printing(void *value)
{
  int *casted_value = (int*)value; 
  *casted_value += 10;
  return (void*)casted_value;
}


int main()
{
  pthread_t t;
  int value = 8;
  void * retrval;
  pthread_create(&t,NULL,printing,(void *)&value);
  pthread_join(t, &retrval);
  int *return_value = (int *)retrval;
  printf("the value returned by the thread is %d \n",*return_value);
  return 0;
}