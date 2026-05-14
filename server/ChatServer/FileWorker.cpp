#include "FileWorker.h"

#include "base64.h"

#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>

FileWorker::FileWorker() : _b_stop(false)
{
    _work_thread = std::thread([this]() {
        for (;;) {
            std::unique_lock<std::mutex> lock(_mtx);
            _cv.wait(lock, [this]() {
                return _b_stop || !_task_que.empty();
            });

            if (_b_stop && _task_que.empty()) {
                break;
            }

            auto task = _task_que.front();
            _task_que.pop();
            lock.unlock();
            TaskCallback(task);
        }
    });
}

FileWorker::~FileWorker()
{
    _b_stop = true;
    _cv.notify_one();
    if (_work_thread.joinable()) {
        _work_thread.join();
    }
}

void FileWorker::PostTask(std::shared_ptr<FileTask> task)
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _task_que.push(task);
    }
    _cv.notify_one();
}

void FileWorker::TaskCallback(std::shared_ptr<FileTask> task)
{
    auto out_dir = boost::filesystem::current_path() / "file_uploads";
    boost::filesystem::create_directories(out_dir);

    auto safe_name = SanitizeFileName(task->_name);
    if (safe_name.empty()) {
        safe_name = "upload.bin";
    }

    auto out_path = out_dir / (task->_md5 + "_" + safe_name);
    std::ios_base::openmode mode = std::ios::binary | std::ios::app;
    if (task->_seq == 1) {
        mode = std::ios::binary | std::ios::trunc;
    }

    std::ofstream outfile(out_path.string(), mode);
    if (!outfile) {
        std::cerr << "open upload file failed: " << out_path.string() << std::endl;
        return;
    }

    std::string decoded = base64_decode(task->_file_data);
    outfile.write(decoded.data(), static_cast<std::streamsize>(decoded.size()));
    if (!outfile) {
        std::cerr << "write upload file failed: " << out_path.string() << std::endl;
        return;
    }

    if (task->_last) {
        std::cout << "file upload saved: " << out_path.string() << std::endl;
    }
}

std::string FileWorker::SanitizeFileName(const std::string& file_name)
{
    boost::filesystem::path p(file_name);
    std::string base = p.filename().string();
    for (auto& ch : base) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (uch < 32 || ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
            ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
            ch = '_';
        }
    }
    return base;
}
