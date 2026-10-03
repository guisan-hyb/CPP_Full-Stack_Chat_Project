#include "ConfigMgr.h"
#include <filesystem>
#include <fstream>
#include <iostream>

std::string trim(const std::string& str) {
	std::size_t f_pos = str.find_first_not_of(" \n\r\t");
	if (f_pos == std::string::npos) {
		return "";
	}

	std::size_t l_pos = str.find_last_not_of(" \n\r\t");
	return str.substr(f_pos, l_pos - f_pos + 1);
}

ConfigMgr::ConfigMgr() {
	std::filesystem::path current_path = std::filesystem::current_path();
	std::filesystem::path config_path = current_path / "config.ini";
	std::cout << "Config path: " << config_path << std::endl;

	std::ifstream file(config_path);
	if (!file.is_open()) {
		std::cerr << "Failed to open config file: " << config_path << std::endl;
		return;
	}

	std::string line;
	std::string sectionInfo_name;
	while (std::getline(file, line)) {
		line = trim(line);

		// 记得跳过空行和注释
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;

		if (line.front() == '[' && line.back() == ']') {
			sectionInfo_name = line.substr(1, line.size() - 2);
		}
		else {
			std::size_t pos = line.find('=');
			if (pos == std::string::npos) {
				continue;
			}

			std::string key = trim(line.substr(0, pos));
			std::string val = trim(line.substr(pos + 1));

			if (!key.empty() && !sectionInfo_name.empty())
				_config_map[sectionInfo_name][key] = val;
			// 注：这种写法要把 []重载的返回值改为引用
		}
	}

	// 测试, 打印所有的键值对
	for (auto& ele : _config_map) {
		auto& sectionInfo_name = ele.first;
		auto& sectionInfo = ele.second;
		std::cout << "[" << sectionInfo_name << "]" << std::endl;
		for (auto& pair : sectionInfo._section_datas) {
			std::cout << pair.first << " = " << pair.second << std::endl;
		}
	}
}

