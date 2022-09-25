#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "misc/cpp/imgui_stdlib.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <crow.h>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <deque>
#include <iostream>
#include <json/json.h>
#include <list>
#include <map>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

using namespace std;
using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
using ConnectionHdl = websocketpp::connection_hdl;
using SslContext = websocketpp::lib::asio::ssl::context;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
	!defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

Client client, client_priv;
ConnectionHdl connection, connection_priv;
atomic_bool done = false, current_order_filled = true;
map<string, string> pric;
map<string, bool> tp, tac, show;
map<string, atomic_bool> data_ok;
vector<string> acts = {
	"Swing buy using same quantity",
	"Swing sell using same quantity",
	"Buy using specified quantity",
	"Sell using specified quantity",
	"Close all positions",
	"Buy using specified quantity, reduce only",
	"Sell using specified quantity, reduce only",
	"Buy using specified leverage times total available balance",
	"Sell using specified leverage times total available balance"};
int ac_current = 4, th_priv;
vector<bool> ac_needparam = {false, false, true, true, false,
							 true,	true,  true, true};
bool rstat = false, use_heikin_ashi = false, connected_byb = false,
	 webhook_use_ssl = false, webhook_listening = false;
int use_endpoint = 2, use_webhook = 0;
string bybit_key = "", bybit_secret = "",
	   bybit_endpoint = "https://api-testnet.bybit.com",
	   wss_endpoint = "wss://stream-testnet.bybit.com", chk_imap = "";
typedef vector<float> dataset;
string pairn, tsignal;
int webhook_port = 80;
struct kandle {
	float o, h, l, c;
	kandle &operator=(const kandle &k2) {
		o = k2.o;
		h = k2.h;
		l = k2.l;
		c = k2.c;
		return *this;
	}
	kandle operator+(const kandle &k2) const {
		kandle ret{o, std::max(h, k2.h), std::min(l, k2.l), k2.c};
		return ret;
	}
	kandle &operator+=(const kandle &k2) {
		*this = *this + k2;
		return *this;
	}
};
struct tradeitem {
	string typ = "", symbol = "";
	float param = 0.0;
	bool reduceonly = false;
	tradeitem operator=(const tradeitem t2) {
		typ = t2.typ;
		symbol = t2.symbol;
		param = t2.param;
		reduceonly = t2.reduceonly;
		return *this;
	}
};

tradeitem ttmp;
vector<thread> threads;
map<string, tradeitem> td;
map<string, deque<kandle>> kq;
map<string, map<bool, dataset>> macache;
unordered_set<int> uidset;
unordered_set<string> tv_ip;
string chk_res;
bool ws_ready = false;

struct ExampleAppLog {
	ImGuiTextBuffer Buf;
	bool ScrollToBottom;

	void Clear() { Buf.clear(); }

	void AddLog(const char *str) {
		Buf.append(str);
		ScrollToBottom = true;
	}

	void Draw(const char *title, bool *p_opened = NULL) {
		ImGui::Begin(title, p_opened);
		ImGui::TextUnformatted(Buf.begin());
		if (ScrollToBottom)
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;
		ImGui::End();
	}
};
ExampleAppLog my_log;

map<string, ImVec4> cols;
auto endt = chrono::system_clock::now().time_since_epoch() / chrono::seconds(1);
const vector<string> kival{"1",	  "3",	 "5",	"15", "30", "60", "120",
						   "240", "360", "720", "D",  "W",	"M"},
	kiusr{"1 minute", "3 minutes", "5 minutes", "15 minutes", "30 minutes",
		  "1 hour",	  "2 hours",   "4 hours",	"6 hours",	  "12 hours",
		  "1 day",	  "1 week",	   "1 month"};
const vector<uint64_t> msval{
	60000ULL,			60000ULL * 3ULL,   60000ULL * 5ULL,
	60000ULL * 15ULL,	60000ULL * 30ULL,  60000ULL * 60ULL,
	60000ULL * 120ULL,	60000ULL * 240ULL, 60000ULL * 360ULL,
	60000ULL * 720ULL,	86400000ULL,	   86400000ULL * 7ULL,
	86400000ULL * 30ULL};
int kinterval = 7;

ImVec4 hexvec(string hex) {
	int r, g, b;
	sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b);
	return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

struct InputTextCallback_UserData {
	std::string *Str;
	ImGuiInputTextCallback ChainCallback;
	void *ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData *data) {
	InputTextCallback_UserData *user_data =
		(InputTextCallback_UserData *)data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		std::string *str = user_data->Str;
		IM_ASSERT(data->Buf == str->c_str());
		str->resize(data->BufTextLen);
		data->Buf = (char *)str->c_str();
	} else if (user_data->ChainCallback) {
		data->UserData = user_data->ChainCallbackUserData;
		return user_data->ChainCallback(data);
	}
	return 0;
}

bool ImGui::InputText(const char *label, std::string *str,
					  ImGuiInputTextFlags flags,
					  ImGuiInputTextCallback callback, void *user_data) {
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data;
	cb_user_data.Str = str;
	cb_user_data.ChainCallback = callback;
	cb_user_data.ChainCallbackUserData = user_data;
	return InputText(label, (char *)str->c_str(), str->capacity() + 1, flags,
					 InputTextCallback, &cb_user_data);
}

bool ImGui::InputTextWithHint(const char *label, const char *hint,
							  std::string *str, ImGuiInputTextFlags flags,
							  ImGuiInputTextCallback callback,
							  void *user_data) {
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data;
	cb_user_data.Str = str;
	cb_user_data.ChainCallback = callback;
	cb_user_data.ChainCallbackUserData = user_data;
	return InputTextWithHint(label, hint, (char *)str->c_str(),
							 str->capacity() + 1, flags, InputTextCallback,
							 &cb_user_data);
}

string get_signature(string input) {
	array<unsigned char, EVP_MAX_MD_SIZE> hash;
	unsigned int hashLen;
	HMAC(EVP_sha256(), bybit_secret.c_str(), static_cast<int>(36),
		 reinterpret_cast<unsigned char const *>(input.c_str()),
		 static_cast<int>(input.size()), hash.data(), &hashLen);
	char buf[1024] = {0};
	char tmp[3] = {0};
	for (int i = 0; i < 32; i++) {
		sprintf(tmp, "%02x", hash[i]);
		strcat(buf, tmp);
	}
	return string(buf);
}

