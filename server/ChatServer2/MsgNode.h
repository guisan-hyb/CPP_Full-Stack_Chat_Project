#pragma once

#include <string>

class MsgNode {
public:
	MsgNode(short max_len) : _total_len(max_len), _cur_len(0) {
		_data = new char[_total_len + 1]();
		_data[_total_len] = '\0';
	}

	virtual ~MsgNode() {
		delete[] _data;
	}

	void Clear() {
		memset(_data, 0, _total_len);
		_cur_len = 0;
	}

	short _cur_len;
	short _total_len;
	char* _data;
};



class RecvNode : public MsgNode
{
public:
	RecvNode(int max_len, short msg_id);
	short GetMsgId();

private:
	short _msg_id;
};

class SendNode : public MsgNode
{
public:
	SendNode(const char* msg, short max_len, short msg_id);
	short GetMsgId();

private:
	short _msg_id;
};