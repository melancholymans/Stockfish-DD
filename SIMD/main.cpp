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

	//2�̊֐����e�X�g����
	__m128 c = addWithAssembly(a, b);
	__m128 d = addWithIntrinsics(a, b);
	//���ʂ𕂓������_�z��ɃX�g�A���Č��ʕ\��
	_mm_store_ps(&C[0], c);
	_mm_store_ps(&D[0], d);
	printf("%g %g %g %g \n", C[0], C[1], C[2], C[3]);
	printf("%g %g %g %g \n", D[0], D[1], D[2], D[3]);
	//set���߂��e�X�g
	set_test();
	return 0;
}

/*
�Q�l�ɂ��ׂ�URL
http://wwweic.eri.u-tokyo.ac.jp/computer/manual/altix/compile/CC/Intel_Cdoc91/main_cls/mergedProjects/intref_cls/common/intref_sse_overview.htm

�X�g���[�~���O SIMD �g�����߂̃��[�h����
�g�ݍ��݊֐���	����	�Ή����� SSE ����
_mm_loadh_pi	��ʂ̒l�̃��[�h	MOVHPS reg, mem
_mm_loadl_pi	���ʂ̒l�̃��[�h	MOVLPS reg, mem
_mm_load_ss		�ŉ��ʂ̒l�����[�h���āA��� 3 �̒l���N���A����	MOVSS
_mm_load1_ps	1 �̒l�� 4 ���[�h���ׂĂɃ��[�h����	MOVSS + Shuffling
_mm_load_ps		4 �̒l�����[�h���� (�A�h���X�̃A���C�����g�������Ă��Ȃ���΂Ȃ�܂���)	MOVAPS
_mm_loadu_ps	4 �̒l�����[�h���� (�A�h���X�̃A���C�����g�������Ă���K�v�͂���܂���)	MOVUPS
_mm_loadr_ps	4 �̒l���t���Ń��[�h����	MOVAPS + Shuffling

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