dataset srclose() {
	dataset ret;
	for (const kandle &i : kq[pairn]) {
		ret.push_back(i.c);
	}
	return ret;
}

dataset sropen() {
	dataset ret;
	for (const kandle &i : kq[pairn]) {
		ret.push_back(i.o);
	}
	return ret;
}

dataset srhigh() {
	dataset ret;
	for (const kandle &i : kq[pairn]) {
		ret.push_back(i.h);
	}
	return ret;
}

dataset srlow() {
	dataset ret;
	for (const kandle &i : kq[pairn]) {
		ret.push_back(i.l);
	}
	return ret;
}

dataset ohlc4() {
	dataset ret;
	for (const kandle &i : kq[pairn]) {
		ret.push_back((i.o + i.h + i.l + i.c) / 4.0f);
	}
	return ret;
}

dataset haopen() {
	dataset ret, haclose = ohlc4();
	ret.push_back((kq[pairn].begin()->o + kq[pairn].begin()->c) / 2.0f);
	for (size_t i = 1; i < kq[pairn].size(); ++i)
		ret.push_back((ret[i - 1] + haclose[i - 1]) / 2.0f);
	return ret;
}

dataset maclose(dataset ds, size_t len) {
	dataset ret;
	float sum = 0.0;
	for (size_t i = 0; i < len; ++i)
		sum += ds[i];
	for (size_t i = 1; i < len; ++i)
		ret.push_back(0.0);
	ret.push_back(sum / (float)len);
	for (size_t i = len; i < ds.size(); ++i) {
		sum = sum + ds[i] - ds[i - len];
		ret.push_back(sum / (float)len);
	}
	return ret;
}

static void glfw_error_callback(int error, const char *description) {
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void send_message(Client *client, ConnectionHdl *connection, std::string msg) {
	client->send(*connection, msg, websocketpp::frame::opcode::text);
}

void close_connection(Client *client, ConnectionHdl *connection) {
	client->close(*connection, websocketpp::close::status::normal, "done");
}

void on_message(Client *client, ConnectionHdl hdl,
				websocketpp::config::asio_client::message_type::ptr msg) {
	if (done)
		return;
	Json::Reader reader;
	Json::Value output;
	reader.parse(msg->get_payload(), output);
	const string prefixk = "kline.", prefixo = "user.order.unifiedAccount";

	if (output.isMember("topic") &&
		equal(prefixk.begin(), prefixk.end(),
			  output["topic"].asString().begin()) &&
		output.isMember("data")) {

		string symbol = output["topic"].asString();
		symbol = symbol.substr(symbol.find(".") + 1);
		if (!equal(kival[kinterval].begin(), kival[kinterval].end(),
				   symbol.begin()))
			return;
		symbol = symbol.substr(symbol.find(".") + 1);
		kq[symbol].rbegin()->h = stof(output["data"][0]["high"].asString());
		kq[symbol].rbegin()->l = stof(output["data"][0]["low"].asString());
		pric[symbol] = output["data"][0]["close"].asString();
		kq[symbol].rbegin()->c = stof(pric[symbol]);
		if (output["data"][0]["confirm"].asBool()) {
			const float initc = stof(output["data"][0]["close"].asString());
			kandle tk;
			tk.o = initc;
			tk.h = initc;
			tk.l = initc;
			tk.c = initc;
			kq[symbol].pop_front();
			kq[symbol].push_back(tk);
		}
		endt = output["data"][0]["end"].asUInt64() / 1000;
	} else if (output.isMember("topic") &&
			   output["topic"].asString() == prefixo) {
		if (output["data"]["result"][0]["orderStatus"].asString() == "Filled") {
			current_order_filled = true;
			my_log.AddLog("[INFO] Order filled\n");
		}
	}
}

void on_open(Client *client, ConnectionHdl *connection, ConnectionHdl hdl) {
	*connection = hdl;
}

websocketpp::lib::shared_ptr<SslContext> on_tls_init() {
	auto ctx = websocketpp::lib::make_shared<SslContext>(
		boost::asio::ssl::context::sslv23);
	return ctx;
}

void turn_off_logging(Client &client) {
	client.clear_access_channels(websocketpp::log::alevel::all);
	client.clear_error_channels(websocketpp::log::elevel::all);
}

void set_message_handler(Client &client) {
	client.set_message_handler(
		websocketpp::lib::bind(&on_message, &client, ::_1, ::_2));
}

void set_open_handler(Client &client, ConnectionHdl *connection) {
	client.set_open_handler(
		websocketpp::lib::bind(&on_open, &client, connection, ::_1));
}

void set_tls_init_handler(Client &client) {
	client.set_tls_init_handler(websocketpp::lib::bind(&on_tls_init));
}

void set_url(Client &client, string url) {
	websocketpp::lib::error_code ec;
	auto connection = client.get_connection(url, ec);
	client.connect(connection);
}

size_t ReceiveData(void *contents, size_t size, size_t nmemb, void *stream) {
	string *str = (string *)stream;
	(*str).append((char *)contents, size * nmemb);
	return size * nmemb;
}

void HttpGet(const string url, string &response) {
	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		return;
	}
	curl_easy_setopt(curl, CURLOPT_NOPROXY, "localhost");
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return;
}

string system_timestamp() {

	chrono::time_point<chrono::system_clock, chrono::milliseconds> tp =
		chrono::time_point_cast<chrono::milliseconds>(
			chrono::system_clock::now());
	auto tmp =
		chrono::duration_cast<chrono::milliseconds>(tp.time_since_epoch());
	return to_string(tmp.count());
}

string bybit_timestamp() {
	string response = "";
	HttpGet(bybit_endpoint + "/v2/public/time", response);
	Json::Reader reader;
	Json::Value output;
	reader.parse(response, output);
	const string time_now = output["time_now"].asString();
	size_t dot = time_now.find('.');
	return time_now.substr(0, dot) + time_now.substr(dot + 1, 3);
}

