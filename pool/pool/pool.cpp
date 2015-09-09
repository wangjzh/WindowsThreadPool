
#include "stdafx.h"
#include <windows.h>
#include <list>
using namespace std;


/****************************************************/

/****************************************************/
HANDLE hEvent;  // ����ͬ��,��windows api�ṩ�ģ�������mfc����ṩ��
CRITICAL_SECTION g_cs; //������
bool shutDowm;
// ��ʱ���ص�����
void CALLBACK TimeProc(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime);
// �̻߳ص�����
DWORD CALLBACK ThreadProc(PVOID pvoid); 
/****************************************************/

/****************************************************/
//������
void * myprocess(void *arg)
{
	Sleep(20000); //ģ�⹤������ִ���¼�
	printf("myprocess %s\n",(char *)arg);
	return NULL;
}

struct worker
{
	void * (* p)(void *arg);
	void *arg;
};
//�������
list<struct worker> workersList_Pool;

void addWork(void *(* p)(void *arg),void *arg)
{
	struct worker work;
	work.arg=arg;
	work.p=p;
	workersList_Pool.push_back(work);
	SetEvent(hEvent); //�¼���λ
	/*
	����ע������1
	SetEvent(hEvent)�ܹ�ʹ�����߳��е�����һ����ʼִ�У���������һ���̣߳��޴���֪����
	�ܿ��ܵ�һ�������ǣ�5�����񶼱�һ���߳�ִ�У�������Ϊ��SetEvent(hEvent)��Ҫʱ�䣬�������������ʱ���ڣ�
	�ղ��Ǹ���Ͷ�ŵ�����ִ����ϣ�����ϵͳ�ٴ�ѡ��һ���߳�ȥִ�У���ѡ�������߳̿����������Ǹ��ո�
	�õ��Ǹ��̡߳�
	*/

}
/****************************************************/

/****************************************************/
//�̵߳���Ϣ
struct threadInfo
{
	bool isRuning;
	HANDLE hThread;
};
//�̵߳�����,Ҳ�����̳߳�
list<struct threadInfo *> threadsList_Pool;

//�̺߳���
void threadFun(struct threadInfo *threadNode)
{
	while (1)
	{
		DWORD dwWaitResult;
		/*
		����ע������3
		Ϊ���������̶߳����ڵȴ�״̬�������WaitForSingleObject �ŵ�if(workersList_Pool.size()==0) ����
		*/
		if((workersList_Pool.size()==0)&&(shutDowm==false)) 
		{
			//printf("come in %d\n",integer);
			threadNode->isRuning=false;//��ʾ���߳�û�б�ʹ��
			dwWaitResult = WaitForSingleObject( hEvent,INFINITE); // ���޵ȴ���ֱ���¼�����λ//��WaitForSingleObject���غ��Զ�����Ϊ���ź�״̬
		}
      

        struct worker w;
		w.p=NULL;

		/*
		����ע������2
		�����������񣬱������߳���ִ�У����ܵ�����ǣ���һ���̰߳�����ִ�����ˣ������ٴ�ִ�е�workersList_Pool.size()
		��ʱ�򣬵ڶ����̵߳�ִ�е�����û�б�ɾ��workersList_Pool.pop_front()�������Ļ�����һ���̵߳�workersList_Pool.size()
		�ֲ���0�ˣ�����ִ��workersList_Pool.pop_front()�������Ļ���workersList_Pool.pop_front()�ͱ���ִ����һ�Ρ�
		���ԣ������ٴ��ж�workersList_Pool.size()��ֵ�����Ұ�workersList_Pool.size()��workersList_Pool.pop_front()
		�ŵ��������С�
		*/

        EnterCriticalSection(&g_cs);
	    if (workersList_Pool.size()!=0)  //��һ�εõ�size
	    {
			threadNode->isRuning=true; //��ʾ���߳����ڱ�ʹ��
			
			w.arg=workersList_Pool.front().arg;
			w.p=workersList_Pool.front().p;
			workersList_Pool.pop_front(); 
	    }
        LeaveCriticalSection(&g_cs);
        
		//�Ƿ�ݻٸ��߳�
		if (shutDowm==true)
		{
          ExitThread(0);
		  //�߳������սᣬ�߳���ʹ�õ�������Դȫ���ͷ�
		}

		if (w.p!=NULL)
		{
		  w.p(w.arg);
		  //�����ʱ��� ����Ҫִ�еĴ���ŵ� ����������
		}
	}
	
	//ExitThread(1);
	//�̵߳������˳���ʽ��һ�����߳����еĴ���ִ�����ˣ���һ�������߳����е���ExitThread(1);
}

