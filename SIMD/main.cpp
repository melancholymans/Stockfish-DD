#include <xmmintrin.h>
#include <stdio.h>

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
	return 0;
}
