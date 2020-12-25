/**********************************
����UDP����ʵ�ֿɿ����䣬���ܰ�����
�ٽ�������
�ڲ����
��ȷ��Ӧ��
�ܳ�ʱ�ش�
���ۻ�ȷ��
����������
��ӵ������
***********************************/

#include"define.h"

#define SERVER_IP    "127.0.0.1"  //������IP��ַ 
#define SERVER_PORT  8888		//�������˿ں�
#define BUFFER sizeof(packet)  //��������С
#define TIMEOUT 5  //��ʱ����λs������һ���е�����ack����ȷ����
#define WINDOWSIZE 20 //�������ڴ�С

SOCKADDR_IN addrServer;   //��������ַ
SOCKADDR_IN addrClient;   //�ͻ��˵�ַ

SOCKET sockServer;//�������׽���
SOCKET sockClient;//�ͻ����׽���


char buffer[WINDOWSIZE][BUFFER];//ѡ���ش�������
char filepath[20];//�ļ�·��
auto ack = vector<int>(WINDOWSIZE);
int totalpacket;//���ݰ�����
int totalack = 0;//��ȷȷ�ϵ����ݰ�����
int curseq = 0;//��ǰ���͵����ݰ������к�
int curack = 0;//��ǰ�ȴ���ȷ�ϵ����ݰ������кţ���С��
int dupack = 0;//����ack
float cwnd = 1;//ӵ�����ڴ�С����ʼ����Ϊ1
int ssthresh = 32;//��ֵ����ʼ����Ϊ32
unsigned long long seqnumber = static_cast<unsigned long long>(UINT32_MAX) + 1;//���кŸ���
int sendwindow = WINDOWSIZE* BUFFER;//��ʼ����
int recvSize;
int length = sizeof(SOCKADDR);
int STATE = 0;


enum state
{
	SLOWSTART,//������
	AVOID,//ӵ������
	FASTRECO//���ٻָ�
};



//ӵ������������ǻ������ڴ�С
int minwindow(int a, int b)
{
	if (a >= b)
		return b;
	else
		return a;
}


//��ʼ������
void inithandler()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ 
	int err;
	//�汾 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll 
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		cout << "Could not find a usable version of Winsock.dll" << endl;
		WSACleanup();
	}
	sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//�����׽���Ϊ������ģʽ 
	int iMode = 1; //1����������0������ 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//���������� 

	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		cout << "Could  not  bind  the  port" << SERVER_PORT << "for  socket. Error  code is" << err << endl;
		WSACleanup();
		return;
	}
	else
	{
		cout << "�����������ɹ�" << endl;
	}
	for (int i = 0; i < WINDOWSIZE; i++)
	{
		ack[i] = 1;//��ʼ�����Ϊ1
	}
}

