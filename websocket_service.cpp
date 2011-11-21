#include <iostream>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"
#include "websocket_handler.hpp"

#include "dataserver.hpp"
#include "json.hpp"

struct StreamWatch{
	StreamWatch(InputStream *s, unsigned si, unsigned ei, unsigned df):
		stream(s),
		startIndex(si),
		endIndex(ei),
		decimateFactor(df),
		index(si),
		outIndex(0){	
	}
	InputStream *stream;

	// stream sample indexes
	unsigned startIndex;
	unsigned endIndex;
	unsigned decimateFactor;
	unsigned index;

	// resampled indexes
	unsigned outIndex;
	

	EventListener data_received_l;
};

typedef std::pair<InputStream* const, StreamWatch*> watch_pair;

class ClientConn{
	public:
	ClientConn(websocketpp::session_ptr c): client(c){
		std::cout << "Opened" <<std::endl;
		l_device_list_changed.subscribe(
			device_list_changed,
			boost::bind(&ClientConn::on_device_list_changed, this)
		);
		l_streaming_state_changed.subscribe(
			streaming_state_changed,
			boost::bind(&ClientConn::on_streaming_state_changed, this)
		);

		on_device_list_changed();
		on_streaming_state_changed();
	}

	~ClientConn(){
		BOOST_FOREACH(watch_pair &p, watches){
			delete p.second;
		}
	}

	void watch(InputStream *stream, unsigned startIndex, unsigned endIndex, unsigned decimateFactor){
		StreamWatch *w = new StreamWatch(stream, startIndex, endIndex, decimateFactor);
		watches.insert(watch_pair(stream, w));
		w->data_received_l.subscribe(
			stream->data_received,
			boost::bind(&ClientConn::on_data_received, this, w)
		);
		on_data_received(w);
	}

	websocketpp::session_ptr client;

	EventListener l_device_list_changed;
	EventListener l_streaming_state_changed;
	std::map<InputStream*, StreamWatch*> watches;

	void on_message(const std::string &msg){
		std::cout << "Recd:" << msg <<std::endl;

		try{
			JSONNode n = libjson::parse(msg);
			string cmd = n.at("_cmd").as_string();
			if (cmd == "watch"){
				string device = n.at("device").as_string();
				string channel = n.at("channel").as_string();
				string streamName = n.at("stream").as_string();
				int startIndex = n.at("startIndex").as_int();
				int endIndex = n.at("endIndex").as_int();
				int decimateFactor = n.at("decimateFactor").as_int();

				InputStream* stream = findStream(device, channel, streamName);
				watch(stream, startIndex, endIndex, decimateFactor);
			}
		}catch(std::exception &e){ // TODO: more helpful error message
			std::cerr << "WS JSON error:" << e.what() << std::endl;
			return;
		}		
	}

	void on_message(const std::vector<unsigned char> &data){
		
	}

	void sendJSON(JSONNode &n){
		string jc = (string) n.write_formatted();
		client->send(jc);
	}

	void on_device_list_changed(){
		JSONNode n(JSON_NODE);
		JSONNode devices = jsonDevicesArray();
		devices.set_name("devices");

		n.push_back(JSONNode("_action", "devices"));
		n.push_back(devices);

		sendJSON(n);
	}

	void on_streaming_state_changed(){
		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "streamstate"));
		n.push_back(JSONNode("streaming", false));

		sendJSON(n);
	}

	void on_data_received(StreamWatch* w){
		JSONNode n(JSON_NODE);
		n.push_back(JSONNode("_action", "update"));
		n.push_back(JSONNode("streamId", w->stream->id));
		n.push_back(JSONNode("startIndex", w->index));
		JSONNode a(JSON_ARRAY);
		a.set_name("data");
		while (w->index < w->stream->buffer_fill_point && w->index < w->endIndex){
			a.push_back(JSONNode("", w->stream->data[w->index]));
			w->index += w->decimateFactor;
		}
		n.push_back(a);

		if (w->index >= w->endIndex){
			n.push_back(JSONNode("end", true));
			watches.erase(w->stream);
			delete w;
		}

		sendJSON(n);
	}
};

std::map<websocketpp::session_ptr, ClientConn*> connections;

void data_server_handler::on_open(websocketpp::session_ptr client){
	connections.insert(std::pair<websocketpp::session_ptr, ClientConn*>(client, new ClientConn(client)));
}
	
void data_server_handler::on_close(websocketpp::session_ptr client){
	std::map<websocketpp::session_ptr, ClientConn*>::iterator it = connections.find(client);
	delete it->second;
	connections.erase(it);
}

void data_server_handler::on_message(websocketpp::session_ptr client,const std::string &msg){
	connections.find(client)->second->on_message(msg);
}

void data_server_handler::on_message(websocketpp::session_ptr client, const std::vector<unsigned char> &data) {
	connections.find(client)->second->on_message(data);
}