CURLcode HttpGet_withauth(const string url, const string payload,
						  string &response) {
	CURLcode res;
	CURL *curl = curl_easy_init();
	const string bybit_key_x = bybit_key.substr(0, 18);
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "X-BAPI-SIGN-TYPE: 2");
	const string btime = system_timestamp();
	headers = curl_slist_append(
		headers, ("X-BAPI-SIGN: " +
				  get_signature(btime + bybit_key_x + "10000" + payload))
					 .c_str());
	headers =
		curl_slist_append(headers, ("X-BAPI-API-KEY: " + bybit_key_x).c_str());
	headers =
		curl_slist_append(headers, ("X-BAPI-TIMESTAMP: " + btime).c_str());
	headers = curl_slist_append(headers, "X-BAPI-RECV-WINDOW: 10000");
	if (curl == NULL)
		return CURLE_FAILED_INIT;
	if (payload.empty())
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	else
		curl_easy_setopt(curl, CURLOPT_URL, (url + "?" + payload).c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	return res;
}

inline CURLcode HttpPost_withauth(const string &url, const Json::Value &data,
								  string &response) {
	Json::StreamWriterBuilder builder;
	const string bybit_key_x = bybit_key.substr(0, 18);
	const string payload = Json::writeString(builder, data);
	CURLcode res;
	CURL *curl = curl_easy_init();
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "X-BAPI-SIGN-TYPE: 2");
	const string btime = system_timestamp();
	headers = curl_slist_append(
		headers, ("X-BAPI-SIGN: " +
				  get_signature(btime + bybit_key_x + "5000" + payload))
					 .c_str());
	headers =
		curl_slist_append(headers, ("X-BAPI-API-KEY: " + bybit_key_x).c_str());
	headers =
		curl_slist_append(headers, ("X-BAPI-TIMESTAMP: " + btime).c_str());
	headers = curl_slist_append(headers, "X-BAPI-RECV-WINDOW: 5000");
	headers = curl_slist_append(headers, "Content-Type:application/json");
	if (curl == NULL) {
		return CURLE_FAILED_INIT;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	return res;
}

void keepalive() {
	Json::Value param;
	param["op"] = "ping";
	const string opp = param.toStyledString();
	string response = "";
	Json::Reader reader;
	Json::Value output;
	while (!done) {
		for (int i = 0; i < 200 && !done; ++i)
			this_thread::sleep_for(chrono::milliseconds(100));
		if (!done) {
			send_message(&client, &connection, opp);
			if (ws_ready)
				send_message(&client_priv, &connection_priv, opp);
		}
	}
	return;
}

void drawkandle(float o, float h, float l, float c, ImDrawList *draw_list,
				const float xsz, const float x, const float y, float th,
				const float khtot, const float kltot, const float sca) {
	ImU32 col;
	if (c > o)
		col = ImColor(cols["green"]);
	else
		col = ImColor(cols["red"]);
	draw_list->AddRectFilled(ImVec2(x, y + (khtot - o) * sca),
							 ImVec2(x + xsz - 1.0f, y + (khtot - c) * sca),
							 col);
	draw_list->AddLine(ImVec2(x + (xsz / 2.0) - 1.0f, y + (khtot - h) * sca),
					   ImVec2(x + (xsz / 2.0) - 1.0f, y + (khtot - l) * sca),
					   col, th);
	return;
}

struct label {
	ImVec2 loc;
	string pcon;
	ImColor col;
	ImVec4 fcol;
	label(ImVec2 a, string b, ImColor c, ImVec4 d) {
		loc = a;
		pcon = b;
		col = c;
		fcol = d;
	};
};
vector<label> vl;
vector<string> syms;
map<string, int> pricescale;
float xtot = 120.0f, ysz = 160.0f, spacing = 1.0f;
map<string, float> xsz;
string imap_endpoint = "", mailbox_login = "", mailbox_password = "";

void inslabel(ImVec2 loc, string pcon, ImColor col, ImVec4 fcol) {
	vl.push_back(label(loc, pcon, col, fcol));
}
void subs(string symbol, int kinterval) {
	while (!data_ok[symbol])
		this_thread::sleep_for(chrono::milliseconds(100));
	data_ok[symbol] = false;
	uint64_t ms = duration_cast<chrono::milliseconds>(
					  chrono::system_clock::now().time_since_epoch())
					  .count();

	string rs = "",
		   url = bybit_endpoint +
				 "/derivatives/v3/public/"
				 "kline?category=linear&symbol=" +
				 symbol + "&interval=" + kival[kinterval] +
				 "&start=" + to_string(ms - 200ULL * msval[kinterval]) +
				 "&end=" + to_string(ms);
	HttpGet(url, rs);
	Json::Reader reader;
	Json::Value output;
	reader.parse(rs, output);
	output = output["result"]["list"];
	kandle tk;
	kq[symbol].clear();
	for (int i = output.size() - 1; i >= 0; --i) {
		tk.o = stof(output[i][1].asString());
		tk.h = stof(output[i][2].asString());
		tk.l = stof(output[i][3].asString());
		tk.c = stof(output[i][4].asString());
		kq[symbol].push_back(tk);
	}
	Json::Value param;
	param["op"] = "subscribe";
	param["args"][0] = "kline." + kival[kinterval] + "." + symbol;
	send_message(&client, &connection, param.toStyledString());
	data_ok[symbol] = true;
}
void unsub(string symbol, int kinterval) {
	while (!data_ok[symbol])
		this_thread::sleep_for(chrono::milliseconds(100));
	data_ok[symbol] = false;
	kq[symbol].clear();
	Json::Value param;
	param["op"] = "unsubscribe";
	param["args"][0] = "kline." + kival[kinterval] + "." + symbol;
	send_message(&client, &connection, param.toStyledString());
	data_ok[symbol] = true;
}
void unsubc(string symbol, int okival, int kinterval) {
	Json::Value param;
	param["op"] = "unsubscribe";
	param["args"][0] = "kline." + kival[okival] + "." + symbol;
	send_message(&client, &connection, param.toStyledString());
	uint64_t ms = duration_cast<chrono::milliseconds>(
					  chrono::system_clock::now().time_since_epoch())
					  .count();
	string rs = "",
		   url = bybit_endpoint +
				 "/derivatives/v3/public/"
				 "kline?category=linear&symbol=" +
				 symbol + "&interval=" + kival[kinterval] +
				 "&start=" + to_string(ms - 200ULL * msval[kinterval]) +
				 "&end=" + to_string(ms);
	HttpGet(url, rs);
	Json::Reader reader;
	Json::Value output;
	reader.parse(rs, output);
	output = output["result"]["list"];
	kandle tk;
	while (!data_ok[symbol])
		this_thread::sleep_for(chrono::milliseconds(100));
	data_ok[symbol] = false;
	kq[symbol].clear();
	for (int i = output.size() - 1; i >= 0; --i) {
		tk.o = stof(output[i][1].asString());
		tk.h = stof(output[i][2].asString());
		tk.l = stof(output[i][3].asString());
		tk.c = stof(output[i][4].asString());
		kq[symbol].push_back(tk);
	}
	data_ok[symbol] = true;
	param["op"] = "subscribe";
	param["args"][0] = "kline." + kival[kinterval] + "." + symbol;
	send_message(&client, &connection, param.toStyledString());
}

