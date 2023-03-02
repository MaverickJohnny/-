#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>
#include"primem.h"
int smallsize = 0;
int largesize = 0;
/*全局变量定义，有内存块管理结构，信号量，和flag*/
SmallBlockList* Block16List;/*用于记录16kb内存块的结构指针，存放16kb小内存块的使用信息，地址，数量*/
LargeBlockList* Block256List; /*用于记录256kb内存块的结构指针，存放256kb小内存块的使用信息，地址，数量*/

/*三种二值信号量定义，用于分配内存*/
SEM_ID Sem16;
SEM_ID Sem256;
SEM_ID SemHeap;

/*预分配静态内存池不够时，申请堆内存所需要用的内存，将其用链表穿起来*/
HeapApplyBlock* p_head = NULL;
HeapApplyBlock* p_tail = NULL;

/*判断是否已经预分配完内存池 */
int allocated = 0;

/*计算每一块内存块的首地址，applyAddr为申请的一大块内存的首地址*/
void GetBaseAddr(int* applyAddr) {

	/*blockindex是块地址，用于后边记录个数*/
	int BlockIndex = 0;
	/*smallblockcal和Large用来计算申请的地址，首地址开始挨个往后加就可以了*/
	SmallBlock16* SmallBlockCal;
	LargeBlock256* LargeBlockCal;

	/*小数据块管理结构指向申请到的首地址*/
	Block16List = applyAddr;

	Block256List = Block16List + 1;
	/*第一个小内存块的首地址，由一个小内存块指针指向 */
	SmallBlockCal = Block256List + 1;
	/*第一个大内存块的首地址，由一个小内存块指针指向，这里注意要加上小内存块数量，越过小内存块区 */
	LargeBlockCal = SmallBlockCal + SmallBlockNum + 1;


	/*初始化小内存块链表*/
	Block16List->baseAddr = SmallBlockCal;
	Block16List->busyNum = 0;

	/*初始化大内存块链表*/
	Block256List->baseAddr = LargeBlockCal;
	Block256List->busyNum = 0;


	for (BlockIndex = 0; BlockIndex < SmallBlockNum; BlockIndex++) {
		/*初始化，将链表中的每一个小内存块置为可用的状态 */
		Block16List->busy[BlockIndex] = Available;
		/*打印每一个内存块的地址*/
		printf("第 %d 个小内存块地址：%d\n", BlockIndex + 1, Block16List->baseAddr + BlockIndex);
	}

	for (BlockIndex = 0; BlockIndex < LargeBlockNum; BlockIndex++) {
		/*同上 */
		Block256List->busy[BlockIndex] = Available;
		/*打印每一个内存块的地址*/
		printf("第 %d 个大内存块地址：%d\n", BlockIndex + 1, Block256List->baseAddr + BlockIndex);
	}

	printf("小内存管理块的首地址：%08dD,%08xH\n", Block16List, Block16List);
	printf("大内存管理块的首地址：%08dD,%08xH\n", Block256List, Block256List);
	printf("小内存块数组的首地址：%08dD,%08xH\n", Block16List->baseAddr, Block16List->baseAddr);
	printf("大内存块数组的首地址：%08dD,%08xH\n", Block256List->baseAddr, Block256List->baseAddr);

}