/****************************************************/

/****************************************************/
//����һ������״̬��destroyPool��ʱ�������̶߳�������WaitForSingleObject
//�����ǲ�������״̬������Ĵ���Ҳ�������ݻ��̳߳�
void destroyPool()
{
	//linux����ɾ�����߳�֮ǰ������Ҫ�Ȼ��Ѹ��̵߳ģ���Ϊ���̻߳���WaitForSingleObject
	//����windows ��ɾ�����߳�Ҳ��Ҫ�Ȼ�������߳�,����windows���Ҳ��� �������������̵߳ĺ��������Բ�ȡ�����for��ʽ
    shutDowm=true; //�ݻ������߳�
    int threadsListPoolSize=threadsList_Pool.size();
    for (int i=0;i<threadsListPoolSize;i++)
    {
       SetEvent(hEvent); //�¼���λ��
	   //˵��1��ÿ����һ����λ������һ���̱߳�����
	   //˵��2�����������£���Ϊ����ݻ��̳߳أ�˵���㲻����ʹ���̳߳أ�˵������Ϊ��������Ѿ�ִ����ϣ������̳߳��е��̶߳��ǿ��еȴ���
	   //�������߳��п��е��߳�������threadsList_Pool.size()�����Է���threadsList_Pool.size()�� SetEvent(hEvent)
	   /*
        ����ע������6
		���㲻������״̬��������threadsList_Pool.size()���߳��ڿ��еȴ������м����߳��ڹ��������跢��threadsList_Pool.size()�� SetEvent(hEvent)��
		��û��threadsList_Pool.size()��WaitForSingleObject�����²��� SetEvent(hEvent)�޴��ɷ������²���ϵͳ�����˲���SetEvent(hEvent)��
		�����ּ���������Ҳû�����⣬��Ϊ�������߳�ִ�����е��ж���������if((workersList_Pool.size()==0)&&(shutDowm==false)) �������Ѿ�������
        shutDowm=true��ֱ�ӵ����߳��岻����ִ��WaitForSingleObject������ȥִ��	if (shutDowm==true){ ExitThread(0);}��
		���Բ���ϵͳ�����˲���SetEvent(hEvent) ����Ӱ�����ǵĳ���

		����"����ϵͳ�����˲���SetEvent(hEvent)",��linux�е�pthread_cond_signal��˵�ĺ���������������̶߳���æµ(Ҳ����û��WaitForSingleObject)��
		�����佫û���κ�����
	   */
    }
   
	//�����ȴ������߳��˳�
	list<struct threadInfo *>::const_iterator item;
	for (item=threadsList_Pool.begin();item!=threadsList_Pool.end();++item)
	{
		WaitForSingleObject((*(*item)).hThread,INFINITE);
	}

	//�ر������߳̾�������ͷ��ڴ�
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
	hEvent=CreateEvent(NULL ,false,false,NULL); //���� �Զ��¼�
	InitializeCriticalSection(&g_cs);
	//����10���߳�
	for (int i=0;i<20;i++)
	{
		struct threadInfo *threadNode=(struct threadInfo *)malloc(sizeof(struct threadInfo)); //�����߳������еĽڵ�
		threadNode->isRuning=false;
	
		HANDLE hThread;
		DWORD threadID;
		hThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)threadFun,(void *)threadNode,0,&threadID); //ʹ��windows api �����߳�
		//(void *)threadNode ���̵߳Ĳ���
		threadNode->hThread=hThread;
		threadsList_Pool.push_back(threadNode);
		/*
        ����ע������4
		�߳�list�е������߳�һ��ʼ����������WaitForSingleObject������һ��SetEvent(hEvent)ʱ����������һ���̱߳�������
		��Ϊ������������߳�������ԣ��������ǲ�֪������̵߳������߳�list�е��Ǹ�λ�á�
        �����������е����������ǿ��Կ��Ƶģ������������������еı�ͷ�Ľڵ��ȱ�ִ�С�
		*/
		
	}
   
	//destroyPool(); //���������߳�

	//������ʱ������ʱ����̳߳��е��߳��Ƿ���ȫ����ʹ��
	DWORD dwThreadId;
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, 0, 0, &dwThreadId); 

	
	for (int i=0;i<10;i++)
	{
		 void *p="i love beijing";
		 addWork(myprocess,p);  //p��void * myprocess(void *arg)�Ĳ��������̵߳Ĳ���"(void *)threadNode" ��������һ����

		 /*
          ����ע������1
		  SetEvent(hEvent)�ܹ�ʹ�����߳��е�����һ����ʼִ�У���������һ���̣߳��޴���֪����
		  �ܿ��ܵ�һ�������ǣ�5�����񶼱�һ���߳�ִ�У�������Ϊ��SetEvent(hEvent)��Ҫʱ�䣬�������������ʱ���ڣ�
		  �ղ��Ǹ���Ͷ�ŵ�����ִ����ϣ�����ϵͳ�ٴ�ѡ��һ���߳�ȥִ�У���ѡ�������߳̿����������Ǹ��ո�
		  �õ��Ǹ��̡߳�
		 */
	}
    
	
	scanf("%d",&wait);
	destroyPool(); //���������߳�
	scanf("%d",&wait);
	return 0;
}