string to2digi(uint64_t x) {
	return x < 10 ? "0" + to_string(x) : to_string(x);
}

string fmtperiod(uint64_t ut) {
	string ret = to2digi(ut % 60);
	ut /= 60;
	if (ut) {
		ret = to2digi(ut % 60) + ":" + ret;
		ut /= 60;
	}
	if (ut) {
		ret = to2digi(ut % 24) + ":" + ret;
		ut /= 24;
	}
	return ret;
}

bool cmp(const label &a, const label &b) { return a.loc.y < b.loc.y; }

dataset mn(const dataset a, const dataset b) {
	dataset ret;
	for (size_t i = 0; i < a.size(); ++i)
		ret.push_back(min(a[i], b[i]));
	return ret;
}

dataset mx(const dataset a, const dataset b) {
	dataset ret;
	for (size_t i = 0; i < a.size(); ++i)
		ret.push_back(max(a[i], b[i]));
	return ret;
}

void drawsymbol(string symbol, float xsz, bool &rstat) {
	while (!data_ok[symbol])
		this_thread::sleep_for(chrono::milliseconds(100));
	dataset c = use_heikin_ashi ? ohlc4() : srclose(),
			o = use_heikin_ashi ? haopen() : sropen(),
			h = use_heikin_ashi ? mx(mx(c, o), srhigh()) : srhigh(),
			l = use_heikin_ashi ? mn(mn(c, o), srlow()) : srlow();
	dataset ma10 = maclose(c, 10), ma20 = maclose(c, 20), ma30 = maclose(c, 30);
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	const ImVec2 p = ImGui::GetCursorScreenPos();
	const float initx = p.x - 10.0, inity = p.y + 4.0;
	float x = initx + xsz / 2.0 - 4.0 + xtot -
			  floor(xtot / (xsz + spacing)) * (xsz + spacing),
		  y = inity;
	const float th = 1.0f;
	const float finalx = initx + xtot;
	if (c.empty())
		return;
	float kh2 = *(h.rbegin()), kl2 = *(l.rbegin());
	for (vector<float>::const_iterator it =
			 h.begin() +
			 max(0, (int)(h.size() - floor(xtot / (xsz + spacing))));
		 it != h.end(); ++it) {
		kh2 = max(*it, kh2);
	}
	for (vector<float>::const_iterator it =
			 l.begin() +
			 max(0, (int)(l.size() - floor(xtot / (xsz + spacing))));
		 it != l.end(); ++it) {
		kl2 = min(*it, kl2);
	}
	const float mul = ysz / (kh2 - kl2);
	x = initx + xtot - floor(xtot / (xsz + spacing)) * (xsz + spacing);
	y = inity;
	for (int i = max(0, (int)(o.size() - floor(xtot / (xsz + spacing))));
		 i < o.size(); ++i) {
		if (i + 1 != o.size()) {
			if (ma10[i])
				draw_list->AddLine(
					ImVec2(x + (xsz / 2.0), y + (kh2 - ma10[i]) * mul),
					ImVec2(x + (xsz / 2.0) + xsz + spacing,
						   y + (kh2 - ma10[i + 1]) * mul),
					ImColor(cols["purple"]), th);
			if (ma20[i])
				draw_list->AddLine(
					ImVec2(x + (xsz / 2.0), y + (kh2 - ma20[i]) * mul),
					ImVec2(x + (xsz / 2.0) + xsz + spacing,
						   y + (kh2 - ma20[i + 1]) * mul),
					ImColor(cols["cyan"]), th);
			if (ma30[i])
				draw_list->AddLine(
					ImVec2(x + (xsz / 2.0), y + (kh2 - ma30[i]) * mul),
					ImVec2(x + (xsz / 2.0) + xsz + spacing,
						   y + (kh2 - ma30[i + 1]) * mul),
					ImColor(cols["pink"]), th);
		}
		drawkandle(o[i], h[i], l[i], c[i], draw_list, xsz, x, y, th, kh2, kl2,
				   mul);
		x += xsz + spacing;
	}
	inslabel(ImVec2(finalx, y + (kh2 - kq[pairn].rbegin()->c) * mul),
			 to_string(kq[pairn].rbegin()->c),
			 ImColor(kq[pairn].rbegin()->c > kq[pairn].rbegin()->o
						 ? cols["green"]
						 : cols["red"]),
			 cols["black"]);
	inslabel(ImVec2(finalx, y + (kh2 - *(ma10.rbegin())) * mul),
			 to_string(*(ma10.rbegin())), ImColor(cols["purple"]),
			 cols["black"]);
	inslabel(ImVec2(finalx, y + (kh2 - *(ma20.rbegin())) * mul),
			 to_string(*(ma20.rbegin())), ImColor(cols["cyan"]), cols["black"]);
	inslabel(ImVec2(finalx, y + (kh2 - *(ma30.rbegin())) * mul),
			 to_string(*(ma30.rbegin())), ImColor(cols["pink"]), cols["black"]);

	string toclose =
		fmtperiod(endt - chrono::system_clock::now().time_since_epoch() /
							 chrono::seconds(1));
	inslabel(
		ImVec2(finalx, y + (kh2 - kq[pairn].rbegin()->c) * mul + 0.01), toclose,
		ImColor(kq[pairn].rbegin()->c > kq[pairn].rbegin()->o ? cols["green"]
															  : cols["red"]),
		cols["black"]);
	if (use_heikin_ashi)
		inslabel(ImVec2(finalx, y + (kh2 - kq[pairn].rbegin()->c) * mul + 0.02),
				 "HA: " + to_string(*(c.rbegin())),
				 ImColor(*(c.rbegin()) > *(o.rbegin()) ? cols["green"]
													   : cols["red"]),
				 cols["black"]);

	float lasty = 0.0, wid = 0.0, hei = 0.0;
	sort(vl.begin(), vl.end(), cmp);
	for (label &l : vl) {
		if (l.pcon != toclose) {
			const size_t siz =
				min(l.pcon.find('.') == string::npos
						? string::npos
						: l.pcon.find('.') + pricescale[symbol] + 1,
					l.pcon.size());
			l.pcon = l.pcon.substr(0, siz);
			if (l.pcon.find('.') == string::npos)
				l.pcon += ".";
			while (l.pcon.find('.') + pricescale[symbol] + 1 != l.pcon.size())
				l.pcon += "0";
		}
		ImVec2 siv = ImGui::CalcTextSize(l.pcon.c_str());
		if (siv.x > wid)
			wid = siv.x;
		if (siv.y > hei)
			hei = siv.y;
	}
	for (label l : vl) {
		l.loc.y -= 6.5;
		l.loc.x += 5.0;
		if (l.loc.y - 0.2 < lasty)
			l.loc.y += (lasty - (l.loc.y - 0.2));
		ImGui::SetCursorScreenPos(l.loc);
		draw_list->AddRectFilled(ImVec2(l.loc.x - 1.0, l.loc.y - 0.2),
								 ImVec2(l.loc.x + 1.0 + wid, l.loc.y + hei),
								 l.col);
		lasty = l.loc.y + hei;

		ImGui::PushStyleColor(ImGuiCol_Text, l.fcol);
		ImGui::Text("%s", l.pcon.c_str());
		ImGui::PopStyleColor();
	}
	vl.clear();
}