/*内存初始化，创建信号量，并计算要申请的内存块一共多少个，每个大小为多少，返回内存首地址*/
int* MemInit() {
	/*定义局部变量大小*/
	int SmallSize;
	int LargeSize;
	int ListSize;
	int TotalSize;
	/*基地址*/
	int* HeadAddr;
	/*信号量初始化*/
	Sem16 = semBCreate(SEM_Q_FIFO, SEM_FULL);
	Sem256 = semBCreate(SEM_Q_FIFO, SEM_FULL);
	SemHeap = semBCreate(SEM_Q_FIFO, SEM_FULL);

	/*判断是不是进行过预分配了*/
	if (allocated > 0) {
		printf("已经完成内存管理初始化。\n");
		return;
	}


	/*计算首先要申请多少字节的内存地址*/
	/*小内存块总共要花费的字节*/
	SmallSize = SmallBlockNum * sizeof(SmallBlock16);
	/*大内存块总共要花费的字节*/
	LargeSize = LargeBlockNum * sizeof(LargeBlock256);
	/*内存块管理链表总共要花费的字节*/
	ListSize = sizeof(SmallBlockList) + sizeof(LargeBlockList);
	/*总共空间*/
	TotalSize = SmallSize + LargeSize + ListSize;

	printf("小内存块数量：%d，每块占用%d字节，共%d字节\n", SmallBlockNum, sizeof(SmallBlock16), SmallSize);
	printf("大内存块数量：%d，每块占用%d字节，共%d字节\n", LargeBlockNum, sizeof(LargeBlock256), LargeSize);
	printf("小内存管理块占用%d字节\n", sizeof(SmallBlockList));
	printf("大内存管理块占用%d字节\n", sizeof(LargeBlockList));
	printf("初始化时总共需申请%d字节的内存区域\n", TotalSize);

	/*堆内存申请管理链表初始化*/
	p_head = (struct HeapApplyBlock*)malloc(sizeof(HeapApplyBlock));
	p_head->next = NULL;
	p_tail = p_head;

	/*HeadAddr是申请TotalSize大小的内存块返回的首地址*/
	/*这里正式进行大内存块的预分配申请*/
	HeadAddr = (int*)malloc(TotalSize);
	if (HeadAddr == NULL) {
		printf("不能成功分配存储空间。\n");
		return NULL;
	}
	else {
		allocated = 1;
		/*计算基地址，并打印前边每一个内存块地址*/
		GetBaseAddr(HeadAddr);

		printf("内存管理初始化完成，起始地址：%08dD , %08xH\n", HeadAddr, HeadAddr);

		return HeadAddr;
	}
}

/*按类型申请内存，返回申请到的内存块的首地址*/
int* MallocDependOnType(int type, int size) {

	/*
		按类型分配内存主要有三种类型，16kb小块，256kb大块，堆内存更大的块
		下面首先是索引，堆地址，以及三个标识符，用于判断三个类型分配到内存没有，然后决定对信号量的操作
	*/
	int index = 0;
	int* HeapAddr = NULL;
	int isSmallBlockAlloc = 0;
	int isLargeBlockAlloc = 0;
	int isHeapBlockAlloc = 0;
	/**/

	/*
		三种内存存储块结构局部定义，都是用于返回地址的temp块
	*/
	SmallBlock16* SmallBlockAddr = NULL;
	LargeBlock256* LargeBlockAddr = NULL;
	HeapApplyBlock* p_new = NULL;

	switch (type) {
	case SmallBlockFlag:
		/*获取信号量 互斥*/
		semTake(Sem16, WAIT_FOREVER);
		/*获得信号量，小块里有一个被用了 加1*/
		Block16List->busyNum++;
		/*用于返回地址的temp small block*/
		SmallBlockAddr = Block16List->baseAddr;
		for (index = 0; index < SmallBlockNum; index++) {
			if (Block16List->busy[index] == Available) {
				/*循环判断哪个块可用，然后释放置1为不可用，信号量释放*/
				Block16List->busy[index] = NotAvailable;
				semGive(Sem16);/*释放信号量 互斥*/
				isSmallBlockAlloc = 1;
				return SmallBlockAddr + index;
			}
		}
		if (isSmallBlockAlloc == 0)
			semGive(Sem16); /*如果没有分配到可用块，标识置0，这样也释放信号量*/
		break;
	case LargeBlockFlag:
		semTake(Sem256, WAIT_FOREVER);
		Block256List->busyNum++;
		LargeBlockAddr = Block256List->baseAddr;
		for (index = 0; index < LargeBlockNum; index++) {
			if (Block256List->busy[index] == Available) {
				Block256List->busy[index] = NotAvailable;
				semGive(Sem256);
				isLargeBlockAlloc = 1;
				return LargeBlockAddr + index;
			}
		}
		if (isLargeBlockAlloc == 0)
			semGive(Sem256);
		break;
	case HeapFlag:
		semTake(SemHeap, WAIT_FOREVER);
		p_new = (HeapApplyBlock*)malloc(sizeof(HeapApplyBlock));
		if (NULL == p_new) {
			printf("没在堆中申请到\n");
			semGive(SemHeap);
			return NULL;
		}
		HeapAddr = (int*)malloc(sizeof(size));
		p_new->heap_add = HeapAddr;
		p_new->size = size;
		p_new->next = NULL;

		p_tail->next = p_new;
		p_tail = p_new;
		semGive(SemHeap);
		return HeapAddr;

		break;
	default:
		break;
	}
}

