#include <xmmintrin.h>
#include <stdio.h>

void set_test();

__declspec(align(16)) float A[] = { 2.0f, -1.0f, 3.0f, 4.0f };
__declspec(align(16)) float B[] = { -1.0f, 3.0f, 4.0f, 2.0f };
__declspec(align(16)) float C[] = { 1.2f, 1.3f, 1.4f, 1.5f };
__declspec(align(16)) float D[] = { 2.1f, 2.2f, 2.3f, 2.4f };

__m128 addWithAssembly(__m128 a, __m128 b)
{
	__m128 r;
	__asm{
		movaps xmm0, xmmword ptr[a]
		movaps xmm1, xmmword ptr[b]
		addps xmm0, xmm1;
		movaps xmmword ptr[r], xmm0
	}
	return r;
}

__m128 addWithIntrinsics(__m128 a,__m128 b)
{
	__m128 r = _mm_add_ps(a, b);
	return r;
}

int main()
{
	__m128 a = _mm_load_ps(&A[0]);
	__m128 b = _mm_load_ps(&B[0]);

	//2つの関数をテストする
	__m128 c = addWithAssembly(a, b);
	__m128 d = addWithIntrinsics(a, b);
	//結果を浮動小数点配列にストアして結果表示
	_mm_store_ps(&C[0], c);
	_mm_store_ps(&D[0], d);
	printf("%g %g %g %g \n", C[0], C[1], C[2], C[3]);
	printf("%g %g %g %g \n", D[0], D[1], D[2], D[3]);
	//set命令をテスト
	set_test();
	return 0;
}

/*
参考にすべきURL
http://wwweic.eri.u-tokyo.ac.jp/computer/manual/altix/compile/CC/Intel_Cdoc91/main_cls/mergedProjects/intref_cls/common/intref_sse_overview.htm

ストリーミング SIMD 拡張命令のロード操作
組み込み関数名	操作	対応する SSE 命令
_mm_loadh_pi	上位の値のロード	MOVHPS reg, mem
_mm_loadl_pi	下位の値のロード	MOVLPS reg, mem
_mm_load_ss		最下位の値をロードして、上位 3 つの値をクリアする	MOVSS
_mm_load1_ps	1 つの値を 4 ワードすべてにロードする	MOVSS + Shuffling
_mm_load_ps		4 つの値をロードする (アドレスのアライメントが合っていなければなりません)	MOVAPS
_mm_loadu_ps	4 つの値をロードする (アドレスのアライメントが合っている必要はありません)	MOVUPS
_mm_loadr_ps	4 つの値を逆順でロードする	MOVAPS + Shuffling

*/


void set_test()
{
	__m64 a = _mm_setzero_si64();
	printf("%d\n", a);
}

void load_test()
{
	//__m64  a = (__m64)123;

}