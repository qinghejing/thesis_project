#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class CSession;

struct FileTask {
    FileTask(std::shared_ptr<CSession> session, std::string md5, std::string name,
        int seq, long long total_size, long long trans_size, int last,
        std::string file_data)
        : _session(session), _md5(md5), _name(name), _seq(seq),
          _total_size(total_size), _trans_size(trans_size), _last(last),
          _file_data(file_data) {}

    std::shared_ptr<CSession> _session;
    std::string _md5;
    std::string _name;
    int _seq;
    long long _total_size;
    long long _trans_size;
    int _last;
    std::string _file_data;
};

class FileWorker
{
public:
    FileWorker();
    ~FileWorker();
    void PostTask(std::shared_ptr<FileTask> task);

private:
    void TaskCallback(std::shared_ptr<FileTask> task);
    static std::string SanitizeFileName(const std::string& file_name);

    std::thread _work_thread;
    std::queue<std::shared_ptr<FileTask>> _task_que;
    std::atomic<bool> _b_stop;
    std::mutex _mtx;
    std::condition_variable _cv;
};
