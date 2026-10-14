#pragma once

#include <mutex>
#include <memory>

template <typename T>
class Singleton {
public:
	~Singleton() = default;
	
	static std::shared_ptr<T> GetInst() {
		static std::once_flag s_flag;
		std::call_once(s_flag, [&]() {
			_ptr = std::shared_ptr<T>(new T);
		});
		return _ptr;
	}

protected:
	Singleton() = default;
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

	static std::shared_ptr<T> _ptr;
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_ptr = nullptr;
