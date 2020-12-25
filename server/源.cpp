/**********************************
基于UDP服务实现可靠传输，功能包括：
①建立连接
②差错检测
③确认应答
④超时重传
⑤累积确认
⑥流量控制
⑦拥塞控制
***********************************/

#include"define.h"

#define SERVER_IP    "127.0.0.1"  //服务器IP地址 
#define SERVER_PORT  8888		//服务器端口号
#define BUFFER sizeof(packet)  //缓冲区大小
#define TIMEOUT 5  //超时，单位s，代表一组中的所有ack都正确接收
#define WINDOWSIZE 20 //滑动窗口大小

SOCKADDR_IN addrServer;   //服务器地址
SOCKADDR_IN addrClient;   //客户端地址

SOCKET sockServer;//服务器套接字
SOCKET sockClient;//客户端套接字


char buffer[WINDOWSIZE][BUFFER];//选择重传缓冲区
char filepath[20];//文件路径
auto ack = vector<int>(WINDOWSIZE);
int totalpacket;//数据包个数
int totalack = 0;//正确确认的数据包个数
int curseq = 0;//当前发送的数据包的序列号
int curack = 0;//当前等待被确认的数据包的序列号（最小）
int dupack = 0;//冗余ack
float cwnd = 1;//拥塞窗口大小，初始设置为1
int ssthresh = 32;//阈值，初始设置为32
unsigned long long seqnumber = static_cast<unsigned long long>(UINT32_MAX) + 1;//序列号个数
int sendwindow = WINDOWSIZE* BUFFER;//初始设置
int recvSize;
int length = sizeof(SOCKADDR);
int STATE = 0;


enum state
{
	SLOWSTART,//慢启动
	AVOID,//拥塞避免
	FASTRECO//快速恢复
};



//拥塞避免的上限是滑动窗口大小
int minwindow(int a, int b)
{
	if (a >= b)
		return b;
	else
		return a;
}


//初始化工作
void inithandler()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示 
	int err;
	//版本 2.2 
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll 
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		cout << "Could not find a usable version of Winsock.dll" << endl;
		WSACleanup();
	}
	sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//设置套接字为非阻塞模式 
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 

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
		cout << "服务器创建成功" << endl;
	}
	for (int i = 0; i < WINDOWSIZE; i++)
	{
		ack[i] = 1;//初始都标记为1
	}
}