void getsyms() {
	string response = "";
	HttpGet(bybit_endpoint + "/v2/public/symbols", response);
	Json::Reader reader;
	Json::Value output;
	reader.parse(response, output);
	output = output["result"];
	for (Json::Value i : output) {
		if (i["quote_currency"].asString() == "USDT") {
			syms.emplace_back(i["name"].asString());
			show[i["name"].asString()] = false;
			data_ok[i["name"].asString()] = true;
		}
		pricescale[i["name"].asString()] = i["price_scale"].asInt();
	}
}

void imap_check() {
	if (imap_endpoint.empty()) {
		chk_imap = to_string(CURLE_FAILED_INIT);
		return;
	}
	CURL *curl = curl_easy_init();
	CURLcode res = CURLE_OK;
	if (curl == NULL) {
		chk_imap = to_string(CURLE_FAILED_INIT);
		return;
	}
	string response;
	curl_easy_setopt(curl, CURLOPT_USERNAME, mailbox_login.c_str());
	curl_easy_setopt(curl, CURLOPT_PASSWORD, mailbox_password.c_str());
	curl_easy_setopt(curl, CURLOPT_URL,
					 ("imaps://" + imap_endpoint + "/INBOX").c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,
					 "UID SEARCH NEW FROM \"noreply@tradingview.com\" SUBJECT "
					 "\"Alert: KRYPTOBOT_\"");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res == CURLE_OK)
		chk_imap = "Success";
	else
		chk_imap = to_string(res);
}

void imap_fetch_uids(string &response) {
	if (imap_endpoint.empty()) {
		chk_imap = to_string(CURLE_FAILED_INIT);
		return;
	}
	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		return;
	}
	curl_easy_setopt(curl, CURLOPT_USERNAME, mailbox_login.c_str());
	curl_easy_setopt(curl, CURLOPT_PASSWORD, mailbox_password.c_str());
	curl_easy_setopt(curl, CURLOPT_URL,
					 ("imaps://" + imap_endpoint + "/INBOX").c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,
					 "UID SEARCH NEW FROM \"noreply@tradingview.com\" SUBJECT "
					 "\"Alert: KRYPTOBOT_\"");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
}
void imap_fetch_subj(int uid, string &response) {
	if (imap_endpoint.empty()) {
		chk_imap = to_string(CURLE_FAILED_INIT);
		return;
	}
	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		return;
	}
	curl_easy_setopt(curl, CURLOPT_USERNAME, mailbox_login.c_str());
	curl_easy_setopt(curl, CURLOPT_PASSWORD, mailbox_password.c_str());
	curl_easy_setopt(curl, CURLOPT_URL,
					 ("imaps://" + imap_endpoint + "/INBOX/;UID=" +
					  to_string(uid) + "/;section=HEADER.FIELDS%20(Subject)")
						 .c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	response = response.substr(response.find("Alert: ") + 7);
}

string parsesignal(string response) {
	string ret = "";
	string::reverse_iterator it = response.rbegin();
	while (!isupper(*it))
		++it;
	while (isupper(*it) && it != response.rend())
		ret += *(it++);
	reverse(ret.begin(), ret.end());
	return ret;
}

void subs_order() {
	Json::Value param;
	param["op"] = "auth";
	param["args"][0] = bybit_key;
	const string exptime = to_string(stoull(system_timestamp()) + 10000);
	param["args"][1] = exptime;
	param["args"][2] = get_signature("GET/realtime" + exptime);
	send_message(&client_priv, &connection_priv, param.toStyledString());
	Json::Value param2;
	param2["op"] = "subscribe";
	param2["args"][0] = "user.order.unifiedAccount";
	send_message(&client_priv, &connection_priv, param2.toStyledString());
}

void check_key() {
	const string url =
		bybit_endpoint + "/unified/v3/private/order/unfilled-orders";
	string response;
	CURLcode res =
		HttpGet_withauth(url, "category=linear&symbol=BTCUSDT", response);
	if (res != CURLE_OK) {
		fprintf(stderr, "Failed: %s\n", curl_easy_strerror(res));
	}
	Json::Reader reader;
	Json::Value output;
	reader.parse(response, output);
	chk_res = output["retMsg"].asString();
	if (chk_res == "Success") {
		turn_off_logging(client_priv);
		client_priv.init_asio();
		set_tls_init_handler(client_priv);
		set_open_handler(client_priv, &connection_priv);
		set_message_handler(client_priv);
		set_url(client_priv, wss_endpoint + "/unified/private/v3");
		my_log.AddLog(
			"[INFO] Established a connection to private websocket endpoint\n");
		threads.emplace_back(&Client::run, &client_priv);
		th_priv = threads.size() - 1;
		sleep(3);
		ws_ready = true;
		subs_order();
	}
	return;
}

