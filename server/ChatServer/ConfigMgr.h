#pragma once

#include <unordered_map>
#include <string>

class SectionInfo {
public:
	SectionInfo() {}
	SectionInfo(const SectionInfo& other) {
		_section_datas = other._section_datas;
	}
	SectionInfo& operator=(const SectionInfo& other) {
		if (this == &other) {
			return *this;
		}

		_section_datas = other._section_datas;
		return *this;
	}

	std::string& operator[](const std::string& key) {
		return _section_datas[key];
	}

	std::unordered_map<std::string, std::string> _section_datas;
};


class ConfigMgr {
public:
	~ConfigMgr() {
		_config_map.clear();
	}

	static ConfigMgr& GetInst() {
		static ConfigMgr config_mgr;
		return config_mgr;
	}

	SectionInfo& operator[](const std::string& key) {
		return _config_map[key];
	}

private:
	ConfigMgr();
	ConfigMgr(const ConfigMgr&) = delete;
	ConfigMgr& operator=(ConfigMgr&) = delete;
	std::unordered_map<std::string, SectionInfo> _config_map;
};