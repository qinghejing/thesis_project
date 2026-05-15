#pragma once
#include <boost/asio.hpp>
#include "CSession.h"
#include <memory.h>
#include <map>
#include <mutex>
#include <vector>
using namespace std;
using boost::asio::ip::tcp;
class CServer
{
public:
	CServer(boost::asio::io_context& io_context, short port);
	~CServer();
	void ClearSession(std::string);
	bool CheckValid(const std::string& uuid);
private:
	void HandleAccept(shared_ptr<CSession>, const boost::system::error_code & error);
	void StartAccept();
	void on_timer(const boost::system::error_code& ec);
	boost::asio::io_context &_io_context;
	short _port;
	tcp::acceptor _acceptor;
	boost::asio::steady_timer _timer;
	std::map<std::string, shared_ptr<CSession>> _sessions;
	std::mutex _mutex;
};

