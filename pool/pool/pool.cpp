
#include "stdafx.h"
#include <windows.h>
#include <list>
using namespace std;


/****************************************************/

/****************************************************/
HANDLE hEvent;  // 用于同步,是windows api提供的，不是由mfc类库提供的
CRITICAL_SECTION g_cs; //互斥体
bool shutDowm;
// 定时器回调函数
void CALLBACK TimeProc(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime);
// 线程回调函数
DWORD CALLBACK ThreadProc(PVOID pvoid); 
/****************************************************/

/****************************************************/
//任务函数
void * myprocess(void *arg)
{
	Sleep(20000); //模拟工作函数执行事件
	printf("myprocess %s\n",(char *)arg);
	return NULL;
}

struct worker
{
	void * (* p)(void *arg);
	void *arg;
};
//任务队列
list<struct worker> workersList_Pool;

void addWork(void *(* p)(void *arg),void *arg)
{
	struct worker work;
	work.arg=arg;
	work.p=p;
	workersList_Pool.push_back(work);
	SetEvent(hEvent); //事件置位
	/*
	经典注意事项1
	SetEvent(hEvent)能够使空闲线程中的任意一个开始执行（至于是哪一个线程，无从所知）。
	很可能的一件事情是，5个任务都被一个线程执行，这是因为，SetEvent(hEvent)需要时间，可能正好在这段时间内，
	刚才那个被投放的任务被执行完毕，操作系统再次选择一个线程去执行，所选择的这个线程可能正好是那个刚刚
	用的那个线程。
	*/

}
/****************************************************/

/****************************************************/
//线程的信息
struct threadInfo
{
	bool isRuning;
	HANDLE hThread;
};
//线程的链表,也就是线程池
list<struct threadInfo *> threadsList_Pool;

//线程函数
void threadFun(struct threadInfo *threadNode)
{
	while (1)
	{
		DWORD dwWaitResult;
		/*
		经典注意事项3
		为了让所有线程都处在等待状态，必须把WaitForSingleObject 放到if(workersList_Pool.size()==0) 里面
		*/
		if((workersList_Pool.size()==0)&&(shutDowm==false)) 
		{
			//printf("come in %d\n",integer);
			threadNode->isRuning=false;//标示该线程没有被使用
			dwWaitResult = WaitForSingleObject( hEvent,INFINITE); // 无限等待，直到事件被置位//当WaitForSingleObject返回后，自动设置为无信号状态
		}
      

        struct worker w;
		w.p=NULL;

		/*
		经典注意事项2
		设有两个任务，被两个线程在执行，可能的情况是，第一个线程把任务执行完了，并且再次执行到workersList_Pool.size()
		的时候，第二个线程的执行的任务还没有被删除workersList_Pool.pop_front()，这样的话，第一个线程的workersList_Pool.size()
		又不是0了，还会执行workersList_Pool.pop_front()。这样的话，workersList_Pool.pop_front()就被多执行了一次。
		所以，必须再次判断workersList_Pool.size()的值，并且把workersList_Pool.size()和workersList_Pool.pop_front()
		放到互斥体中。
		*/

        EnterCriticalSection(&g_cs);
	    if (workersList_Pool.size()!=0)  //再一次得到size
	    {
			threadNode->isRuning=true; //标示该线程正在被使用
			
			w.arg=workersList_Pool.front().arg;
			w.p=workersList_Pool.front().p;
			workersList_Pool.pop_front(); 
	    }
        LeaveCriticalSection(&g_cs);
        
		//是否摧毁该线程
		if (shutDowm==true)
		{
          ExitThread(0);
		  //线程自我终结，线程所使用的所有资源全部释放
		}

		if (w.p!=NULL)
		{
		  w.p(w.arg);
		  //把最耗时间的 正真要执行的代码放到 互斥体外面
		}
	}
	
	//ExitThread(1);
	//线程的两种退出方式，一个是线程体中的代码执行完了，另一个是在线程体中调用ExitThread(1);
}

/****************************************************/

