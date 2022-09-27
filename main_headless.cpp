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
#include <csignal>
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
vector<string> acts = {"Swing buy using same quantity",
					   "Swing sell using same quantity",
					   "Buy using specified quantity",
					   "Sell using specified quantity",
					   "Close all positions",
					   "Buy using specified quantity, reduce only",
					   "Sell using specified quantity, reduce only",
					   "Buy using specified leverage times total equity",
					   "Sell using specified leverage times total equity"};
int th_priv;
vector<bool> ac_needparam = {false, false, true, true, false,
							 true,	true,  true, true};
bool use_heikin_ashi = false, connected_byb = false, webhook_use_ssl = false,
	 webhook_listening = false;
string bybit_key = "", bybit_secret = "",
	   bybit_endpoint = "https://api-testnet.bybit.com",
	   wss_endpoint = "wss://stream-testnet.bybit.com", chk_imap = "";
string tsignal;
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
	tradeitem operator=(const tradeitem t2) {
		typ = t2.typ;
		symbol = t2.symbol;
		param = t2.param;
		return *this;
	}
};

tradeitem ttmp;
vector<thread> threads;
map<string, tradeitem> td;
unordered_set<int> uidset;
unordered_set<string> tv_ip;
string chk_res;
bool ws_ready = false;

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
	const string prefixo = "user.order.unifiedAccount";
	if (output.isMember("topic") && output["topic"].asString() == prefixo) {
		if (output["data"]["result"][0]["orderStatus"].asString() == "Filled") {
			current_order_filled = true;
			clog << "[INFO] Order filled" << endl;
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

string imap_endpoint = "", mailbox_login = "", mailbox_password = "";

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
		cout << "Success" << endl;
	else
		cout << to_string(res) << endl;
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
	HttpGet_withauth(url, "category=linear&symbol=BTCUSDT", response);
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
		clog << "[INFO] Established a connection to private websocket endpoint"
			 << endl;
		threads.emplace_back(&Client::run, &client_priv);
		th_priv = threads.size() - 1;
		sleep(3);
		ws_ready = true;
		subs_order();
	} else
		cerr << "[WARN] " << chk_res << endl;
	return;
}

float totalEquity() {
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
	return stof(output["result"]["totalEquity"].asString());
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
		clog << "[INFO] " << tradei.typ << endl;
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
							   "equity")
			market_order(tradei.symbol, "Buy",
						 totalEquity() * tradei.param /
							 current_price(tradei.symbol, "Buy"),
						 false);
		else if (tradei.typ == "Sell using specified leverage times total "
							   "equity")
			market_order(tradei.symbol, "Sell",
						 totalEquity() * tradei.param /
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
			cerr << "[WARN] TLS certificate and/or key not found!" << endl;
		if_cert.close();
		if_key.close();
	} else
		app.port(webhook_port).multithreaded().run();
}

int main() {
	Json::Value conf;
	Json::Reader reader;
	ifstream conf_file("config.json");
	conf_file >> conf;
	conf_file.close();
	bybit_key = conf["key"].asString();
	bybit_secret = conf["secret"].asString();
	bybit_endpoint = conf["http_url"].asString();
	wss_endpoint = conf["ws_url"].asString();
	if (abs(stoll(bybit_timestamp()) - stoll(system_timestamp())) > 1000)
		cerr << "[WARN] System time is out of sync with Bybit server time!"
			 << endl;
	turn_off_logging(client);
	client.init_asio();
	set_tls_init_handler(client);
	set_open_handler(client, &connection);
	set_message_handler(client);
	set_url(client, wss_endpoint + "/contract/usdt/public/v3");
	clog << "[INFO] Established a connection to public websocket endpoint"
		 << endl;
	static websocketpp::lib::thread t1(&Client::run, &client);
	static thread t2(&keepalive);
	check_key();
	for (Json::Value i : conf["rules"]) {
		ttmp.typ = i["type"].asString();
		ttmp.symbol = i["pair"].asString();
		for (size_t j = 0; j < acts.size(); ++j)
			if (ttmp.typ == acts[j]) {
				if (ac_needparam[j])
					ttmp.param = i["arg"].asFloat();
				break;
			}
		td[i["signal"].asString()] = ttmp;
	}
	if (conf["mode"].asString() == "mailbox") {
		imap_endpoint = conf["smtp_url"].asString();
		mailbox_login = conf["smtp_login"].asString();
		mailbox_password = conf["smtp_password"].asString();
		imap_check();
	} else {
		webhook_port = conf["webhook_port"].asInt();
		webhook_use_ssl = conf["webhook_use_ssl"].asBool();
		webhook_listening = true;
		threads.emplace_back(start_webhook);
	}
	getchar();
	cout << "Exiting..." << endl;
	done = true;
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
