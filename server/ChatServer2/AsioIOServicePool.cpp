#include "AsioIOServicePool.h"

AsioIOServicePool::AsioIOServicePool(std::size_t nums) 
	:	_ioServices(nums), _works(nums), _nextIOService(0)
{
	for (std::size_t i = 0; i < nums; i++) {
		_works[i] = std::make_unique<WorkGuard>(net::make_work_guard(_ioServices[i]));
	}

	for (std::size_t i = 0; i < _ioServices.size(); i++) {
		_threads.emplace_back([this, i]() {
			_ioServices[i].run();
		});
	}
}

AsioIOServicePool::~AsioIOServicePool() {
	Stop();
}

void AsioIOServicePool::Stop() {
	for (std::size_t i = 0; i < _ioServices.size(); i++) {
		_ioServices[i].stop();
	}

	for (std::size_t i = 0; i < _works.size(); i++) {
		_works[i].reset();
	}

	for (std::size_t i = 0; i < _threads.size(); i++) {
		if (_threads[i].joinable()) {
			_threads[i].join();
		}
	}
}

net::io_context& AsioIOServicePool::GetIOService() {
	return _ioServices[(_nextIOService++) % _ioServices.size()];
}