/****************************************************/
//讨论一个理想状态，destroyPool的时候，所以线程都在阻塞WaitForSingleObject
//就算是不是理想状态，下面的代码也能完美摧毁线程池
void destroyPool()
{
	//linux中在删除该线程之前，是需要先唤醒该线程的，因为该线程还在WaitForSingleObject
	//估计windows 中删除该线程也需要先唤醒这个线程,但在windows中找不到 唤醒所有阻塞线程的函数，所以采取下面的for方式
    shutDowm=true; //摧毁所有线程
    int threadsListPoolSize=threadsList_Pool.size();
    for (int i=0;i<threadsListPoolSize;i++)
    {
       SetEvent(hEvent); //事件置位。
	   //说明1，每发送一个置位，将有一个线程被唤醒
	   //说明2，理想的情况下，因为你想摧毁线程池，说明你不再想使用线程池，说明你认为你的任务已经执行完毕，所以线程池中的线程都是空闲等待的
	   //，所以线程中空闲的线程数就是threadsList_Pool.size()，所以发送threadsList_Pool.size()个 SetEvent(hEvent)
	   /*
        经典注意事项6
		就算不是理想状态，并不是threadsList_Pool.size()个线程在空闲等待，还有几个线程在工作，假设发了threadsList_Pool.size()个 SetEvent(hEvent)，
		但没有threadsList_Pool.size()的WaitForSingleObject，导致部分 SetEvent(hEvent)无处可发，导致操作系统丢弃了部分SetEvent(hEvent)。
		在这种假设的情况下也没有问题，因为我们在线程执行体中的判断是这样的if((workersList_Pool.size()==0)&&(shutDowm==false)) ，我们已经设置了
        shutDowm=true，直接导致线程体不会再执行WaitForSingleObject，而是去执行	if (shutDowm==true){ ExitThread(0);}。
		所以操作系统丢弃了部分SetEvent(hEvent) 并不影响我们的程序。

		关于"操作系统丢弃了部分SetEvent(hEvent)",在linux中的pthread_cond_signal中说的很清晰：如果所有线程都在忙碌(也就是没有WaitForSingleObject)，
		则该语句将没有任何作用
	   */
    }
   
	//阻塞等待所有线程退出
	list<struct threadInfo *>::const_iterator item;
	for (item=threadsList_Pool.begin();item!=threadsList_Pool.end();++item)
	{
		WaitForSingleObject((*(*item)).hThread,INFINITE);
	}

	//关闭所有线程句柄，并释放内存
	for (item=threadsList_Pool.begin();item!=threadsList_Pool.end();++item)
	{
		CloseHandle((*(*item)).hThread);
		free(*item);
	}
    workersList_Pool.clear();
	printf("destroyPool\n");
}
/****************************************************/

/****************************************************/
int _tmain(int argc, _TCHAR* argv[])
{
	int wait;
	shutDowm=false;
	hEvent=CreateEvent(NULL ,false,false,NULL); //创建 自动事件
	InitializeCriticalSection(&g_cs);
	//创建10个线程
	for (int i=0;i<20;i++)
	{
		struct threadInfo *threadNode=(struct threadInfo *)malloc(sizeof(struct threadInfo)); //创建线程链表中的节点
		threadNode->isRuning=false;
	
		HANDLE hThread;
		DWORD threadID;
		hThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)threadFun,(void *)threadNode,0,&threadID); //使用windows api 创建线程
		//(void *)threadNode 是线程的参数
		threadNode->hThread=hThread;
		threadsList_Pool.push_back(threadNode);
		/*
        经典注意事项4
		线程list中的所有线程一开始都被阻塞，WaitForSingleObject，后来一旦SetEvent(hEvent)时，将有任意一个线程被启动，
		因为这个被启动的线程有随机性，所以我们不知道这个线程到底在线程list中的那个位置。
        而任务链表中的任务，我们是可以控制的，我总是让任务链表中的表头的节点先被执行。
		*/
		
	}
   
	//destroyPool(); //销毁所有线程

	//开启定时器，定时检查线程池中的线程是否在全部被使用
	DWORD dwThreadId;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, 0, 0, &dwThreadId); 

	
	for (int i=0;i<10;i++)
	{
		 void *p="i love beijing";
		 addWork(myprocess,p);  //p是void * myprocess(void *arg)的参数，和线程的参数"(void *)threadNode" 根本不是一回事

		 /*
          经典注意事项1
		  SetEvent(hEvent)能够使空闲线程中的任意一个开始执行（至于是哪一个线程，无从所知）。
		  很可能的一件事情是，5个任务都被一个线程执行，这是因为，SetEvent(hEvent)需要时间，可能正好在这段时间内，
		  刚才那个被投放的任务被执行完毕，操作系统再次选择一个线程去执行，所选择的这个线程可能正好是那个刚刚
		  用的那个线程。
		 */
	}
    
	
	scanf("%d",&wait);
	destroyPool(); //销毁所有线程
	scanf("%d",&wait);
	return 0;
}


