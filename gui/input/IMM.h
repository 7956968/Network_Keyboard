
#ifndef __IMM_H__
#define __IMM_H__

#include "File.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


typedef int			VD_BOOL;
typedef const char* VD_PCSTR;

typedef struct _HZMB_HEAD{
	int sec_num;		//������
	int hz_num;			//������
}HZMB_HEAD;

typedef struct _HZMB_SECTOR{
	char str[8];		//�ؼ����ַ���,������ƴ��,ע��,�ʻ��ȵ�
	int offset;			//�������ݿ�ʼƫ��
	int next;			//��һ�����εĽڵ��
}HZMB_SECTOR;

class CIMM
{
public:
	CIMM();
	~CIMM();

	VD_BOOL Open(VD_PCSTR table);
	void Close();
	int Filter(VD_PCSTR key);
	VD_PCSTR GetChar(int offset);

private:
	CFile m_FileTable;
	HZMB_HEAD* m_pHead;
	HZMB_SECTOR* m_pSectors;
	char* m_pChars;

	int m_nStartPos;
	int m_nEndPos;
};

#endif //__IMM_H__