//超时重传
void timeouthandler()
{
	BOOL flag=false;
	packet* pkt1 = new packet;
	pkt1->init_packet();
	if (ack[curack % WINDOWSIZE] == 2)//快速重传之后还有没被确认的，认为包丢失
	{
		for (int i = curack; i != curseq; i = (i++) % seqnumber)
		{
			memcpy(pkt1, &buffer[i % WINDOWSIZE], BUFFER);
			sendto(sockServer, (char*)pkt1, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
			cout << "重传第 " << i << " 号数据包" << endl;
			flag = true;
		}		
	}
	if(flag==true)
	{
		ssthresh = cwnd / 2;
		cwnd = 1;
		STATE = SLOWSTART;//检测到超时，就回到慢启动状态
		cout << "==========================检测到超时，回到慢启动阶段============================" << endl;
		cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << endl << endl;
	}
}

//快速重传
void FASTRECOhandler()
{
	packet* pkt1 = new packet;
	pkt1->init_packet();
	for (int i = curack; i != curseq; i = (i++) % seqnumber)
	{
		memcpy(pkt1, &buffer[i % WINDOWSIZE], BUFFER);
		sendto(sockServer, (char*)pkt1, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
		cout << "重传第 " << i << " 号数据包" << endl;
	}
}


//收到数据包，判断是否是正确的ack，并做相应处理
void ackhandler(unsigned int a)
{
	long long index = a;	
	switch (STATE)
	{
	case SLOWSTART:
		if ((index + seqnumber - curack) % seqnumber < minwindow(cwnd, WINDOWSIZE))
		{
			cout << "<<<<<<<<<<<<<<<<<<<<<<收到第" << index << "号数据包的ack" << endl << endl;
			ack[index % WINDOWSIZE] = 3;
			if (cwnd <= ssthresh)
			{
				cwnd++;
				cout << "==========================慢启动阶段============================" << endl;
				cout << "cwnd=  " << cwnd << "     sstresh= " << ssthresh << endl << endl;

			}
			else
			{
				STATE = AVOID;
			}
			//累积确认
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
			if (dupack == 3)//进入快速重传 状态跳转到拥塞避免
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
			cout << "<<<<<<<<<<<<<<<<<<<<<<收到第" << index << "号数据包的ack" << endl << endl;
			ack[index % WINDOWSIZE] = 3;
			cwnd = cwnd + 1 / cwnd;
			cout << "==========================达到阈值，进入拥塞避免阶段============================" << endl;
			cout << "cwnd=  " << int(cwnd) << "     sstresh=" << ssthresh << endl << endl;
			//累积确认
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
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<初始化工作<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	inithandler();
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<读文件<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
	cout << "请输入要发送的文件名：";
	cin >> filepath;
	ifstream is(filepath, ifstream::in | ios::binary);//以二进制方式打开文件
	if (!is.is_open()) {
		cout << "文件无法打开!" << endl;
		exit(1);
	}
	is.seekg(0, std::ios_base::end);  //将文件流指针定位到流的末尾
	int length1 = is.tellg();
	totalpacket = length1 / 1024 + 1;
	cout << "文件大小为" << length1 << "Bytes,总共有" << totalpacket << "个数据包" << endl;
	is.seekg(0, std::ios_base::beg);  //将文件流指针重新定位到流的开始

	packet* pkt = new packet;
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<建立连接<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
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
				cout << "当前没有客户端请求连接！" << endl;
				count = 0;
				break;
			}
		}
		//握手建立连接阶段
		//服务器收到客户端发来的TAG=0的数据报，标识请求连接
		//服务器向客户端发送一个 100 大小的状态码，表示服务器准备好了，可以发送数据
		//客户端收到 100 之后回复一个 200 大小的状态码，表示客户端准备好了，可以接收数据了
		//服务器收到 200 状态码之后，就开始发送数据了 
		if (pkt->tag == 0)
		{
			clock_t st = clock();//开始计时
			cout << "开始建立连接..." << endl;
			int stage = 0;
			bool runFlag = true;
			int waitCount = 0;
			while (runFlag)
			{
				switch (stage)
				{
				case 0://发送100阶段
					pkt = connecthandler(100, totalpacket);
					sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					//sendto(sockServer, buffer, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1://等待接收200阶段
					ZeroMemory(pkt, sizeof(*pkt));
					recvSize = recvfrom(sockServer, (char*)pkt, BUFFER, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0)
					{
						++waitCount;
						Sleep(200);
						if (waitCount > 20)
						{
							runFlag = false;
							cout << "连接建立失败！等待建立新连接..." << endl;
							break;
						}
						continue;
					}
					else
					{
						if (pkt->tag == 200)
						{
							pkt->init_packet();
							cout << "开始文件传输..." << endl;
							memcpy(pkt->data, filepath, strlen(filepath));
							pkt->len = strlen(filepath);
							sendto(sockServer, (char*)pkt, BUFFER, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							stage = 2;					
						}
					}
					break;
					//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<数据传输<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<//
				case 2:
					if (totalack == totalpacket)//数据包传输完毕
					{
						pkt->init_packet();
						pkt->tag = 88;
						cout << "*************************************" << endl;
						cout << "数据传输成功！" << endl;
						cout << "传输用时: " << (clock() - st) * 1000.0 / CLOCKS_PER_SEC << "ms" << endl;
						sendto(sockServer, (char*)pkt, sizeof(*pkt), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
						runFlag = false;
						exit(0);
						break;
					}

					while ((curseq + seqnumber - curack) % seqnumber < minwindow(cwnd, WINDOWSIZE) && sendwindow>0)//只要窗口还没被用完，就持续发送数据包
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
						cout << "发送了seq为 " << curseq << " 的数据包" << endl << endl;
						ack[curseq % WINDOWSIZE] = 2;
						++curseq;
						curseq %= seqnumber;
					}
					//等待接收确认
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
	//关闭套接字 
	closesocket(sockServer);
	WSACleanup();
	return 0;
}



