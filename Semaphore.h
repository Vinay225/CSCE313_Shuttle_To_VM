#include <iostream>
#include <stdio.h>
#include <queue>
#include <string>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <condition_variable>
#include <mutex>
using namespace std;

class Semaphore{
private:
  int value;
  mutex m;
  condition_variable cv;
public:
  Semaphore (int _v):value(_v){
    
  }
  void P(){
    unique_lock<mutex> l (m);
    // wait until the value is positive
    cv.wait (l, [this]{return value > 0;});
    value --;
  }
  void V(){
    unique_lock<mutex> l (m);
    value ++;
    cv.notify_one();
  }
};