float totalAvailableBalance() {
	const string url =
		bybit_endpoint + "/unified/v3/private/account/wallet/balance";
	string response;
	CURLcode res = HttpGet_withauth(url, "", response);
	if (res != CURLE_OK) {
		fprintf(stderr, "Failed: %s\n", curl_easy_strerror(res));
	}
	Json::Reader reader;
	Json::Value output;
	reader.parse(response, output);
	return stof(output["result"]["totalAvailableBalance"].asString());
}

float current_price(string sym, string type) {
	const string url =
		bybit_endpoint +
		"/derivatives/v3/public/tickers?category=linear&symbol=" + sym;
	string response;
	HttpGet(url, response);
	Json::Reader reader;
	Json::Value output;
	reader.parse(response, output);
	return stof(type == "Buy"
					? output["result"]["list"][0]["askPrice"].asString()
					: output["result"]["list"][0]["bidPrice"].asString());
}

float get_position(string symbol) {
	const string url = bybit_endpoint + "/unified/v3/private/position/list";
	string response;
	CURLcode res =
		HttpGet_withauth(url, "category=linear&symbol=" + symbol, response);
	if (res != CURLE_OK)
		fprintf(stderr, "Failed: %s\n", curl_easy_strerror(res));
	Json::Reader reader;
	Json::Value output;
	reader.parse(response, output);
	return stof(output["result"]["list"][0]["size"].asString());
}

string get_side(string symbol) {
	const string url = bybit_endpoint + "/unified/v3/private/position/list";
	string response;
	CURLcode res =
		HttpGet_withauth(url, "category=linear&symbol=" + symbol, response);
	if (res != CURLE_OK)
		fprintf(stderr, "Failed: %s\n", curl_easy_strerror(res));
	Json::Reader reader;
	Json::Value output;
	reader.parse(response, output);
	return output["result"]["list"][0]["side"].asString();
}

void market_order(string sym, string side, float qty, const bool reduce_only) {
	current_order_filled = false;
	char qtyc[64] = {0};
	sprintf(qtyc, "%.3f", qty);
	Json::Value param;
	param["category"] = "linear";
	param["orderType"] = "Market";
	param["qty"] = string(qtyc);
	param["reduceOnly"] = reduce_only;
	param["side"] = side;
	param["symbol"] = sym;
	param["timeInForce"] = "GoodTillCancel";
	string res = "";
	HttpPost_withauth(bybit_endpoint + "/unified/v3/private/order/create",
					  param, res);
	for (const char &ch : res)
		if (isdigit(ch)) {
			if (ch != '0')
				current_order_filled = true; // Bybit returned an error
			break;
		}
	while (!current_order_filled)
		this_thread::sleep_for(chrono::milliseconds(100));
}

void exec_trades(string rsignal) {
	tradeitem tradei;
	float posi;
	string tsignal = "";
	for (char c : rsignal)
		if (isalpha(c) || isdigit(c) || c == '_')
			tsignal += c;
	while (!tsignal.empty() && tsignal.find('_') != string::npos) {
		tsignal = tsignal.substr(tsignal.find('_') + 1);
		tradei = td[tsignal.substr(0, tsignal.find('_'))];
		my_log.AddLog(("[INFO] " + tradei.typ + "\n").c_str());
		if (tradei.typ == "REMOVED")
			continue;
		if (tradei.typ == "Swing buy using same quantity") {
			posi = get_position(tradei.symbol);
			market_order(tradei.symbol, "Buy", posi, true);
			market_order(tradei.symbol, "Buy", posi, false);
		} else if (tradei.typ == "Swing sell using same quantity") {
			posi = get_position(tradei.symbol);
			market_order(tradei.symbol, "Sell", posi, true);
			market_order(tradei.symbol, "Sell", posi, false);
		} else if (tradei.typ == "Buy using specified quantity")
			market_order(tradei.symbol, "Buy", tradei.param, false);
		else if (tradei.typ == "Sell using specified quantity")
			market_order(tradei.symbol, "Sell", tradei.param, false);
		else if (tradei.typ == "Close all positions")
			market_order(tradei.symbol,
						 get_side(tradei.symbol) == "Sell" ? "Buy" : "Sell",
						 get_position(tradei.symbol), true);
		else if (tradei.typ == "Buy using specified quantity, reduce only")
			market_order(tradei.symbol, "Buy", tradei.param, true);
		else if (tradei.typ == "Sell using specified quantity, reduce only")
			market_order(tradei.symbol, "Sell", tradei.param, true);
		else if (tradei.typ == "Buy using specified leverage times total "
							   "available balance")
			market_order(tradei.symbol, "Buy",
						 totalAvailableBalance() * tradei.param /
							 current_price(tradei.symbol, "Buy"),
						 false);
		else if (tradei.typ == "Sell using specified leverage times total "
							   "available balance")
			market_order(tradei.symbol, "Sell",
						 totalAvailableBalance() * tradei.param /
							 current_price(tradei.symbol, "Sell"),
						 false);
	}
	return;
}

void imap_thread_fuid() {
	string uids = "", sj = "";
	int uidn;
	while (!done) {
		for (int i = 0; i < 250 && !done; ++i)
			this_thread::sleep_for(chrono::milliseconds(100));
		if (done)
			return;
		imap_fetch_uids(uids);
		for (size_t i = 0; i < uids.size() && !done; ++i) {
			uidn = 0;
			while (isdigit(uids[i]))
				uidn = uidn * 10 + (uids[i++] & 15);
			if (uidn && !uidset.count(uidn) && !done) {
				uidset.insert(uidn);
				imap_fetch_subj(uidn, sj);
				exec_trades(sj);
			}
		}
	}
	return;
}
void tv_ip_init() {
	// https://www.tradingview.com/support/solutions/43000529348-about-webhooks/
	tv_ip.insert("52.89.214.238");
	tv_ip.insert("34.212.75.30");
	tv_ip.insert("54.218.53.128");
	tv_ip.insert("52.32.178.7");
}
crow::SimpleApp app;
void start_webhook() {
	tv_ip_init();
	CROW_ROUTE(app, "/kryptobot")
		.methods("POST"_method)([](const crow::request &req) {
			if (!tv_ip.count(req.remote_ip_address))
				return crow::response(crow::status::BAD_REQUEST);
			const string sj = req.body;
			exec_trades(sj);
			return crow::response(200);
		});
	CROW_ROUTE(app, "/test")
	([]() { return "Success"; });
	app.loglevel(crow::LogLevel::Warning);
	if (webhook_use_ssl) {
		ifstream if_cert("cert/cert.crt");
		ifstream if_key("cert/cert.key");
		if (if_cert.good() && if_key.good())
			app.port(webhook_port)
				.ssl_file("cert/cert.crt", "cert/cert.key")
				.multithreaded()
				.run();
		else
			my_log.AddLog("[WARN] TLS certificate and/or key not found!\n");
		if_cert.close();
		if_key.close();
	} else
		app.port(webhook_port).multithreaded().run();
}

