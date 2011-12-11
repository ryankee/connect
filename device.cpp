#include <boost/foreach.hpp>
#include "dataserver.hpp"
#include <iostream>


void Device::prepare_capture(float seconds, bool continuous){
	if (captureState == CAPTURE_ACTIVE){
		pause_capture();
	}

	std::cerr << "prepare capture" <<std::endl;
	captureState = CAPTURE_READY;
	captureLength = seconds;
	captureContinuous = continuous;
	on_prepare_capture();
	captureStateChanged.notify();
}

void Device::start_capture(){
	if (captureState == CAPTURE_PAUSED || captureState == CAPTURE_READY){
		captureState = CAPTURE_ACTIVE;
		std::cerr << "start capture" <<std::endl;
		on_start_capture();
		captureStateChanged.notify();
	}
}

void Device::pause_capture(){
	if (captureState == CAPTURE_ACTIVE){
		captureState = CAPTURE_PAUSED;
		std::cerr << "pause capture" <<std::endl;
		on_pause_capture();
		captureStateChanged.notify();
	}
}

void Device::done_capture(){
	if (captureState == CAPTURE_ACTIVE){
		captureState = CAPTURE_DONE;
		std::cerr << "done capture" <<std::endl;
		on_pause_capture();
		captureStateChanged.notify();
	}
}

void Device::setOutput(Channel* channel, OutputSource* source){
	if (channel->source){
		delete channel->source;
	}
	channel->source=source;
}

device_ptr getDeviceById(string id){
	BOOST_FOREACH(device_ptr d, devices){
		if (d->getId() == id) return d;
	}
	return device_ptr();
}

Channel* Device::channelById(const string& id){
	BOOST_FOREACH(Channel* c, channels){
		if (c->id == id) return c;
	}
	return 0;
}

Stream* Channel::streamById(const string& id){
	BOOST_FOREACH(Stream *i, streams){
		if (i->id == id) return i;
	}
	return 0;
}

struct ErrorStringException : public std::exception{
   string s;
   ErrorStringException (string ss) throw() : s(ss) {}
   virtual const char* what() const throw() { return s.c_str(); }
   virtual ~ErrorStringException() throw() {}
};

Stream* findStream(const string& deviceId, const string& channelId, const string& streamId){
	device_ptr d = getDeviceById(deviceId);
	if (!d){
		throw ErrorStringException("Device not found");
	}
	Channel* c = d->channelById(channelId);
	if (!c){
		throw ErrorStringException("Channel not found");
	}
	Stream *s = c->streamById(streamId);
	if (!s){
		throw ErrorStringException("Stream not found");
	}
	return s;
}

void Stream::allocate(unsigned size, bool cont){
	buffer_size = size;
	buffer_i = 0;
	continuous = cont;
	if (data){
		free(data);
	}
	data = (float *) malloc(buffer_size*sizeof(float));

	if (!data) buffer_size=0;
}

string captureStateToString(CaptureState s){
	switch (s){
		case CAPTURE_INACTIVE:
			return "inactive";
		case CAPTURE_READY:
			return "ready";
		case CAPTURE_ACTIVE:
			return "active";
		case CAPTURE_PAUSED:
			return "paused";
		case CAPTURE_DONE:
			return "done";
		default:
			return "";
	}
}
