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
		FILTER_FLAG_EX_INFORMATION,	//	フィルタのフラグ
		//	FILTER_FLAG_ALWAYS_ACTIVE		: フィルタを常にアクティブにします
		//	FILTER_FLAG_CONFIG_POPUP		: 設定をポップアップメニューにします
		//	FILTER_FLAG_CONFIG_CHECK		: 設定をチェックボックスメニューにします
		//	FILTER_FLAG_CONFIG_RADIO		: 設定をラジオボタンメニューにします
		//	FILTER_FLAG_EX_DATA				: 拡張データを保存出来るようにします。
		//	FILTER_FLAG_PRIORITY_HIGHEST	: フィルタのプライオリティを常に最上位にします
		//	FILTER_FLAG_PRIORITY_LOWEST		: フィルタのプライオリティを常に最下位にします
		//	FILTER_FLAG_WINDOW_THICKFRAME	: サイズ変更可能なウィンドウを作ります
		//	FILTER_FLAG_WINDOW_SIZE			: 設定ウィンドウのサイズを指定出来るようにします
		//	FILTER_FLAG_DISP_FILTER			: 表示フィルタにします
		//	FILTER_FLAG_EX_INFORMATION		: フィルタの拡張情報を設定できるようにします
		//	FILTER_FLAG_NO_CONFIG			: 設定ウィンドウを表示しないようにします
		//	FILTER_FLAG_AUDIO_FILTER		: オーディオフィルタにします
		//	FILTER_FLAG_RADIO_BUTTON		: チェックボックスをラジオボタンにします
		//	FILTER_FLAG_WINDOW_HSCROLL		: 水平スクロールバーを持つウィンドウを作ります
		//	FILTER_FLAG_WINDOW_VSCROLL		: 垂直スクロールバーを持つウィンドウを作ります
		//	FILTER_FLAG_IMPORT				: インポートメニューを作ります
		//	FILTER_FLAG_EXPORT				: エクスポートメニューを作ります
		0,0,						//	設定ウインドウのサイズ (FILTER_FLAG_WINDOW_SIZEが立っている時に有効)
		const_cast<TCHAR*>("nvupscale"),			//	フィルタの名前
		NumTrackbars,					//	トラックバーの数 (0なら名前初期値等もNULLでよい)
		trackbar_names,					//	トラックバーの名前郡へのポインタ
		trackbar_initials,				//	トラックバーの初期値郡へのポインタ
		trackbar_mins, trackbar_maxs,			//	トラックバーの数値の下限上限 (NULLなら全て0～256)
		0,					//	チェックボックスの数 (0なら名前初期値等もNULLでよい)
		nullptr,					//	チェックボックスの名前郡へのポインタ
		nullptr,				//	チェックボックスの初期値郡へのポインタ
		func_proc,					//	フィルタ処理関数へのポインタ (NULLなら呼ばれません)
		func_init,						//	開始時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
		func_exit,						//	終了時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
		NULL,						//	設定が変更されたときに呼ばれる関数へのポインタ (NULLなら呼ばれません)
		NULL,						//	設定ウィンドウにウィンドウメッセージが来た時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
		NULL,NULL,					//	システムで使いますので使用しないでください
		NULL,						//  拡張データ領域へのポインタ (FILTER_FLAG_EX_DATAが立っている時に有効)
		NULL,						//  拡張データサイズ (FILTER_FLAG_EX_DATAが立っている時に有効)
		const_cast<TCHAR*>("Upscale Filter by NVIDIA Maxine Video Effects"),
		//  フィルタ情報へのポインタ (FILTER_FLAG_EX_INFORMATIONが立っている時に有効)
		NULL,						//	セーブが開始される直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
		NULL,						//	セーブが終了した直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
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
