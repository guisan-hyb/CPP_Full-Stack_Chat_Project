#include "ConfigMgr.h"

// 辅助函数：去除字符串首尾的空格和回车换行符
// Linux换行符是\n, Windows换行符是\r\n
std::string trim(const std::string& str) {
	std::size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return "";
	}

	std::size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, last - first + 1);
}

ConfigMgr::ConfigMgr() {
	// 获取当前工作目录 (使用 C++17 std::filesystem)
	std::filesystem::path current_path = std::filesystem::current_path();
	// 构建 config.ini 文件的完整路径
	std::filesystem::path config_path = current_path / "config.ini";
	std::cout << "Config path: " << config_path << std::endl;

	// 使用标准库 ifstream 读取文件
	std::ifstream file(config_path);
	if (!file.is_open()) {
		std::cerr << "Failed to open config file: " << config_path << std::endl;
		return;
	}

	std::string line;
	std::string current_section;
	SectionInfo current_section_info;

	// 逐行解析 INI 文件
	while (std::getline(file, line)) {
		std::string trimmed_line = trim(line);

		// 跳过空行和注释 (以 ; 或 # 开头)
		if (trimmed_line.empty() || trimmed_line[0] == ';' || trimmed_line[0] == '#') {
			continue;
		}

		// 解析Section -> 名称，例如: [GateServer]
		if (trimmed_line.front() == '[' && trimmed_line.back() == ']') {
			// 如果之前已经有 section 在解析，先将其存入 map
			if (!current_section.empty()) {
				_config_map[current_section] = current_section_info;
				current_section_info._section_datas.clear();
			}
			current_section = trimmed_line.substr(1, trimmed_line.size() - 2);// 获取名称 [1~n-2]
		}
		// 解析 键值对
		else {
			std::size_t pos = trimmed_line.find('=');
			if (pos != std::string::npos) {
				std::string key = trim(trimmed_line.substr(0, pos));
				std::string value = trim(trimmed_line.substr(pos + 1));

				if (!key.empty()) {
					current_section_info._section_datas[key] = value;
				}
			}
		}
	}

	// 将最后一个 section 存入 map
	if (!current_section.empty()) {
		_config_map[current_section] = current_section_info;
	}

	// 输出所有的 section 和 key-value 对  
	for (const auto& section_entry : _config_map) {
		const std::string& section_name = section_entry.first;
		const SectionInfo& section_config = section_entry.second;
		std::cout << "[" << section_name << "]" << std::endl;
		for (const auto& key_value_pair : section_config._section_datas) {
			std::cout << key_value_pair.first << "=" << key_value_pair.second << std::endl;
		}
	}
}

