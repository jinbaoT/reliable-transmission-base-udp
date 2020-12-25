#include"define.h"

#define SERVER_PORT  6666 //�������ݵĶ˿ں� 
#define SERVER_IP    "127.0.0.1" //  �������� IP ��ַ 
#define BUFFER sizeof(packet)
#define WINDOWSIZE 20
SOCKET socketClient;//�ͻ����׽���
SOCKADDR_IN addrServer; //��������ַ


char filename[20];
int waitseq = 0;//�ȴ������ݰ�
int totalpacket;//���ݰ�����
int seqnum = 40;//���кŸ���
int len = sizeof(SOCKADDR);
int totalrecv=0;
int recvwindow = WINDOWSIZE*BUFFER;//���մ��ڴ�С
unsigned long long seqnumber = static_cast<unsigned long long>(UINT32_MAX) + 1;//���кŸ���


//ģ�ⶪ��
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound) {
		return TRUE;
	}
	return FALSE;
}


//��ʼ������
void init()
{
	//�����׽��ֿ⣨���룩 
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ 
	int err;
	//�汾 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//�Ҳ��� winsock.dll 
		cout<<"WSAStartup failed with error: "<<err<<endl;
		return ;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		cout<<"Could not find a usable version of Winsock.dll"<<endl;
		WSACleanup();
	}
	else
	{
		cout<<"�׽��ִ����ɹ�"<<endl;
	}
	socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);

}

int main()
{
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<��ʼ������<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	init();
	std::ofstream out_result;
	packet *pkt=new packet;
	pkt->init_packet();
	int stage = 0;
	float packetLossRatio = 0.2;  //Ĭ�ϰ���ʧ�� 
	float ackLossRatio = 0.2;  //Ĭ�� ACK ��ʧ��  							  
	srand((unsigned)time(NULL));//������ӣ�����ѭ���������� 
	BOOL b;
	while (true)
	{
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<��������<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
		pkt->init_packet();
		pkt->tag = 0;
		sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		while (true)
		{
			//�ȴ� server �ظ�
			switch (stage)
			{
			case 0://�ȴ����ֽ׶� 
				recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, &len);
				totalpacket = pkt->len;
				cout << "׼���������ӣ��ܹ���" << totalpacket << "�����ݰ�" << endl;
				pkt->init_packet();
				pkt=connecthandler(200);
				sendto(socketClient, (char*)pkt,sizeof(*pkt) , 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
				stage = 1;
				break;
			case 1:
				recvfrom(socketClient, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrServer, &len);
				memcpy(filename, pkt->data, pkt->len);
				out_result.open(filename, std::ios::out | std::ios::binary);
				cout << "�ļ���Ϊ��" << filename << endl;
				if (!out_result.is_open())
				{
					cout << "�ļ���ʧ�ܣ�����" << endl;
					exit(1);
				}
				stage = 2;
				break;
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<�ļ�����<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
			case 2:
				pkt->init_packet();
				recvfrom(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, &len);
				if (pkt->tag == 88)
				{
					cout << "**************************************" << endl;
					cout << "�ļ�����ɹ�";
					goto success;

				}
                //GBNʵ��				
				if (pkt->seq == waitseq && totalrecv < totalpacket&&!corrupt(pkt))
				{
					
					b = lossInLossRatio(packetLossRatio);
					if (b) {
						cout << "***************��  " << pkt->seq << " �����ݰ���ʧ" << endl << endl;
						continue;
					}
					cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<�յ���" << pkt->seq << "�����ݰ�" << endl << endl;
					recvwindow -= BUFFER;
					out_result.write(pkt->data, pkt->len);
					out_result.flush();
					recvwindow += BUFFER;
					make_mypkt(pkt, waitseq, recvwindow);		
					cout << "���ͶԵ�" << waitseq << "�����ݰ���ȷ��" << endl;
					sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					waitseq++;
					waitseq %= seqnumber;
					totalrecv++;
				}
				else
				{
					make_mypkt(pkt, waitseq - 1, recvwindow);
					cout << "**********�����ڴ������ݰ���������һ���ظ�ack" << waitseq - 1 << endl;
					sendto(socketClient, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
				}
				//SRʵ��
				/*
				if (pkt.seq <= waitseq + windowsize && pkt.seq >= waitseq&&totalrecv<totalpacket)
				{

					b = lossInLossRatio(packetLossRatio);
					if (b) {
						cout << "***************��  " << pkt.seq << " �����ݰ���ʧ" << endl << endl;
						continue;
					}
					cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<�յ���" << pkt.seq << "�����ݰ�" << endl << endl;
					ack_send[pkt.seq - waitseq] = true;
					memcpy(&buffer_1[pkt.seq], &buffer[11], pkt.len);
					buffer[2] = pkt.seq;
					cout << "���ͶԵ�" << pkt.seq << "�����ݰ���ȷ��" << endl;
					sendto(socketClient, buffer, BUFFER, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					int ack_s = 0;
					totalrecv++;
					while (ack_send[ack_s] && ack_s < windowsize)
					{
						out_result.write(buffer_1[ack_s], 1024);
						out_result.flush();
						waitseq++;
						if (waitseq == 20)
							waitseq = 0;
						ack_s += 1;
					}
					//��ǰ��������					
					if (ack_s > 0)
					{
						for (int i = 0; i < windowsize; i++)
						{
							if (ack_s + i < windowsize)
							{
								ack_send[i] = ack_send[i + ack_s];
								memcpy(buffer_1[i], buffer_1[i + ack_s], pkt.len);
								ZeroMemory(buffer_1[i + ack_s], sizeof(buffer_1[i + ack_s]));
							}
							else
							{
								ack_send[i] = false;
								ZeroMemory(buffer_1[i], sizeof(buffer_1[i]));
							}
						}
					}					
				}
				else// if (pkt.seq >= waitseq - windowsize && pkt.seq <= windowsize - 1)
				{
					ZeroMemory(buffer, BUFFER);
					buffer[2] = waitseq;
					cout << "���ڴ����ڣ�������һ���ظ�ACK" << waitseq-1 << endl;
					sendto(socketClient, buffer, 9, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
				}
				break;
		*/
			}
		}
	success:
		{
			out_result.close();
			exit(0);
		}
	}
	closesocket(socketClient);
	WSACleanup();
	return 0;
}