/****************************************************/

/****************************************************/
/*讨论一个理想状态，定时器检查threadsList_Pool中节点的isRuning的时候，线程池中线程并没有改变threadsList_Pool中节点的isRuning的值
  也就是说定时器检查threadsList_Pool中节点的isRuning的时候，和线程池中线程改变threadsList_Pool中节点的isRuning的值 不是一个时间点

  就算不是理想状态，我们采取加锁InitializeCriticalSection(&g_cs)，也就可以避开上面"一个在读取isRuning,一个在修改isRuning"的问题
*/
void CALLBACK TimeProc(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	int num=0;
	int deleteNum=0;

    InitializeCriticalSection(&g_cs);
	int listSize=threadsList_Pool.size();
	list<struct threadInfo *>::const_iterator item;
	for (item=threadsList_Pool.begin();item!=threadsList_Pool.end();++item)
	{
		if ((*(*item)).isRuning==true)
		{
          num++;
		}
	}
	if (num==listSize) // 此时此刻所有的线程都在被使用，马上增加线程的容量
	{
		for (int i=0;i<10;i++)
		{
			struct threadInfo *threadNode=(struct threadInfo *)malloc(sizeof(struct threadInfo)); //创建线程链表中的节点
			threadNode->isRuning=false;

			HANDLE hThread;
			DWORD threadID;
			hThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)threadFun,(void *)threadNode,0,&threadID); //使用windows api 创建线程
			//(void *)threadNode 是线程的参数
			threadNode->hThread=hThread;
			threadsList_Pool.push_back(threadNode);

		}
	}//if (num==listSize) 

	//如果有多余的没有被使用的线程则删除一定数量的线程
    //例如，如果有10个空闲线程，则删除5个空闲线程
	if ((listSize-num)>10)
	{
		for (item=threadsList_Pool.begin();item!=threadsList_Pool.end();)
		{
			if ((*(*item)).isRuning==false)
			{
				//摧毁该线程
				TerminateThread((*(*item)).hThread,1);
				CloseHandle((*(*item)).hThread);
				//释放该节点所占的堆内存
				free(*item);
                //从lisg中删除该节点
                item = threadsList_Pool.erase(item);  //删除该节点

				deleteNum++;
				if (deleteNum==5)
				{
					break;
				}
			}
			else
			{
               item++;
			}	
		} //for 
	}//if ((listSize-num)>=10)
	LeaveCriticalSection(&g_cs);
	//一开始开启了20个线程，只有10个任务，这个定时删除了5个线程，所以剩下的线程数目应该是15个
	printf("least  %d\n",threadsList_Pool.size());
}

DWORD CALLBACK ThreadProc(PVOID pvoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE); 
	SetTimer(NULL, 10, 10000, TimeProc); //第三个参数就是时间间隔
	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(msg.message == WM_TIMER)
		{
			TranslateMessage(&msg);    // 翻译消息
			DispatchMessage(&msg);     // 分发消息
		}
	}
	KillTimer(NULL, 10);
	printf("thread end here\n");
	return 0;
}

/****************************************************/

/****************************************************/