int main() {
	cols["green"] = hexvec("8AFF80");
	cols["red"] = hexvec("ff6c6b");
	cols["purple"] = hexvec("a9a1e1");
	cols["white"] = hexvec("DFDFDF");
	cols["black"] = hexvec("1B2229");
	cols["pink"] = hexvec("c678dd");
	cols["yellow"] = hexvec("ECBE7B");
	cols["orange"] = hexvec("da8548");
	cols["cyan"] = hexvec("46D9FF");
	cols["green"] = hexvec("98be65");
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return 1;
#if defined(IMGUI_IMPL_OPENGL_ES2)
	const char *glsl_version = "#version 100";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
	const char *glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	const char *glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
	GLFWwindow *window = glfwCreateWindow(640, 480, "KryptoBot", NULL, NULL);
	if (window == NULL)
		return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.IniFilename = NULL;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, cols["black"]);

	rstat = true;
	io.Fonts->AddFontFromFileTTF("JetBrainsMono-Regular.ttf", 15);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	if (abs(stoll(bybit_timestamp()) - stoll(system_timestamp())) > 1000) {
		my_log.AddLog(
			"[WARN] System time is out of sync with Bybit server time!\n");
	}
	while (!glfwWindowShouldClose(window) && !done && !connected_byb) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		my_log.Draw("Logs");
		ImGui::Begin("Preferences", &rstat);
		ImGui::Text("Please choose an endpoint.");
		if (ImGui::RadioButton("Mainnet (api.bybit.com)", &use_endpoint, 0) ||
			ImGui::RadioButton("Mainnet (api.bytick.com)", &use_endpoint, 1) ||
			ImGui::RadioButton("Testnet", &use_endpoint, 2)) {
			switch (use_endpoint) {
			case 0:
				bybit_endpoint = "https://api.bybit.com";
				wss_endpoint = "wss://stream.bybit.com";
				break;
			case 1:
				bybit_endpoint = "https://api.bytick.com";
				wss_endpoint = "wss://stream.bybit.com";
				break;
			case 2:
				bybit_endpoint = "https://api-testnet.bybit.com";
				wss_endpoint = "wss://stream-testnet.bybit.com";
				break;
			}
			// https://bybit-exchange.github.io/docs/derivativesV3/unified_margin/#t-authentication
		}
		if (ImGui::Button("Connect"))
			connected_byb = true;
		ImGui::End();
		if (!rstat)
			done = true;
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(cols["black"].x * cols["black"].w,
					 cols["black"].y * cols["black"].w,
					 cols["black"].z * cols["black"].w, cols["black"].w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}
	getsyms();
	glfwPollEvents();
	turn_off_logging(client);
	client.init_asio();
	set_tls_init_handler(client);
	set_open_handler(client, &connection);
	set_message_handler(client);
	set_url(client, wss_endpoint + "/contract/usdt/public/v3");
	my_log.AddLog(
		"[INFO] Established a connection to public websocket endpoint\n");
	glfwPollEvents();
	static websocketpp::lib::thread t1(&Client::run, &client);
	static thread t2(&keepalive);
	bool established_imap = false;
	string webhook_res = "";
	ttmp.symbol = "ETHUSDT";
	tp[ttmp.symbol] = true;
	ttmp.typ = "Close all positions";
	tac[ttmp.typ] = true;
	while (!glfwWindowShouldClose(window) && !done) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		my_log.Draw("title");
		ImGui::Begin("Preferences", &rstat);
		if (chk_res == "Success")
			ImGui::BeginDisabled();
		ImGui::InputTextWithHint("Key", "<Bybit API Key>", &bybit_key,
								 ImGuiInputTextFlags_Password);
		ImGui::InputTextWithHint("Secret", "<Bybit API Secret>", &bybit_secret,
								 ImGuiInputTextFlags_Password);
		if (chk_res == "Success")
			ImGui::EndDisabled();
		if (chk_res != "Success" && ImGui::Button("Check API key and secret")) {
			chk_res = "...";
			my_log.AddLog("[INFO] Checking API key and secret\n");
			threads.emplace_back(check_key);
		}

		if (!chk_res.empty()) {
			ImGui::Text("Result: ");
			if (chk_res == "Success")
				ImGui::PushStyleColor(ImGuiCol_Text, cols["green"]);
			else if (chk_res != "...")
				ImGui::PushStyleColor(ImGuiCol_Text, cols["red"]);
			ImGui::SameLine();
			ImGui::Text("%s", chk_res.c_str());
			if (chk_res != "...")
				ImGui::PopStyleColor();
		}
		ImGui::Separator();
		ImGui::Text("Mode: ");
		if (!use_webhook && chk_imap == "Success") {
			if (!established_imap) {
				established_imap = true;
				threads.emplace_back(imap_thread_fuid);
			}
			ImGui::BeginDisabled();
		}
		ImGui::RadioButton("Mailbox", &use_webhook, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Webhook", &use_webhook, 1);
		if (!use_webhook) {
			ImGui::InputTextWithHint("IMAP Endpoint", "imap.among.us",
									 &imap_endpoint);
			ImGui::InputTextWithHint("Mailbox Login", "impostor@among.us",
									 &mailbox_login);
			ImGui::InputTextWithHint("Mailbox Password", "Pa55w0rt",
									 &mailbox_password,
									 ImGuiInputTextFlags_Password);
			if (ImGui::Button("Check Mailbox")) {
				chk_imap = "...";
				threads.emplace_back(imap_check);
			}
			if (chk_imap == "Success")
				ImGui::EndDisabled();
			if (!chk_imap.empty()) {
				ImGui::Text("Result: ");
				if (chk_imap == "Success")
					ImGui::PushStyleColor(ImGuiCol_Text, cols["green"]);
				else if (chk_imap != "...")
					ImGui::PushStyleColor(ImGuiCol_Text, cols["red"]);
				ImGui::SameLine();
				ImGui::Text("%s", chk_imap.c_str());
				if (chk_imap != "...")
					ImGui::PopStyleColor();
			}
		} else {
			if (webhook_listening)
				ImGui::BeginDisabled();

			ImGui::InputInt("Port", &webhook_port);
			ImGui::Checkbox("Use TLS Encryption", &webhook_use_ssl);
			if (webhook_use_ssl)
				ImGui::Text("Please put certificate files under cert/, names "
							"cert.crt and cert.key.");
			if (webhook_listening)
				ImGui::EndDisabled();
			if (ImGui::Button("Start Webhook") && !webhook_listening) {
				webhook_listening = true;
				threads.emplace_back(start_webhook);
			}
			if (webhook_listening) {
				if (ImGui::Button("Test Webhook")) {
					webhook_res = "";
					HttpGet(string(webhook_use_ssl ? "https://" : "http://") +
								"localhost:" + to_string(webhook_port) +
								"/test",
							webhook_res);
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop Webhook")) {
					webhook_listening = false;
					app.port(webhook_port).multithreaded().stop();
				}
				ImGui::Text("%s", webhook_res.c_str());
			}
		}
		ImGui::Separator();
		ImGui::Text("Trading rules:");
		ImGui::Text("When receive KRYPTOBOT_");
		ImGui::SameLine();
		ImGui::InputTextWithHint("##", "PleaseBuySomeEthereum", &tsignal);
		if (ImGui::BeginCombo("do", ttmp.typ.c_str())) {
			for (int i = 0; i < acts.size(); ++i)
				if (ImGui::Selectable(acts[i].c_str(), tac[acts[i]]) &&
					ttmp.typ != acts[i]) {
					tac[ttmp.typ] = false;
					tac[acts[i]] = true;
					ttmp.typ = acts[i];
					if (!ac_needparam[i])
						ttmp.param = 0.0;
					ac_current = i;
				}
			ImGui::EndCombo();
		}
		ImGui::Text("on");
		ImGui::SameLine();
		if (ImGui::BeginCombo("trading Pair,", ttmp.symbol.c_str())) {
			for (int i = 0; i < syms.size(); ++i)
				if (ImGui::Selectable(syms[i].c_str(), tp[syms[i]]) &&
					ttmp.symbol != syms[i]) {
					tp[ttmp.symbol] = false;
					tp[syms[i]] = true;
					ttmp.symbol = syms[i];
				}
			ImGui::EndCombo();
		}
		if (ac_needparam[ac_current])
			ImGui::InputFloat("Parameter", &ttmp.param, 1.0, 1.0, "%.2f");
		if (ImGui::Button("Add rule"))
			td[tsignal] = ttmp;
		for (map<string, tradeitem>::iterator it = td.begin(); it != td.end();
			 ++it) {
			if (it->second.typ == "REMOVED")
				continue;
			if (it->second.param != 0.0)
				ImGui::TextWrapped(
					"KRYPTOBOT_%s: %s on pair %s with parameter %.2f",
					it->first.c_str(), it->second.typ.c_str(),
					it->second.symbol.c_str(), it->second.param);
			else
				ImGui::TextWrapped("KRYPTOBOT_%s: %s on pair %s",
								   it->first.c_str(), it->second.typ.c_str(),
								   it->second.symbol.c_str());
			ImGui::SameLine();
			if (ImGui::Button("-"))
				it->second.typ = "REMOVED";
		}
		ImGui::Separator();
		ImGui::Text("Graphs:");
		if (ImGui::BeginCombo("Add Symbol", "ETH")) {
			for (int i = 0; i < syms.size(); ++i) {
				if (ImGui::Selectable(
						syms[i].substr(0, syms[i].size() - 4).c_str(),
						show[syms[i]])) {
					show[syms[i]] ^= 1;
					xsz[syms[i]] = 12.0f;
					if (show[syms[i]])
						threads.emplace_back(subs, syms[i], kinterval);
					else
						threads.emplace_back(unsub, syms[i], kinterval);
				}
			}
			ImGui::EndCombo();
		}
		const int okival = kinterval;
		if (ImGui::BeginCombo("Time Interval", kiusr[kinterval].c_str())) {
			for (int i = 0; i < kival.size(); ++i) {
				const bool isSelected = (kinterval == i);
				if (ImGui::Selectable(kiusr[i].c_str(), isSelected)) {
					kinterval = i;
					for (map<string, bool>::const_iterator it = show.begin();
						 it != show.end(); ++it)
						if (it->second) {
							threads.emplace_back(unsubc, it->first, okival,
												 kinterval);
						}
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::Checkbox("Use Heikin-Ashi Candlesticks", &use_heikin_ashi);
		ImGui::End();
		for (map<string, bool>::const_iterator it = show.begin();
			 it != show.end(); ++it)
			if (it->second && data_ok[it->first]) {
				pairn = it->first;
				ImGui::SetNextWindowSize(ImVec2(200.0f, 150.0f),
										 ImGuiCond_FirstUseEver);
				ImGui::Begin(pairn.substr(0, pairn.size() - 4).c_str(),
							 &show[pairn]);
				xtot = ImGui::GetWindowSize().x - 70.0f;
				ysz = ImGui::GetWindowSize().y - 70.0f;
				ImGui::BeginChild((pairn + ".candle").c_str());
				if (ImGui::IsWindowFocused())
					xsz[pairn] = max(4.0f, xsz[pairn] + io.MouseWheel / 3.0f);
				drawsymbol(pairn, xsz[pairn], rstat);
				ImGui::EndChild();
				ImGui::End();
				if (!show[pairn])
					threads.emplace_back(unsub, pairn, kinterval);
			}
		if (!rstat)
			done = true;
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(cols["black"].x * cols["black"].w,
					 cols["black"].y * cols["black"].w,
					 cols["black"].z * cols["black"].w, cols["black"].w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}
	done = true;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	if (webhook_listening)
		app.port(webhook_port).multithreaded().stop();
	t2.join();
	close_connection(&client, &connection);
	t1.join();
	close_connection(&client_priv, &connection_priv);
	threads[th_priv].join();
	for (size_t i = 0; i < threads.size(); ++i) {
		if (i != th_priv) {
			threads[i].join();
		}
	}
	return 0;
}
