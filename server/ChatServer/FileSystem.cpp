#include "FileSystem.h"

#include "const.h"

FileSystem::FileSystem()
{
    for (int i = 0; i < FILE_WORKER_COUNT; ++i) {
        _file_workers.push_back(std::make_shared<FileWorker>());
    }
}

FileSystem::~FileSystem()
{
}

void FileSystem::PostMsgToQue(std::shared_ptr<FileTask> msg, int index)
{
    if (_file_workers.empty()) {
        return;
    }
    int safe_index = index % static_cast<int>(_file_workers.size());
    _file_workers[safe_index]->PostTask(msg);
}
