#include "common.h"
#include "TCPRequestChannel.h"
#include "BoundedBuffer.h"
#include "HistogramCollection.h"
#include <sys/wait.h>
#include <thread>
using namespace std;

void patient_thread_function(int n, int p, BoundedBuffer* req_buf){
    /* What will the patient threads do? */
	DataRequest d(p, 0.0, 1); // implement for ecg one first
	for (int i = 0; i < n; i++) {
		char* c = (char*)&d;
		vector<char> data(c, c+sizeof(DataRequest));
		req_buf->push(data);
		d.seconds += 0.004;
	}
}

void worker_thread_function(TCPRequestChannel* chan, BoundedBuffer* req_buf, BoundedBuffer* res_buf, int buf_cap, mutex* m){
    /*
		Functionality of the worker threads	
    */
   vector<char> data_buf;
   double res = 0;
   while(true) {
	   data_buf = req_buf->pop();
	   Request* r = (Request*)data_buf.data();
	   chan->cwrite(r, data_buf.size());
	
	   if (r->getType() == DATA_REQ_TYPE) {
		    DataRequest* dr = (DataRequest*)data_buf.data();
			chan->cread(&res, sizeof(double));
			pair<int, double>* hist_data = new pair<int,double>(dr->person, res);
			char* c = (char*)hist_data;
			vector<char> data(c, c+sizeof(pair<int, double>));
			res_buf->push(data);
	   }

	   else if (r->getType() == QUIT_REQ_TYPE) {
			delete chan;
			break;
	   }

	   else if (r->getType() == FILE_REQ_TYPE) {
			FileRequest* fr = (FileRequest*) r;
			string filename = fr->getFileName();
			int len = sizeof (FileRequest) + filename.size()+1;
			char fileDataBuf[buf_cap];
			chan->cread(fileDataBuf, fr->length);
			m->lock();
			int fdperson = open(("./received/" + filename).c_str(), O_CREAT | O_WRONLY | O_DSYNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			lseek(fdperson, fr->offset, SEEK_SET);
			write(fdperson, fileDataBuf, fr->length);
			close(fdperson);
			m->unlock();
	   }
   }
}
void histogram_thread_function (BoundedBuffer* res_buf, HistogramCollection* hc, Semaphore * lock) {
	/*
		Functionality of the histogram threads	
	*/
	vector<char> data_buf;
	while (true) {
		data_buf = res_buf->pop();
		pair<int, double>* res = (pair<int, double>*) data_buf.data();
		if (res->first == -1) {
			break;
		}
		lock->P();
		hc->update(res->first,res->second);
		lock->V();
	}
}

void file_thread_function(string filename, BoundedBuffer* req_buf, TCPRequestChannel* chan, int buf_cap) {
	/*
		Use lseek by f.offset to write to the output file
	*/
	if (filename == "") {
		return;
	}
	FileRequest fm (0,0);
	int len = sizeof (FileRequest) + filename.size()+1;
	char buf2 [len];
	memcpy (buf2, &fm, sizeof (FileRequest));
	std::strcpy (buf2 + sizeof (FileRequest), filename.c_str());
	chan->cwrite (buf2, len);  
	int64 filelen;
	chan->cread (&filelen, sizeof(int64));
	if (isValidResponse(&filelen)){
		std::cout << "File length is: " << filelen << " bytes" << endl;
	}

	int count = ceil((double)filelen / buf_cap);
	for (int i = 0; i < count; i++) {
		double currentPos = i * buf_cap;
		fm.length = buf_cap;
		fm.offset = i * buf_cap;
		if(filelen - currentPos < buf_cap) {
			fm.length = filelen - currentPos;
		}
		memcpy (buf2, &fm, sizeof (FileRequest));
		std::strcpy (buf2 + sizeof (FileRequest), filename.c_str());
		vector<char> req(buf2, buf2+len);
		req_buf->push(req);
	}
}

int main(int argc, char *argv[]){
	
	int opt;
	int h = 100; //?
	int n = 1000;
	int p = 1;
	string host = "";
	string r = "";
	int w = 100;
	int64 buffercapacity = MAX_MESSAGE;	
	Semaphore hist_lock(1);
	string filename = "";
	bool transfer = false;
	int b = 10; // size of bounded buffer, note: this is different from another variable buffercapacity/m
	// take all the arguments first because some of these may go to the server
	mutex m;
	while ((opt = getopt(argc, argv, "f:n:p:h:w:m:b:r:")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				transfer = true;
				break;
			case 'n':
				n = stoi(optarg);
				break;
			case 'p':
				p = stoi(optarg);
				break;
			case 'h':
				host = optarg;
				break;
			case 'r':
				r = optarg;
				break;
			case 'w':
				w = stoi(optarg);
				break;
			case 'm':
				buffercapacity = stoi(optarg);
				break;
			case 'b':
				b = stoi(optarg);
				break;
		}
	}
	if(host == "" || r == ""){
		cout << "Usage: at least ./client <server name/IP> <port no>" << endl;
        exit (-1);
	}
	/*
	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char* args[] = {"./server","-m", (char*) to_string(buffercapacity).c_str(), nullptr};
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}
	*/
	TCPRequestChannel chan(host, r);
	BoundedBuffer request_buffer(b);
	BoundedBuffer response_buffer(b);
	HistogramCollection hc;

	vector<thread> patients;
	vector<thread> workers;
	vector<thread> histograms;

	vector<TCPRequestChannel*> worker_channels;
	for (int i = 0; i < w; i++) {
		TCPRequestChannel* chanNew = new TCPRequestChannel(host, r);
		worker_channels.push_back(chanNew);
	}
	
	// initialize histograms 
	for (int i = 0; i < p; i ++) {
		Histogram* h = new Histogram(10, -3.0, 3.0);
		hc.add(h);
	}

	struct timeval start, end;
    gettimeofday (&start, 0);

    /* Start all threads here */
	if (!transfer) {
		for (int i = 0; i < p; i++) {
			patients.push_back(thread(patient_thread_function, n, i+1, &request_buffer));
		}
	}
	
	thread filethread (file_thread_function,filename, &request_buffer, &chan, buffercapacity);
	
	for (int i = 0; i < w; i++) {
		workers.push_back(thread(worker_thread_function, worker_channels[i], &request_buffer, &response_buffer, buffercapacity, &m));
	}
	
	if(!transfer) {
		for (int i = 0; i < h; i++) {
			histograms.push_back(thread(histogram_thread_function, &response_buffer, &hc, &hist_lock));
		}
	}

	/* Join all threads here */
	if (!transfer) {
		for (int i = 0; i < p; i++) {
			patients[i].join();
		}
	}
	filethread.join();
	
	for (int i = 0; i < w; i++) {
		Request q(QUIT_REQ_TYPE);
		char* c = (char*)&q;
		request_buffer.push(vector<char>(c,c+sizeof(Request)));
	}

	for (int i = 0; i < w; i++) {
		workers[i].join();
	}

	if (!transfer) {
		for (int i = 0; i < h; i++) {
			pair<int, double> quit_req(-1, -1.0);
			char* c = (char*)&quit_req;
			response_buffer.push(vector<char>(c,c+sizeof(pair<int, double>)));
		}
	}

	if (!transfer) {
		for (int i = 0; i < h; i++) {
			histograms[i].join();
		}
	}

    gettimeofday (&end, 0);

    // print the results and time difference
	if(filename == "") {
		hc.print ();
	}
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
	
	// closing the channel    
    Request q (QUIT_REQ_TYPE);
    chan.cwrite (&q, sizeof (Request));
	
	wait(0);
	cout << "Client process exited" << endl;

}
