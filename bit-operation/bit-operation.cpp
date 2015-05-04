#include <iostream>
#include <cstdint>	//int64_t uint64_t���g������
#include <intrin.h>
#include <immintrin.h>
#include <ammintrin.h>
#include "cpuid.h"

using namespace std;
/*
bit���Z�̊�b
KMC�u�v���O����������������b�v���Q��

64bit�ϐ���index�̒�`
�ŏ��bit��63bit
�ŉ���bit��0bit
�Ƃ���
*/

int main()
{
	//�����Ă���bit�̐��𐔂���
	int64_t A = 184;		//1011 1000
	printf("%lld \n", A & -A);	//		8 3bit�ڂ������Ă���̂łW�ƂȂ�
	//�����Ă����ԉ���bit���N���A
	printf("%lld\n", A & (A - 1));	//184-8=176
	//�����Ă����ԉ���bit����̌���S�ĂP�ɂ���
	printf("%lld\n", A ^ (-A));	//		-16�@�����t���̐����Ȃ̂Ń}�C�i�X�l�ɂȂ�
	//��̌��ƈ�ԉ��ʂ�bit��S��1�ɂ���
	printf("%lld\n", A | (-A));	//		-8�@�����t���̐����Ȃ̂Ń}�C�i�X�l�ɂȂ�
	//�����Ă����ԉ���bit��艺�̌���S�ĂP�ɂ���
	printf("%lld\n", A ^ (A-1));	//	15�@�����t���̐����Ȃ̂Ń}�C�i�X�l�ɂȂ�
	//�����Ă���bit��𑖍�����
	//i &= i-1��i�̗����Ă����ԉ���bit���N���A
	for (uint64_t i = (uint64_t)A; i != 0; i &= i - 1){
		printf("%lld\n", i);	
		/*
		10111000->184
		10110000->176
		10100000^>160
		10000000->128
		00000000->0	i��0�ƂȂ�̂ŕ\������Ȃ�
		*/
	}
	//�����bit�𑀍삷�閽��
	
	//bts����:�����bit�𗧂āA���Ă�O�̂���bit�̏��(0/1�j��Ԃ�
	//https://msdn.microsoft.com/ja-jp/library/z56sc6y4.aspx
	printf("bts����\n");
	long a = 184;
	long b = 6;
	unsigned char c;
	c = _bittestandset(&a, b);	//a��6bit�ڂ̏��(=0)��Ԃ���6bit�ڂ��P�ɂ���
	printf("%d\n", c);	//	0
	printf("%d\n", a);	//	184+64=248
	/*
	64bit�ł̊֐��ŃR���p�C������ƃG���[���ł�B
	VC2013�v���W�F�N�g�̃v���p�e�C�őS�Ă̍\����I�����Ă����A�v���b�g�t�H�[����Win32����x64�ɕύX����
	���̎��u�V�����v���W�N�g�v���b�g�t�H�[���v�_�C�����O���ł���u�V�����v���b�g�t�H�[���v�v���_�E����x64��I������
	���̎��u�������O�̃\�����[�V�����t���b�g�t�H�[�������ő��݂��܂��v�ƃ��b�Z�[�W���o���ꍇ��
	�u�V�����v���W�N�g�v���b�g�t�H�[���v�_�C�����O�̍����ɂ���
	�u�V�����\�����[�V�����t���b�g�t�H�[�����쐬����v�Ƃ����`�G�b�N�}�[�N���͂���
	*/
	__int64 a64 = 5026267842446548976;	//100 0101 1100 0000 1110 0011 1111 1000 1110 1110 1111 1110 1101 1111 1111 0000
	__int64 b64 = 33;
	_bittest64(&a64, b64);
	c = _bittestandset64(&a64, b64);	//a��6bit�ڂ̏��(=0)��Ԃ���6bit�ڂ��P�ɂ���
	printf("%d\n", c);	//	0
	printf("%lld\n", a64);	//	5026267842446548976+8,589,934,592=5026267851036483568
	
	//bt����:�����bit�̏��(0/1�j��Ԃ�
	printf("bt����\n");
	a = 184; //01011 1000
	b = 7;
	c = _bittest(&a, b);
	printf("%d\n", c);	
	printf("%d\n", a);	//	184 �ύX�͂��Ȃ��̂�184�̂܂�
	a64 = 5026267842446548976;
	b64 = 33;
	c = _bittest64(&a64, b64);
	printf("%d\n", c);
	printf("%lld\n", a64);	

	//btr����:�����bit�̏��(0/1)��Ԃ���0�ɃZ�b�g
	printf("btr����\n");
	a = 184; //01011 1000
	b = 7;
	c = _bittestandreset(&a, b);
	printf("%d\n", c);
	printf("%d\n", a);	//	184-128=56
	a64 = 5026267842446548976;
	b64 = 33;
	c = _bittestandreset64(&a64, b64);
	printf("%d\n", c);
	printf("%lld\n", a64);	

	//btc����:�����bit�̏��(0/1)��Ԃ���bit�𔽓]������
	printf("btc����\n");
	a = 184; //1011 1000
	b = 7;
	c = _bittestandcomplement(&a, b);	
	printf("%d\n", c);
	printf("%d\n", a);	//0011 1000->56
	a64 = 5026267842446548976;
	b64 = 33;
	c = _bittestandcomplement64(&a64, b64);
	printf("%d\n", c);	
	printf("%lld\n", a64);	
	
	//blsi����:�����Ă����ԉ��̃r�b�g�����߂�
	printf("blsi����\n");
	/*
	����Windows�@��CPU�ɂ͔�����Ă��Ȃ����߃Z�b�g(BMI1)�Ȃ̂��A�R���p�C���͂Ƃ��邪
	���f����B
	http://en.wikipedia.org/wiki/Bit_Manipulation_Instruction_Sets#BMI2_.28Bit_Manipulation_Instruction_Set_2.29
	https://msdn.microsoft.com/ja-jp/library/hh977022.aspx
	*/
	/*
	unsigned int bit_test = 184;
	unsigned int bits;
	bits = _blsi_u32(bit_test);
	printf("bits = %d\n", bits);

	64bit�ł̖��߂͂�����
	unsigned __int64 _blsi_u64(unsigned __int64)
	*/

	//blsmsk����:�����Ă����ԉ���bit�����bit���P�ɂ����������߂�
	printf("blsmsk����\n");
	/*
	����Windows�@��CPU�ɂ͔�����Ă��Ȃ����߃Z�b�g(BMI1)�̂悤�ł��B
	unsigned int _blsmsk_u32(unsigned int)
	unsigned __int64 _blsmsk_u64(unsigned __int64)
	*/

	//blsr����:�B�����Ă����ԉ��̃r�b�g��1�ɂ����������߂�
	printf("blsr����\n");
	/*
	����Windows�@��CPU�ɂ͔�����Ă��Ȃ����߃Z�b�g(BMI1)�̂悤�ł��B
	unsigned int _blsr_u32(unsigned int)
	unsigned __int64 _blsr_u64(unsigned __int64)
	*/

	//tzcnt����:�����Ă����ԉ��̃r�b�g�̌��������߂�
	/*
	����Windows�@��CPU�ɂ͔�����Ă��Ȃ����߃Z�b�g(BMI1)�̂悤�ł��B
	unsigned int _tzcnt_u32(unsigned int)
	unsigned __int64 _tzcnt_u64(unsigned __int64)
	*/

	//lzcnt����:�����Ă����ԏ�̃r�b�g�̏�ɂ���0�̐��𐔂���
	printf("lzcnt����\n");
	/*
	�܂Ƃ��Ȍ��ʂ��Ԃ��Ă��Ȃ��̂�CPU�ɖ��߃Z�b�g���Ȃ��̂ł�
	�T�|�[�g����Ă��Ȃ�
	*/
	unsigned short x16;
	unsigned short a16 = 184;
	x16 = __lzcnt16(a16);
	printf("count(16)=%d\n", x16);

	unsigned int x32;
	unsigned int a32 = 184;
	x32 = __lzcnt(a32);
	printf("count=%d\n",x32);

	unsigned __int64 x64;
	a64 = 184;
	x64 = __lzcnt64(a64);
	printf("count(64)=%lld\n", x64);

	//bzhi����:
	/*
	�T�|�[�g����Ă��Ȃ�
	*/
	//bextr����
	/*
	�T�|�[�g����Ă��Ȃ�
	*/
	//pext����
	/*
	�T�|�[�g����Ă��Ȃ�
	*/
	//pdep����
	/*
	�T�|�[�g����Ă��Ȃ�
	*/
	//SIMD���߂̃T�|�[�g��
	/*
	SSE4.2�܂ł̓T�|�[�g
	AVX����:256bit����x�Ɏ��s�ł��閽�߃Z�b�g
	*/
	//����Windows�@�ɃT�|�[�g����Ă���@�\
	//Microsoft���T�|�[�g���Ă���x64 (amd64) �g�ݍ��݊֐��ꗗ
	//https://msdn.microsoft.com/ja-jp/library/hh977022.aspx
	cpuid();
	/*
	GenuineIntel
	Intel(R) Core(TM) i7 CPU       X 980  @ 3.33GHz
		3DNOW not supported
3DNOWEXT not supported
		ABM not supported			->���̋@�\�̓T�|�[�g����Ă��Ȃ�
			//http://en.wikipedia.org/wiki/Bit_Manipulation_Instruction_Sets#BMI2_.28Bit_Manipulation_Instruction_Set_2.29
			ABM (Advanced Bit Manipulation)[edit]
			ABM is only implemented as a single instruction set by AMD; all AMD processors support both instructions or neither. Intel considers POPCNT as part of SSE4.2, and LZCNT as part of BMI1. POPCNT has a separate CPUID flag; however, Intel uses AMD's ABM flag to indicate LZCNT support (since LZCNT completes the ABM).[3]

			Instruction	Description[4]
			POPCNT	Population count
			LZCNT	Leading zeros count

		ADX not supported
AES supported
		AVX not supported
		AVX2 not supported
		AVX512CD not supported
		AVX512ER not supported
		AVX512F not supported
		AVX512PF not supported
		BMI1 not supported			->���̋@�\���T�|�[�g����Ă��Ȃ�
			BMI1 (Bit Manipulation Instruction Set 1)[edit]
			The instructions below are those enabled by the BMI bit in CPUID. Intel officially considers LZCNT as part of BMI, but advertises LZCNT support using the ABM CPUID feature flag.[3] BMI1 is available in AMD's Jaguar,[5] Piledriver[6] and newer processors, and in Intel's Haswell[7] and newer processors.

			Instruction	Description[3]	Equivalent C expression[8]
			ANDN	Logical and not	~x & y
			BEXTR	Bit field extract (with register)	(src >> start) & ((1 << len)-1)[9]
			BLSI	Extract lowest set isolated bit	x & -x
			BLSMSK	Get mask up to lowest set bit	x ^ (x - 1)
			BLSR	Reset lowest set bit	x & (x - 1)
			TZCNT	Count the number of trailing zero bits	N/A

		BMI2 not supported			->���̋@�\���T�|�[�g����Ă��Ȃ�
			BMI2 (Bit Manipulation Instruction Set 2)[edit]
			Intel introduced BMI2 together with BMI1 in its line of Haswell processors. Only AMD has produced processors supporting only BMI1 without BMI2; support for BMI2 is planned for AMD's next architecture, Excavator.[10]

			Instruction	Description
			BZHI	Zero high bits starting with specified bit position
			MULX	Unsigned multiply without affecting flags, and arbitrary destination registers
			PDEP	Parallel bits deposit
			PEXT	Parallel bits extract
			RORX	Rotate right logical without affecting flags
			SARX	Shift arithmetic right without affecting flags
			SHRX	Shift logical right without affecting flags
			SHLX	Shift logical left without affecting flags

CLFSH supported
CMPXCHG16B supported
CX8 supported
ERMS not supported
F16C not supported
FMA not supported
FSGSBASE not supported
FXSR supported
HLE not supported
INVPCID not supported
LAHF supported
LZCNT not supported
MMX supported
MMXEXT not supported
MONITOR supported
MOVBE not supported
MSR supported
		OSXSAVE not supported
PCLMULQDQ supported
POPCNT supported
		PREFETCHWT1 not supported
		RDRAND not supported
		RDSEED not supported
RDTSCP supported
		RTM not supported
SEP supported
		SHA not supported
SSE supported
SSE2 supported
SSE3 supported
SSE4.1 supported
SSE4.2 supported
		SSE4a not supported
SSSE3 supported
SYSCALL supported
		TBM not supported			->���̋@�\���T�|�[�g����Ă��Ȃ�
			TBM (Trailing Bit Manipulation)[edit]
			TBM consists of instructions complementary to the instruction set started by BMI1; their complementary nature means they do not necessarily need to be used directly but can be generated by an optimizing compiler when supported.[13] AMD introduced TBM together with BMI1 in its Piledriver[6] line of processors; AMD Jaguar processors do not support TBM.[5]

			Instruction	Description[4]	Equivalent C expression[14]
			BEXTR	Bit field extract (with immediate)	(src >> start) & ((1 << len)-1)
			BLCFILL	Fill from lowest clear bit	x & (x + 1)
			BLCI	Isolate lowest clear bit	x | ~(x + 1)
			BLCIC	Isolate lowest clear bit and complement	~x & (x + 1)
			BLCMASK	Mask from lowest clear bit	x ^ (x + 1)
			BLCS	Set lowest clear bit	x | (x + 1)
			BLSFILL	Fill from lowest set bit	x | (x - 1)
			BLSIC	Isolate lowest set bit and complement	~x | (x - 1)
			T1MSKC	Inverse mask from trailing ones	~x | (x + 1)
			TZMSK	Mask from trailing zeros	~x & (x - 1)

		XOP not supported
		XSAVE not supported
	*/
	getchar();
	return 0;
}