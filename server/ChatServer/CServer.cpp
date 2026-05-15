#include "CServer.h"
#include <iostream>
#include "AsioIOServicePool.h"
#include "UserMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include <chrono>
#include <ctime>
CServer::CServer(boost::asio::io_context& io_context, short port):_io_context(io_context), _port(port),
_acceptor(io_context, tcp::endpoint(tcp::v4(),port)), _timer(io_context)
{
	cout << "Server start success, listen on port : " << _port << endl;
	StartAccept();
	_timer.expires_after(std::chrono::seconds(HEARTBEAT_CHECK_INTERVAL));
	_timer.async_wait([this](boost::system::error_code ec) {
		on_timer(ec);
		});
}

CServer::~CServer() {
	boost::system::error_code ec;
	_timer.cancel(ec);
	cout << "Server destruct listen on port : " << _port << endl;
}

void CServer::HandleAccept(shared_ptr<CSession> new_session, const boost::system::error_code& error){
	if (!error) {
		new_session->Start();
		lock_guard<mutex> lock(_mutex);
		_sessions.insert(make_pair(new_session->GetSessionId(), new_session));
	}
	else {
		cout << "session accept failed, error is " << error.what() << endl;
	}

	StartAccept();
}

void CServer::StartAccept() {
	auto &io_context = AsioIOServicePool::GetInstance()->GetIOService();
	shared_ptr<CSession> new_session = make_shared<CSession>(io_context, this);
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, placeholders::_1));
}

void CServer::ClearSession(std::string uuid) {
	std::shared_ptr<CSession> session = nullptr;
	{
		lock_guard<mutex> lock(_mutex);
		auto iter = _sessions.find(uuid);
		if (iter == _sessions.end()) {
			return;
		}
		session = iter->second;
		_sessions.erase(iter);
	}

	if (session) {
		auto uid = session->GetUserId();
		if (uid > 0) {
			UserMgr::GetInstance()->RmvUserSession(uid, uuid);
			auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
			RedisMgr::GetInstance()->DecreaseCount(server_name);
		}
	}
}
bool CServer::CheckValid(const std::string& uuid) {
	lock_guard<mutex> lock(_mutex);
	return _sessions.find(uuid) != _sessions.end();
}

void CServer::on_timer(const boost::system::error_code& ec) {
	if (ec) {
		return;
	}

	std::vector<std::shared_ptr<CSession>> expired_sessions;
	int session_count = 0;
	{
		lock_guard<mutex> lock(_mutex);
		auto now = std::time(nullptr);
		for (auto& item : _sessions) {
			auto& session = item.second;
			if (session->IsHeartbeatExpired(now)) {
				session->Close();
				expired_sessions.push_back(session);
				continue;
			}

			if (session->GetUserId() > 0) {
				session_count++;
			}
		}
	}

	for (auto& session : expired_sessions) {
		session->DealExceptionSession();
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, self_name, std::to_string(session_count));

	_timer.expires_after(std::chrono::seconds(HEARTBEAT_CHECK_INTERVAL));
	_timer.async_wait([this](boost::system::error_code ec) {
		on_timer(ec);
		});
}