/****************************************************/

/****************************************************/
/*����һ������״̬����ʱ�����threadsList_Pool�нڵ��isRuning��ʱ���̳߳����̲߳�û�иı�threadsList_Pool�нڵ��isRuning��ֵ
  Ҳ����˵��ʱ�����threadsList_Pool�нڵ��isRuning��ʱ�򣬺��̳߳����̸߳ı�threadsList_Pool�нڵ��isRuning��ֵ ����һ��ʱ���

  ���㲻������״̬�����ǲ�ȡ����InitializeCriticalSection(&g_cs)��Ҳ�Ϳ��Աܿ�����"һ���ڶ�ȡisRuning,һ�����޸�isRuning"������
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
	if (num==listSize) // ��ʱ�˿����е��̶߳��ڱ�ʹ�ã����������̵߳�����
	{
		for (int i=0;i<10;i++)
		{
			struct threadInfo *threadNode=(struct threadInfo *)malloc(sizeof(struct threadInfo)); //�����߳������еĽڵ�
			threadNode->isRuning=false;

			HANDLE hThread;
			DWORD threadID;
			hThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)threadFun,(void *)threadNode,0,&threadID); //ʹ��windows api �����߳�
			//(void *)threadNode ���̵߳Ĳ���
			threadNode->hThread=hThread;
			threadsList_Pool.push_back(threadNode);

		}
	}//if (num==listSize) 

	//����ж����û�б�ʹ�õ��߳���ɾ��һ���������߳�
    //���磬�����10�������̣߳���ɾ��5�������߳�
	if ((listSize-num)>10)
	{
		for (item=threadsList_Pool.begin();item!=threadsList_Pool.end();)
		{
			if ((*(*item)).isRuning==false)
			{
				//�ݻٸ��߳�
				TerminateThread((*(*item)).hThread,1);
				CloseHandle((*(*item)).hThread);
				//�ͷŸýڵ���ռ�Ķ��ڴ�
				free(*item);
                //��lisg��ɾ���ýڵ�
                item = threadsList_Pool.erase(item);  //ɾ���ýڵ�

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
	//һ��ʼ������20���̣߳�ֻ��10�����������ʱɾ����5���̣߳�����ʣ�µ��߳���ĿӦ����15��
	printf("least  %d\n",threadsList_Pool.size());
}

DWORD CALLBACK ThreadProc(PVOID pvoid)
{
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE); 
	SetTimer(NULL, 10, 10000, TimeProc); //��������������ʱ����
	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(msg.message == WM_TIMER)
		{
			TranslateMessage(&msg);    // ������Ϣ
			DispatchMessage(&msg);     // �ַ���Ϣ
		}
	}
	KillTimer(NULL, 10);
	printf("thread end here\n");
	return 0;
}

/****************************************************/

/****************************************************/