
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
  pthread_t t1,t2;
  int value1 = 1;
  int value2 = 2;

  void *retrval1;
  void *retrval2; 
  
  pthread_create(&t1,NULL,printing,(void *)&value1);
  pthread_join(t1, &retrval1);
  int *return_value = (int *)retrval1; 
  printf("the value returned by the thread is %d\n",*return_value); 

  pthread_create(&t2,NULL,printing,(void*)return_value);
  pthread_join(t2,&retrval1);

  return_value = (int *)retrval1;
 
  printf("the value returned by the thread  is %d\n",*return_value);
  return 0;
}