/*
	申请内存块程序
	1.小于16kb的部分申请小内存块，如果小内存块区域满，则申请大内存块
	2.大于16kb小于256kb申请大内存块，如果大内存块区域满，则从堆申请内存块
	3.从堆申请内存块，连接到内存块链表上
*/
int* MemMalloc(int size) {

	/*
		三个标识符
		小块是不是满了，大块是不是满了，小块是否还可用
		Smallblockava和 full是相对的
	*/
	int isSBFULL = 0;
	int isLBFULL = 0;
	int flag = SmallBlockAva;


	if (size > 0 && size <= SmallBlockSize) {
		printf("正在申请小内存块...\n");
		isSBFULL = Block16List->busyNum;
		if (isSBFULL == SmallBlockNum) {
			printf("小内存块满...\n");
			flag = SmallBlockFull;
		}
		else {
			return MallocDependOnType(SmallBlockFlag, size);
		}
	}
	if ((size > SmallBlockSize && size <= LargeBlockSize) || flag == SmallBlockFull) {
		printf("正在申请大内存块...\n");
		isLBFULL = Block16List->busyNum;
		if (isLBFULL == LargeBlockNum) {
			printf("大内存块满...\n");
			flag = LargeBlockFull;
		}
		else {
			return MallocDependOnType(LargeBlockFlag, size);
		}
	}
	if (size > LargeBlockSize || flag == LargeBlockFull) {
		printf("正在从堆申请内存块...\n");
		return MallocDependOnType(HeapFlag, size);
	}
}

/*	首先检验地址合法性，不合法return;
	合法判断是三条链中哪一条链上的内存块，执行free操作 */
void MemFree(int free_add) {

	/*这里判断属于小内存块中的地，使用十进制比较即可*/
	if (free_add >= (*Block16List).baseAddr && free_add <= (*Block16List).baseAddr + SmallBlockNum - 1) {
		int index = 0;

		SmallBlock16* SmallBlockAddr = (*Block16List).baseAddr;

		for (index = 0; index < SmallBlockNum; index++) {
			if (SmallBlockAddr + index == free_add) {
				(*Block16List).busyNum--;
				(*Block16List).busy[index] = Available;
				printf("已成功释放一块小内存\n");
			}
		}
	}/*这里判断属于大内存块中的地，使用十进制比较即可*/
	else if (free_add >= (*Block256List).baseAddr && free_add <= (*Block256List).baseAddr + LargeBlockNum - 1) {
		int index = 0;
		LargeBlock256* LargeBlockAddr = (*Block256List).baseAddr;
		for (index = 0; index < LargeBlockNum; index++) {
			if (LargeBlockAddr + index == free_add) {
				(*Block256List).busyNum--;
				(*Block256List).busy[index] = Available;
				printf("已成功释放一块大内存\n");
			}
		}
	}

	/*	剩下的内存就是在堆里申请的了，这里判断是否是从堆中申请的，在链表中依次释放内存块即可
		如果是，则释放，否则提示输入地址无效*/
	else if (p_head->next != NULL) {
		HeapApplyBlock* p = p_head;
		HeapApplyBlock* p_temp = NULL;
		while (p->next != NULL)
		{
			p_temp = p->next;
			if (p_temp->heap_add == free_add) {
				free(free_add);
				p->next = p_temp->next;
				free(p_temp);
				printf("已成功释放一块从堆中申请的内存\n");
				return;
			}
			else {
				p = p->next;
			}
		}
		printf("该首地址无效。\n");
	}
	else {
		printf("该首地址无效。\n");
	}
}

