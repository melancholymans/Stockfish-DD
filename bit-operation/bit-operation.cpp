#include <iostream>
#include <cstdint>	//int64_t uint64_tを使うため
#include <intrin.h>
#include <immintrin.h>
#include <ammintrin.h>
#include "cpuid.h"

using namespace std;
/*
bit演算の基礎
KMC「プログラムを高速化する話」より参照

64bit変数のindexの定義
最上位bitは63bit
最下位bitは0bit
とする
*/

int main()
{
	//立っているbitの数を数える
	int64_t A = 184;		//1011 1000
	printf("%lld \n", A & -A);	//		8 3bit目が立っているので８となる
	//立っている一番下のbitをクリア
	printf("%lld\n", A & (A - 1));	//184-8=176
	//立っている一番下のbitより上の桁を全て１にする
	printf("%lld\n", A ^ (-A));	//		-16　符号付きの整数なのでマイナス値になる
	//上の桁と一番下位のbitを全て1にする
	printf("%lld\n", A | (-A));	//		-8　符号付きの整数なのでマイナス値になる
	//立っている一番下のbitより下の桁を全て１にする
	printf("%lld\n", A ^ (A-1));	//	15　符号付きの整数なのでマイナス値になる
	//立っているbit列を走査する
	//i &= i-1でiの立っている一番下のbitをクリア
	for (uint64_t i = (uint64_t)A; i != 0; i &= i - 1){
		printf("%lld\n", i);	
		/*
		10111000->184
		10110000->176
		10100000^>160
		10000000->128
		00000000->0	iが0となるので表示されない
		*/
	}
	//特定のbitを操作する命令
	
	//bts命令:特定のbitを立て、立てる前のそのbitの状態(0/1）を返す
	//https://msdn.microsoft.com/ja-jp/library/z56sc6y4.aspx
	printf("bts命令\n");
	long a = 184;
	long b = 6;
	unsigned char c;
	c = _bittestandset(&a, b);	//aの6bit目の状態(=0)を返して6bit目を１にする
	printf("%d\n", c);	//	0
	printf("%d\n", a);	//	184+64=248
	/*
	64bit版の関数でコンパイルするとエラーがでる。
	VC2013プロジェクトのプロパテイで全ての構成を選択しておき、プラットフォームをWin32からx64に変更する
	この時「新しいプロジクトプラットフォーム」ダイヤログがでたら「新しいプラットフォーム」プルダウンでx64を選択する
	この時「同じ名前のソリューションフラットフォームがすで存在します」とメッセージが出た場合は
	「新しいプロジクトプラットフォーム」ダイヤログの左下にある
	「新しいソリューションフラットフォームを作成する」というチエックマークをはずす
	*/
	__int64 a64 = 5026267842446548976;	//100 0101 1100 0000 1110 0011 1111 1000 1110 1110 1111 1110 1101 1111 1111 0000
	__int64 b64 = 33;
	_bittest64(&a64, b64);
	c = _bittestandset64(&a64, b64);	//aの6bit目の状態(=0)を返して6bit目を１にする
	printf("%d\n", c);	//	0
	printf("%lld\n", a64);	//	5026267842446548976+8,589,934,592=5026267851036483568
	
	//bt命令:特定のbitの状態(0/1）を返す
	printf("bt命令\n");
	a = 184; //01011 1000
	b = 7;
	c = _bittest(&a, b);
	printf("%d\n", c);	
	printf("%d\n", a);	//	184 変更はしないので184のまま
	a64 = 5026267842446548976;
	b64 = 33;
	c = _bittest64(&a64, b64);
	printf("%d\n", c);
	printf("%lld\n", a64);	

	//btr命令:特定のbitの状態(0/1)を返して0にセット
	printf("btr命令\n");
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

	//btc命令:特定のbitの状態(0/1)を返してbitを反転させる
	printf("btc命令\n");
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
	
	//blsi命令:立っている一番下のビットを求める
	printf("blsi命令\n");
	/*
	このWindows機のCPUには備わっていない命令セット(BMI1)なのか、コンパイルはとおるが
	中断する。
	http://en.wikipedia.org/wiki/Bit_Manipulation_Instruction_Sets#BMI2_.28Bit_Manipulation_Instruction_Set_2.29
	https://msdn.microsoft.com/ja-jp/library/hh977022.aspx
	*/
	/*
	unsigned int bit_test = 184;
	unsigned int bits;
	bits = _blsi_u32(bit_test);
	printf("bits = %d\n", bits);

	64bit版の命令はこちら
	unsigned __int64 _blsi_u64(unsigned __int64)
	*/

	//blsmsk命令:立っている一番下のbitより上のbitを１にした数を求める
	printf("blsmsk命令\n");
	/*
	このWindows機のCPUには備わっていない命令セット(BMI1)のようです。
	unsigned int _blsmsk_u32(unsigned int)
	unsigned __int64 _blsmsk_u64(unsigned __int64)
	*/

	//blsr命令:。立っている一番下のビットを1にした数を求める
	printf("blsr命令\n");
	/*
	このWindows機のCPUには備わっていない命令セット(BMI1)のようです。
	unsigned int _blsr_u32(unsigned int)
	unsigned __int64 _blsr_u64(unsigned __int64)
	*/

	//tzcnt命令:立っている一番下のビットの桁数を求める
	/*
	このWindows機のCPUには備わっていない命令セット(BMI1)のようです。
	unsigned int _tzcnt_u32(unsigned int)
	unsigned __int64 _tzcnt_u64(unsigned __int64)
	*/

	//lzcnt命令:立っている一番上のビットの上にある0の数を数える
	printf("lzcnt命令\n");
	/*
	まともな結果が返ってこないのでCPUに命令セットがないのでは
	サポートされていない
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

	//bzhi命令:
	/*
	サポートされていない
	*/
	//bextr命令
	/*
	サポートされていない
	*/
	//pext命令
	/*
	サポートされていない
	*/
	//pdep命令
	/*
	サポートされていない
	*/
	//SIMD命令のサポート状況
	/*
	SSE4.2まではサポート
	AVX命令:256bitが一度に実行できる命令セット
	*/
	//このWindows機にサポートされている機能
	//Microsoftがサポートしているx64 (amd64) 組み込み関数一覧
	//https://msdn.microsoft.com/ja-jp/library/hh977022.aspx
	cpuid();
	/*
	GenuineIntel
	Intel(R) Core(TM) i7 CPU       X 980  @ 3.33GHz
		3DNOW not supported
3DNOWEXT not supported
		ABM not supported			->この機能はサポートされていない
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
		BMI1 not supported			->この機能もサポートされていない
			BMI1 (Bit Manipulation Instruction Set 1)[edit]
			The instructions below are those enabled by the BMI bit in CPUID. Intel officially considers LZCNT as part of BMI, but advertises LZCNT support using the ABM CPUID feature flag.[3] BMI1 is available in AMD's Jaguar,[5] Piledriver[6] and newer processors, and in Intel's Haswell[7] and newer processors.

			Instruction	Description[3]	Equivalent C expression[8]
			ANDN	Logical and not	~x & y
			BEXTR	Bit field extract (with register)	(src >> start) & ((1 << len)-1)[9]
			BLSI	Extract lowest set isolated bit	x & -x
			BLSMSK	Get mask up to lowest set bit	x ^ (x - 1)
			BLSR	Reset lowest set bit	x & (x - 1)
			TZCNT	Count the number of trailing zero bits	N/A

		BMI2 not supported			->この機能もサポートされていない
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
		TBM not supported			->この機能もサポートされていない
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