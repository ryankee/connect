#include <iostream>
#include <libusb/libusb.h>
#include <vector>
#include <map>
#include <boost/thread.hpp>

#include "dataserver.hpp"
#include "cee/cee.hpp"
#include "bootloader/bootloader.hpp"

using namespace std;

map <libusb_device *, device_ptr> active_libusb_devices;

boost::thread* usb_thread;

#define NONOLITH_VID 0x59e3
#define CEE_PID 0xCEE1
#define BOOTLOADER_PID 0xBBBB

extern "C" void LIBUSB_CALL device_added_usbthread(libusb_device *dev, void *user_data);
extern "C" void LIBUSB_CALL device_removed_usbthread(libusb_device *dev, void *user_data);

void usb_init(){
	int r = libusb_init(NULL);
	if (r < 0){
		cerr << "Could not init libusb" << endl;
		abort();
	}
	
	libusb_register_hotplug_listeners(NULL, device_added_usbthread, device_removed_usbthread, 0);

	usb_thread = new boost::thread(usb_thread_main);
}

// Dedicated thread for handling soft-real-time USB events
void usb_thread_main(){
	while(1) libusb_handle_events(NULL);
}

void usb_fini(){
	// TODO: kill the USB thread somehow
	usb_thread->join(); //currently blocks forever

	devices.empty();
	
	libusb_exit(NULL);
}

void deviceAdded(libusb_device *dev){
	libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(dev, &desc);
	if (r<0){
		cerr << "Error in get_device_descriptor" << endl;
		libusb_unref_device(dev);
		return;
	}
	
	device_ptr newDevice;
	
	if (desc.idVendor == NONOLITH_VID){
		if (desc.idProduct == CEE_PID){
			newDevice = device_ptr(new CEE_device(dev, desc));
		}else if (desc.idProduct == BOOTLOADER_PID){
			newDevice = device_ptr(new Bootloader_device(dev, desc));
		}
	}
	
	if (newDevice){
		devices.insert(newDevice);
		active_libusb_devices.insert(pair<libusb_device *, device_ptr>(dev, newDevice));
		device_list_changed.notify();
	}
	
	libusb_unref_device(dev);
}

void deviceRemoved(libusb_device *dev){
	map <libusb_device *, device_ptr>::iterator it = active_libusb_devices.find(dev);
	if (it == active_libusb_devices.end()) return;
	cerr << "Device removed" <<std::endl;
	device_ptr ds_dev = it->second;
	devices.erase(ds_dev);
	active_libusb_devices.erase(it);
	device_list_changed.notify();
	ds_dev->onDisconnect();
}

extern "C" void LIBUSB_CALL device_added_usbthread(libusb_device *dev, void *user_data){
	libusb_ref_device(dev);
	io.post(boost::bind(deviceAdded, dev));
}

extern "C" void LIBUSB_CALL device_removed_usbthread(libusb_device *dev, void *user_data){
	io.post(boost::bind(deviceRemoved, dev));
}

void usb_scan_devices(){
	libusb_device **devs;
	
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		cerr << "Error in get_device_list" << endl;
	}

	for (ssize_t i=0; i<cnt; i++){
		libusb_ref_device(devs[i]);
		deviceAdded(devs[i]);
	}

	libusb_free_device_list(devs, 1);
}

bool USB_device::processMessage(ClientConn& client, string& cmd, JSONNode& n){
	if (cmd == "controlTransfer"){
		string id = n.at("id").as_string();
		uint8_t bmRequestType = n.at("bmRequestType").as_int();
		uint8_t bRequest = n.at("bRequest").as_int();
		uint16_t wValue = n.at("wValue").as_int();
		uint16_t wIndex = n.at("wIndex").as_int();
		uint16_t wLength = n.at("wLength").as_int();
	
		uint8_t data[wLength];
	
		bool isIn = bmRequestType & 0x80;
	
		if (!isIn){
			//TODO: also handle array input
			string datastr = n.at("data").as_string();
			datastr.copy((char*)data, wLength);
		}
	
		int ret = controlTransfer(bmRequestType, bRequest, wValue, wIndex, data, wLength);
	
		JSONNode reply(JSON_NODE);
		reply.push_back(JSONNode("_action", "controlTransferReturn"));
		reply.push_back(JSONNode("status", ret));
		reply.push_back(JSONNode("id", id));
	
		if (isIn && ret>=0){
			JSONNode data_arr(JSON_ARRAY);
			for (int i=0; i<ret && i<wLength; i++){
				data_arr.push_back(JSONNode("", data[i]));
			}
			data_arr.set_name("data");
			reply.push_back(data_arr);
		}
	
		client.sendJSON(reply);
	}else if(cmd == "enterBootloader"){
		controlTransfer(0x40|0x80, 0xBB, 0, 0, NULL, 0);
	}else{
		return false;
	}
	return true;
}


