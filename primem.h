/*小内存块数据域的大小 */
#define SmallBlockSize 50
/*小内存块的个数 */
#define SmallBlockNum 20
/*大内存块数据域的大小 */
#define LargeBlockSize 1000
/*大内存块的个数 */
#define LargeBlockNum 4

 /*从小内存块区域申请 */
#define SmallBlockFlag 1
 /*从大内存块区域申请 */
#define LargeBlockFlag 2
/*从heap内存块区域申请 */
#define HeapFlag 3

/*标记块目前是否可用*/
#define Available 0

#define NotAvailable 1

/*小内存块可用 */
#define SmallBlockAva 0
/*小内存块满 ，转为申请大内存块 */
#define SmallBlockFull 1
/*大内存块满 ，转为申请heap内存块 */
#define LargeBlockFull 2

/*小内存块结构*/


typedef struct {
	char data[SmallBlockSize];/*存放data域*/
}SmallBlock16;

/*大内存块结构 */

typedef struct {
	char data[LargeBlockSize];/*存放data域*/
}LargeBlock256;

/*小内存块管理结构*/

typedef struct {
	SmallBlock16* baseAddr;/*小内存块的基地址*/
	int busyNum;/*被使用的数量 */
	char busy[SmallBlockNum];/*每个小内存块的使用情况*/
}SmallBlockList;

/*大内存块管理结构*/
typedef struct {
	LargeBlock256* baseAddr;/*大内存块的基地址*/
	int busyNum;/*被使用的数量 */
	char busy[LargeBlockNum]; /*每个大内存块的使用情况*/
}LargeBlockList;


typedef struct {
	int* heap_add;/*记录从heap申请的内存块的地址 */
	int size;/*该首地址的内存区占用多少字节 */
	struct HeapApplyBlock* next;/*指向下一个内存区域 */
}HeapApplyBlock;
