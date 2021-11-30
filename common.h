#ifndef _COMMON_H_
#define _COMMON_H_
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include <sys/time.h>
#include <cassert>
#include <assert.h>

#include <cmath>
#include <numeric>
#include <algorithm>

#include <list>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
//#include <pthread.h>

using namespace std;

#define NUM_PERSONS 15  // number of person to collect data for
#define MAX_MESSAGE 256  // maximum buffer size for each message

// different types of messages
enum REQUEST_TYPE_PREFIX {DATA_REQ_TYPE, FILE_REQ_TYPE, NEWCHAN_REQ_TYPE, QUIT_REQ_TYPE, UNKNOWN_REQ_TYPE};    
typedef __int64_t int64;

class Request{
protected:
    REQUEST_TYPE_PREFIX type;
public:
    Request (REQUEST_TYPE_PREFIX _rType): type(_rType){}
    REQUEST_TYPE_PREFIX getType (){
        return type;
    }
};


// message requesting a data point
class DataRequest: public Request{
public:
    int person;
    double seconds;
    int ecgno;
    DataRequest (int _person, double _seconds, int _eno):Request (DATA_REQ_TYPE){ // constructor
        person = _person, seconds = _seconds, ecgno = _eno;
    }
};

// message requesting a file
class FileRequest:public Request{
public:
    int64 offset;
    int length;
    FileRequest (int64 _offset, int _length): Request (FILE_REQ_TYPE){
        offset = _offset, length = _length;
    }
    char* getFileName (){
        return (char*)this + sizeof (FileRequest);
    }
};

void EXITONERROR(string msg);
vector<string> split (string line, char separator);
//int64 get_file_size (string filename);

bool isValidResponse(void* r);

#endif