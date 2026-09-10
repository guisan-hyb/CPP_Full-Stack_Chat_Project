#include "AsioIOServicePool.h"

AsioIOServicePool::~AsioIOServicePool()
{
	Stop();// 析构时确保线程安全退出
	std::cout << "AsioIOServicePool destruct" << std::endl;
}

net::io_context& AsioIOServicePool::GetIOService()
{
	auto& service = _ioServices[(_nextIOService++) % _ioServices.size()];
	return service;
}

void AsioIOServicePool::Stop()
{
	// 因为仅仅执行 work_guard.reset() 不能让 io_context 立即从 run 的状态中退出
	// 当 io_context 已经绑定了读或写的监听事件后，还需要手动 stop 该服务
	for (auto& ioc : _ioServices) {
		ioc.stop();
	}

	// 释放 work_guard，防止其阻止 io_context 退出
	for (auto& work : _works) {
		work.reset();
	}

	//等待所有线程结束
	for (auto& t : _threads) {
		if (t.joinable()) {
			t.join();
		}
	}
}

AsioIOServicePool::AsioIOServicePool(std::size_t size) 
	: _ioServices(size), _works(size), _nextIOService(0)
{
	for (std::size_t i = 0; i < size; i++) {
		// 使用 make_work_guard 创建 work_guard，防止 io_context 在没有任务时退出 run()
		_works[i] = std::make_unique<WorkGuard>(net::make_work_guard(_ioServices[i]));
	}

	// 遍历多个 ioservice，创建多个线程，每个线程内部启动 ioservice
	for (std::size_t i = 0; i < size; i++) {
		_threads.emplace_back([this, i]() {
			_ioServices[i].run();
		});
	}
}