//��ʱ�ش�
void timeouthandler()
{
	BOOL flag=false;
	packet* pkt1 = new packet;
	pkt1->init_packet();
	if (ack[curack % WINDOWSIZE] == 2)//�����ش�֮����û��ȷ�ϵģ���Ϊ����ʧ
	{
		for (int i = curack; i != curseq; i = (i++) % seqnumber)
		{
			memcpy(pkt1, &buffer[i % WINDOWSIZE], BUFFER);
			sendto(sockServer, (char*)pkt1, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
			cout << "�ش��� " << i << " �����ݰ�" << endl;
			flag = true;
		}		
	}
	if(flag==true)
	{
		ssthresh = cwnd / 2;
		cwnd = 1;
		STATE = SLOWSTART;//��⵽��ʱ���ͻص�������״̬
		cout << "==========================��⵽��ʱ���ص��������׶�============================" << endl;
		cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << endl << endl;
	}
}

//�����ش�
void FASTRECOhandler()
{
	packet* pkt1 = new packet;
	pkt1->init_packet();
	for (int i = curack; i != curseq; i = (i++) % seqnumber)
	{
		memcpy(pkt1, &buffer[i % WINDOWSIZE], BUFFER);
		sendto(sockServer, (char*)pkt1, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
		cout << "�ش��� " << i << " �����ݰ�" << endl;
	}
}


//�յ����ݰ����ж��Ƿ�����ȷ��ack��������Ӧ����
void ackhandler(unsigned int a)
{
	long long index = a;	
	switch (STATE)
	{
	case SLOWSTART:
		if ((index + seqnumber - curack) % seqnumber < minwindow(cwnd, WINDOWSIZE))
		{
			cout << "<<<<<<<<<<<<<<<<<<<<<<�յ���" << index << "�����ݰ���ack" << endl << endl;
			ack[index % WINDOWSIZE] = 3;
			if (cwnd <= ssthresh)
			{
				cwnd++;
				cout << "==========================�������׶�============================" << endl;
				cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << endl << endl;

			}
			else
			{
				STATE = AVOID;
			}
			//�ۻ�ȷ��
			for (int j = curack; j != (index + 1) % seqnumber; j = (++j) % seqnumber)
			{
				ack[j % WINDOWSIZE] = 1;
				++totalack;
				curack = (curack + 1) % seqnumber;
			}
		}
		else if (index == curack - 1)
		{
			dupack++;
			if (dupack == 3)//��������ش� ״̬��ת��ӵ������
			{
				FASTRECOhandler();
				ssthresh = cwnd / 2;
				cwnd = ssthresh + 3;
				STATE = AVOID;
				dupack = 0;
			}
		}
		break;
	case AVOID:
		if ((index + seqnumber - curack) % seqnumber < minwindow(cwnd, WINDOWSIZE))
		{
			cout << "<<<<<<<<<<<<<<<<<<<<<<�յ���" << index << "�����ݰ���ack" << endl << endl;
			ack[index % WINDOWSIZE] = 3;
			cwnd = cwnd + 1 / cwnd;
			cout << "==========================�ﵽ��ֵ������ӵ������׶�============================" << endl;
			cout << "cwnd=  " << int(cwnd) << "     sstresh=" << ssthresh << endl << endl;
			//�ۻ�ȷ��
			for (int j = curack; j != (index + 1) % seqnumber; j = (++j) % seqnumber)
			{
				ack[j % WINDOWSIZE] = 1;
				++totalack;
				curack = (curack + 1) % seqnumber;
			}
		}
		else if (index == curack - 1)
		{
			dupack++;
			if (dupack == 3)
			{
				FASTRECOhandler();
				STATE = AVOID;
				dupack = 0;
			}
		}
		break;		
	}
			
}

int main()
{
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<��ʼ������<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	inithandler();
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<���ļ�<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	cout << "������Ҫ���͵��ļ�����";
	cin >> filepath;
	ifstream is(filepath, ifstream::in | ios::binary);//�Զ����Ʒ�ʽ���ļ�
	if (!is.is_open()) {
		cout << "�ļ��޷���!" << endl;
		exit(1);
	}
	is.seekg(0, std::ios_base::end);  //���ļ���ָ�붨λ������ĩβ
	int length1 = is.tellg();
	totalpacket = length1 / 1024 + 1;
	cout << "�ļ���СΪ" << length1 << "Bytes,�ܹ���" << totalpacket << "�����ݰ�" << endl;
	is.seekg(0, std::ios_base::beg);  //���ļ���ָ�����¶�λ�����Ŀ�ʼ

	packet* pkt = new packet;
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<��������<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	while (true)
	{
		recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrClient), &length);
		int count = 0;
		int waitcount = 0;
		while (recvSize < 0)
		{
			count++;
			Sleep(100);
			if (count > 20)
			{
				cout << "��ǰû�пͻ����������ӣ�" << endl;
				count = 0;
				break;
			}
		}
		//���ֽ������ӽ׶�
		//�������յ��ͻ��˷�����TAG=0�����ݱ�����ʶ��������
		//��������ͻ��˷���һ�� 100 ��С��״̬�룬��ʾ������׼�����ˣ����Է�������
		//�ͻ����յ� 100 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
		//�������յ� 200 ״̬��֮�󣬾Ϳ�ʼ���������� 
		if (pkt->tag == 0)
		{
			clock_t st = clock();//��ʼ��ʱ
			cout << "��ʼ��������..." << endl;
			int stage = 0;
			bool runFlag = true;
			int waitCount = 0;
			while (runFlag)
			{
				switch (stage)
				{
				case 0://����100�׶�
					pkt = connecthandler(100, totalpacket);
					sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					//sendto(sockServer, buffer, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1://�ȴ�����200�׶�
					ZeroMemory(pkt, sizeof(*pkt));
					recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0)
					{
						++waitCount;
						Sleep(200);
						if (waitCount > 20)
						{
							runFlag = false;
							cout << "���ӽ���ʧ�ܣ��ȴ�����������..." << endl;
							break;
						}
						continue;
					}
					else
					{
						if (pkt->tag == 200)
						{
							pkt->init_packet();
							cout << "��ʼ�ļ�����..." << endl;
							memcpy(pkt->data, filepath, strlen(filepath));
							pkt->len = strlen(filepath);
							sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							stage = 2;					
						}
					}
					break;
					//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<���ݴ���<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
				case 2:
					if (totalack == totalpacket)//���ݰ��������
					{
						pkt->init_packet();
						pkt->tag = 88;
						cout << "*************************************" << endl;
						cout << "���ݴ���ɹ���" << endl;
						cout << "������ʱ: " << (clock() - st) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
						sendto(sockServer, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
						runFlag = false;
						exit(0);
						break;
					}

					while ((curseq + seqnumber - curack) % seqnumber < minwindow(cwnd, WINDOWSIZE) && sendwindow>0)//ֻҪ���ڻ�û�����꣬�ͳ����������ݰ�
					{						
						ZeroMemory(buffer[curseq % WINDOWSIZE], BUFFER);
						pkt->init_packet();
						if (length1 >= 1024)
						{
							is.read(pkt->data, 1024);
							make_pkt(pkt, curseq, 1024);
							length1 -= 1024;							
						}
						else
						{
							is.read(pkt->data, length1);
							make_pkt(pkt, curseq, length1);	
						}
						memcpy(buffer[curseq % WINDOWSIZE], pkt, BUFFER);
						sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
						cout << "������seqΪ " << curseq << " �����ݰ�" << endl << endl;
						ack[curseq % WINDOWSIZE] = 2;
						++curseq;
						curseq %= seqnumber;
					}
					//�ȴ�����ȷ��
					pkt->init_packet();
					recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0)
					{
						waitcount++;
						Sleep(200);
						if (waitcount > 20)
						{
							timeouthandler();
							waitcount = 0;
						}
					}
					else
					{						
						ackhandler(pkt->ack);
						sendwindow = pkt->window;									
					}
					break;
				}
			}
		}
	}
	//�ر��׽��� 
	closesocket(sockServer);
	WSACleanup();
	return 0;
}



