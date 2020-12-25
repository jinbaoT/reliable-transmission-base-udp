#ifndef __DEFINE_H__
#define __DEFINE_H__
#include"pkt.h"
struct packet
{
	unsigned char tag;//连接建立、断开标识 
	unsigned int seq;//序列号 
	unsigned int ack;//确认号
	unsigned short len;//数据部分长度
	unsigned short checksum;//校验和
	unsigned short window;//窗口
	char data[1024];//数据长度

	void init_packet()
	{
		this->tag = -1;
		this->seq = -1;
		this->ack = -1;
		this->len = -1;
		this->checksum = -1;
		this->window = -1;
		ZeroMemory(this->data, 1024);
	}

};

//计算校验和
unsigned short makesum(int count, char* buf)
{
	unsigned long sum;
	for (sum = 0; count > 0; count--)
	{
		sum += *buf++;
		sum = (sum >> 16) + (sum & 0xffff);
	}
	return ~sum;
}

// 判断包是否损坏
bool corrupt(packet* pkt)
{
	int count = sizeof(pkt->data) / 2;
	register unsigned long sum = 0;
	unsigned short* buf = (unsigned short*)(pkt->data);
	while (count--) {
		sum += *buf++;
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}
	if (pkt->checksum == ~(sum & 0xFFFF))
		return true;
	return false;
}

void make_mypkt(packet* pkt, long long ack, unsigned short window)
{
	pkt->ack = ack;
	pkt->window = window;
}


packet* connecthandler(int tag)
{
	packet* pkt = new packet;
	pkt->init_packet();
	pkt->tag = tag;
	return pkt;
}
#endif