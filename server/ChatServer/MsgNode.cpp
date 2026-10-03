#include "MsgNode.h"
#include "const.h"
#include <boost/asio.hpp>

namespace net = boost::asio;

RecvNode::RecvNode(int max_len, short msg_id)
	: MsgNode(max_len), _msg_id(msg_id)
{

}

short RecvNode::GetMsgId()
{
	return _msg_id;
}



SendNode::SendNode(const char* msg, short max_len, short msg_id)
	: MsgNode(max_len + HEAD_TOTAL_LEN), _msg_id(msg_id)
{
	short msg_id_to_net = net::detail::socket_ops::host_to_network_short(_msg_id);
	memcpy(_data, &msg_id_to_net, 2);

	short len_to_net = net::detail::socket_ops::host_to_network_short(_total_len);
	memcpy(_data + HEAD_ID_LEN, &len_to_net, 2);

	memcpy(_data + HEAD_TOTAL_LEN, msg, max_len);
}

