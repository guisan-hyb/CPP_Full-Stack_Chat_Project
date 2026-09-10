#pragma once

#include "const.h"

class AsioIOServicePool : public Singleton<AsioIOServicePool> {
	friend class Singleton<AsioIOServicePool>;
public:
	using IOService = net::io_context;
	using WorkGuard = net::executor_work_guard<net::io_context::executor_type>;
	using WorkGuardPtr = std::unique_ptr<WorkGuard>;

	~AsioIOServicePool();
	AsioIOServicePool(const AsioIOServicePool&) = delete;
	AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

	net::io_context& GetIOService();
	void Stop();

private:
	AsioIOServicePool(std::size_t size = 2 /*std::thread::hardware_concurrency()*/);
	std::vector<IOService> _ioServices;
	std::vector<WorkGuardPtr> _works;
	std::vector<std::thread> _threads;
	std::size_t _nextIOService;
};