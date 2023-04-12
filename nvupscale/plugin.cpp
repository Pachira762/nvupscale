#include <tuple>
#include <Windows.h>
#include "filter.h"

PROCESS_INFORMATION pi{};
HANDLE pipe = INVALID_HANDLE_VALUE;

extern "C" FILTER_DLL * __stdcall GetFilterTable() {
	static constexpr int NumTrackbars = 2;
	static TCHAR* trackbar_names[NumTrackbars] = { const_cast<TCHAR*>("Denoise"), const_cast<TCHAR*>("Scale")};
	static int trackbar_initials[NumTrackbars] = { 0, 0 };
	static int trackbar_mins[NumTrackbars] = { -1, -1 };
	static int trackbar_maxs[NumTrackbars] = { 4, 4 };

	static FILTER_DLL filter = {
		FILTER_FLAG_EX_INFORMATION,	//	�t�B���^�̃t���O
		//	FILTER_FLAG_ALWAYS_ACTIVE		: �t�B���^����ɃA�N�e�B�u�ɂ��܂�
		//	FILTER_FLAG_CONFIG_POPUP		: �ݒ���|�b�v�A�b�v���j���[�ɂ��܂�
		//	FILTER_FLAG_CONFIG_CHECK		: �ݒ���`�F�b�N�{�b�N�X���j���[�ɂ��܂�
		//	FILTER_FLAG_CONFIG_RADIO		: �ݒ�����W�I�{�^�����j���[�ɂ��܂�
		//	FILTER_FLAG_EX_DATA				: �g���f�[�^��ۑ��o����悤�ɂ��܂��B
		//	FILTER_FLAG_PRIORITY_HIGHEST	: �t�B���^�̃v���C�I���e�B����ɍŏ�ʂɂ��܂�
		//	FILTER_FLAG_PRIORITY_LOWEST		: �t�B���^�̃v���C�I���e�B����ɍŉ��ʂɂ��܂�
		//	FILTER_FLAG_WINDOW_THICKFRAME	: �T�C�Y�ύX�\�ȃE�B���h�E�����܂�
		//	FILTER_FLAG_WINDOW_SIZE			: �ݒ�E�B���h�E�̃T�C�Y���w��o����悤�ɂ��܂�
		//	FILTER_FLAG_DISP_FILTER			: �\���t�B���^�ɂ��܂�
		//	FILTER_FLAG_EX_INFORMATION		: �t�B���^�̊g������ݒ�ł���悤�ɂ��܂�
		//	FILTER_FLAG_NO_CONFIG			: �ݒ�E�B���h�E��\�����Ȃ��悤�ɂ��܂�
		//	FILTER_FLAG_AUDIO_FILTER		: �I�[�f�B�I�t�B���^�ɂ��܂�
		//	FILTER_FLAG_RADIO_BUTTON		: �`�F�b�N�{�b�N�X�����W�I�{�^���ɂ��܂�
		//	FILTER_FLAG_WINDOW_HSCROLL		: �����X�N���[���o�[�����E�B���h�E�����܂�
		//	FILTER_FLAG_WINDOW_VSCROLL		: �����X�N���[���o�[�����E�B���h�E�����܂�
		//	FILTER_FLAG_IMPORT				: �C���|�[�g���j���[�����܂�
		//	FILTER_FLAG_EXPORT				: �G�N�X�|�[�g���j���[�����܂�
		0,0,						//	�ݒ�E�C���h�E�̃T�C�Y (FILTER_FLAG_WINDOW_SIZE�������Ă��鎞�ɗL��)
		const_cast<TCHAR*>("nvupscale"),			//	�t�B���^�̖��O
		NumTrackbars,					//	�g���b�N�o�[�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
		trackbar_names,					//	�g���b�N�o�[�̖��O�S�ւ̃|�C���^
		trackbar_initials,				//	�g���b�N�o�[�̏����l�S�ւ̃|�C���^
		trackbar_mins, trackbar_maxs,			//	�g���b�N�o�[�̐��l�̉������ (NULL�Ȃ�S��0�`256)
		0,					//	�`�F�b�N�{�b�N�X�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
		nullptr,					//	�`�F�b�N�{�b�N�X�̖��O�S�ւ̃|�C���^
		nullptr,				//	�`�F�b�N�{�b�N�X�̏����l�S�ւ̃|�C���^
		func_proc,					//	�t�B���^�����֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
		func_init,						//	�J�n���ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
		func_exit,						//	�I�����ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
		NULL,						//	�ݒ肪�ύX���ꂽ�Ƃ��ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
		NULL,						//	�ݒ�E�B���h�E�ɃE�B���h�E���b�Z�[�W���������ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
		NULL,NULL,					//	�V�X�e���Ŏg���܂��̂Ŏg�p���Ȃ��ł�������
		NULL,						//  �g���f�[�^�̈�ւ̃|�C���^ (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
		NULL,						//  �g���f�[�^�T�C�Y (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
		const_cast<TCHAR*>("Upscale Filter by NVIDIA Maxine Video Effects"),
		//  �t�B���^���ւ̃|�C���^ (FILTER_FLAG_EX_INFORMATION�������Ă��鎞�ɗL��)
		NULL,						//	�Z�[�u���J�n����钼�O�ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
		NULL,						//	�Z�[�u���I���������O�ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	};

	return &filter;
}

struct Message {
	int32_t in_x;
	int32_t in_y;
	int32_t out_x;
	int32_t out_y;
	int32_t ar_mode;
	int32_t sr_mode;
	PIXEL pix[4096 * 4096];
};

std::tuple<int, int> GetMode(int ar, int sr) {
	if (sr == -1) { // denoise only
		return { ar > 1 ? 1 : ar, -1 };
	}
	else {
		switch (ar) {
		case -1: // no artifact, enhances more 
			return { -1, 1 };

		case 0: // light artifacts, enhances less and removes more encoding artifacts
			return { -1, 0 };

		case 1: // more artifact, reduces encoder artifacts and enhances
			return { 0, 1 };

		case 2: // more artifact, reduces encoder artifacts and enhances
			return { 1, 1 };

		case 3: // more artifact, reduces encoder artifacts and enhances
			return { 0, 0 };

		case 4: // more artifact, reduces encoder artifacts and enhances
			return { 1, 0 };

		default:
			return { -1, -1 };
		}
	}
}

int GetScaledSize(int size, int scale) {
	switch (scale) {
	case 0: return 4 * size / 3;
	case 1: return 3 * size / 2;
	case 2: return 2 * size;
	case 3: return 3 * size;
	case 4: return 4 * size;
	default: return size;
	}
}

BOOL func_proc(FILTER* fp, FILTER_PROC_INFO* fpip) {
	static Message message{};

	const int in_x = fpip->w;
	const int in_y = fpip->h;
	const int scale = fp->track[1];
	const int out_x = GetScaledSize(in_x, scale);
	const int out_y = GetScaledSize(in_y, scale);
	const auto [ar_mode, sr_mode] = GetMode(fp->track[0], fp->track[1]);

	if (ar_mode == -1 && sr_mode == -1) {
		return TRUE;
	}

	if (out_x > 4096 || out_x > fpip->max_w || out_y > 4096 || out_y > fpip->max_h) {
		return FALSE;
	}

	for (int i = 0; i < in_y; ++i) {
		fp->exfunc->yc2rgb(&message.pix[i * in_x], &fpip->ycp_edit[i * fpip->max_w], in_x);
	}

	message.in_x = in_x;
	message.in_y = in_y;
	message.out_x = out_x;
	message.out_y = out_y;
	message.ar_mode = ar_mode;
	message.sr_mode = sr_mode;

	DWORD size = 24 + 3 * (in_x * in_y);
	DWORD iobyte = 0;

	if (!WriteFile(pipe, &message, size, &iobyte, nullptr) || iobyte != size) {
		return FALSE;
	}

	size = 3 * (out_x * out_y);
	if (!ReadFile(pipe, message.pix, size, &iobyte, nullptr) || iobyte != size) {
		return FALSE;
	}

	for (int i = 0; i < out_y; ++i) {
		fp->exfunc->rgb2yc(&fpip->ycp_edit[i * fpip->max_w], &message.pix[i * out_x], out_x);
	}
	
	fpip->w = out_x;
	fpip->h = out_y;

	return TRUE;
}

BOOL func_init(FILTER* fp) {
	STARTUPINFO si = { sizeof(si) };
	if (!CreateProcess("Plugins\\nvupscale.exe", nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
		MessageBox(NULL, "Failed to start unupscale.exe", "error", MB_OK);
		return FALSE;
	}

	for (int i = 0; i < 10; ++i) { // try connecting to pipe
		Sleep(1); // wait for creating pipe in server

		pipe = CreateFile("\\\\.\\pipe\\nvupscale", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, NULL);
		if (pipe != INVALID_HANDLE_VALUE) {
			break;
		}
	}

	if (pipe == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	return TRUE;
}

BOOL func_exit(FILTER* fp) {
	if (pipe != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe);
	}

	if (pi.hProcess) {
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	return TRUE;
}
