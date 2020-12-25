#ifndef __DEFINE_H__
#define __DEFINE_H__
#include"pkt.h"

struct packet
{
	unsigned char tag;//���ӽ������Ͽ���ʶ 
	unsigned int seq;//���к� 
	unsigned int ack;//ȷ�Ϻ�
	unsigned short len;//���ݲ��ֳ���
	unsigned short checksum;//У���
	unsigned short window;//����
	char data[1024];//���ݳ���

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

unsigned short makesum(int count,  char* buf)
{
	unsigned long sum;
	for (sum = 0; count > 0; count--)
	{
		sum += *buf++;
		sum = (sum >> 16) + (sum & 0xffff);
	}
	return ~sum;
}

void make_pkt(packet* pkt, unsigned int nextseqnum, unsigned int length)
{
	pkt->seq = nextseqnum;
	pkt->len = length;
	pkt->checksum = makesum(sizeof(packet) / 2, pkt->data);
}

// �жϰ��Ƿ���
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


packet* connecthandler(int tag, int packetnum)
{
	packet* pkt = new packet;
	pkt->tag = tag;
	pkt->len = packetnum;
	return pkt;
}

#endif