/*释放记录从堆申请的内存块的链表*/
void HeapListFree() {
	HeapApplyBlock* p = p_head;
	HeapApplyBlock* p_temp = NULL;
	while (p->next != NULL) {
		p_temp = p->next;
		free(p_temp->heap_add);
		p->next = p_temp->next;
		free(p_temp);
		printf("已完全释放申请的堆内存\n");
	}
}

/*管理监视机制*/
void ShowMem() {
	double f1;
	double f2;
	int index;
	HeapApplyBlock* p = p_head->next;
	if (allocated == 0) {
		printf("程序未初始化分配\n");
		return;
	}
	else if (allocated > 0) {
		printf("\n小内存块情况:\n");

		for (index = 0; index < SmallBlockNum; index++) {
			if ((*Block16List).busy[index] == Available)
				printf("第%d块:Available\t", index + 1);
			if ((*Block16List).busy[index] == NotAvailable)
				printf("第%d块:hasUsed\t", index + 1);
			if ((index + 1) % 4 == 0)printf("\n");
		}
		printf("\n");
		printf("大内存块情况:\n");
		for (index = 0; index < LargeBlockNum; index++) {
			if ((*Block256List).busy[index] == Available)
				printf("第%d块:Available\t", index + 1);
			if ((*Block256List).busy[index] == NotAvailable)
				printf("第%d块:hasUsed\t", index + 1);
			if ((index + 1) % 4 == 0)printf("\n");
		}
		printf("\n");


		if (p_head->next == NULL) {
			printf("未申请堆内存\n");
		}
		else {
			printf("堆：\n");
			while (p != NULL)
			{
				printf("首地址：%08dD，首地址：%08xH，占用：%d字节\n", p->heap_add, p->heap_add, p->size);
				p = p->next;
			}
		}
	}
	else {
		printf("内存初始化出现问题！\n");
	}
}

/*选择*/
void showlist() {
	printf("请选择输入：\n");
	printf("1.开始申请内存\n");
	printf("2.开始释放内存\n");
	printf("3.内存占用记录及显示\n");
	printf("0.退出\n");
	printf("\n");
}

/*主程序入口*/
void main() {
	int* applyAddr;
	int malloc_size;
	int* add;
	int free_add;
	int choice;
	/*制0避免重复*/
	allocated = 0;
	printf("程序已运行，现在执行初始化和内存预分配\n");
	printf("\n");
	applyAddr = MemInit();/*记录初始化时的首地址，程序结束时进行free */
	printf("\n内存预分配与初始化已完成\n");
	showlist();

	choice = -1;
	scanf("%d", &choice);

	while (choice != 0) {
		switch (choice) {
		case 1:
			printf("申请字节为？\n");
			scanf("%d", &malloc_size);
			add = MemMalloc(malloc_size);
			if (add != NULL) {
				printf("申请到的地址为%08dD,%08xH\n", add, add);
			}
			else {
				printf("申请不到内存空间\n");
			}
			break;
		case 2:
			printf("输入内存地址：");
			free_add = -1;
			scanf("%d", &free_add);
			MemFree(free_add);
			break;
		case 3:
			ShowMem();
			break;
		default:
			break;
		}
		showlist();
		scanf("%d", &choice);
	}

	free(applyAddr);/*释放大内存块，小内存块 */

	HeapListFree();/*释放存放heap内存块地址的链表 */
	free(p_head);/*释放链表头 */

	return 0;
}
