#include "TranscriptDialog.h"

#include "ChatConstants.h"
#include "MessageParse.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <vector>

namespace chitchat {
namespace {

constexpr int kPad = 12;
constexpr int kBtnH = 28;
constexpr int kBtnW = 96;
constexpr int kHelpH = 54;

struct DialogState {
	std::string* transcript = nullptr;
	HWND edit = nullptr;
	bool accepted = false;
	bool* loop_running = nullptr;
};

std::wstring Utf8ToWideLenient(const std::string& utf8) {
	if (utf8.empty()) {
		return L"";
	}
	const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	if (needed <= 0) {
		return L"";
	}
	std::wstring wide(static_cast<std::size_t>(needed), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &wide[0], needed);
	return wide;
}

bool WideToUtf8(const std::wstring& wide, std::string* utf8) {
	if (!utf8) {
		return false;
	}
	utf8->clear();
	if (wide.empty()) {
		return true;
	}
	const int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
	if (needed <= 0) {
		return false;
	}
	if (needed > kMaxTranscriptBytes) {
		return false;
	}
	utf8->assign(static_cast<std::size_t>(needed), '\0');
	const int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), static_cast<int>(wide.size()), &(*utf8)[0], needed, nullptr, nullptr);
	if (written != needed) {
		utf8->clear();
		return false;
	}
	return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg) {
	case WM_CREATE: {
		auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
		state = reinterpret_cast<DialogState*>(cs->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
		CreateWindowExW(0, L"STATIC",
						L"me: right side    them: left side    Enter = line break in that bubble.\r\n"
						L"Start a line with me: or them: for a new message. Click OK to save.",
						WS_CHILD | WS_VISIBLE, kPad, kPad, 640, kHelpH, hwnd, nullptr, cs->hInstance, nullptr);

		DWORD edit_style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_BORDER;
		state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", edit_style, kPad, kPad + kHelpH, 640, 360, hwnd,
									  reinterpret_cast<HMENU>(1001), cs->hInstance, nullptr);
		SendMessageW(state->edit, EM_SETLIMITTEXT, static_cast<WPARAM>(kMaxTranscriptBytes), 0);
		std::string initial_utf8 = state->transcript ? *state->transcript : std::string();
		std::string crlf;
		crlf.reserve(initial_utf8.size() + 8);
		for (std::size_t i = 0; i < initial_utf8.size(); ++i) {
			if (initial_utf8[i] == '\n' && (i == 0 || initial_utf8[i - 1] != '\r')) {
				crlf.push_back('\r');
			}
			crlf.push_back(initial_utf8[i]);
		}
		const std::wstring initial = Utf8ToWideLenient(crlf);
		SetWindowTextW(state->edit, initial.c_str());

		CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, kPad, 440, kBtnW, kBtnH, hwnd,
						reinterpret_cast<HMENU>(IDOK), cs->hInstance, nullptr);
		CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, kPad + kBtnW + 8, 440, kBtnW, kBtnH, hwnd,
						reinterpret_cast<HMENU>(IDCANCEL), cs->hInstance, nullptr);
		return 0;
	}
	case WM_SIZE: {
		if (!state || !state->edit) {
			return 0;
		}
		const int cw = LOWORD(lParam);
		const int ch = HIWORD(lParam);
		MoveWindow(state->edit, kPad, kPad + kHelpH, std::max(100, cw - kPad * 2), std::max(80, ch - kHelpH - kBtnH - kPad * 3), TRUE);
		MoveWindow(GetDlgItem(hwnd, IDOK), kPad, ch - kPad - kBtnH, kBtnW, kBtnH, TRUE);
		MoveWindow(GetDlgItem(hwnd, IDCANCEL), kPad + kBtnW + 8, ch - kPad - kBtnH, kBtnW, kBtnH, TRUE);
		return 0;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK && state) {
			const int n = GetWindowTextLengthW(state->edit);
			if (n < 0 || n > kMaxTranscriptBytes) {
				MessageBoxW(hwnd, L"Transcript is too large.", L"Chit Chat", MB_OK | MB_ICONWARNING);
				return 0;
			}
			std::vector<wchar_t> buf(static_cast<std::size_t>(n) + 1u, L'\0');
			GetWindowTextW(state->edit, buf.data(), n + 1);
			std::string utf8;
			if (!WideToUtf8(std::wstring(buf.data()), &utf8)) {
				MessageBoxW(hwnd, L"Text must be valid Unicode and under the size limit.", L"Chit Chat", MB_OK | MB_ICONWARNING);
				return 0;
			}
			utf8 = SanitizeTranscript(utf8, kMaxTranscriptBytes);
			std::vector<Message> parsed;
			std::string err;
			if (!ParseTranscript(utf8, &parsed, &err)) {
				const std::wstring werr = Utf8ToWideLenient(err.empty() ? "Could not parse transcript." : err);
				MessageBoxW(hwnd, werr.c_str(), L"Chit Chat", MB_OK | MB_ICONWARNING);
				return 0;
			}
			*state->transcript = FormatTranscript(parsed);
			state->accepted = true;
			DestroyWindow(hwnd);
			return 0;
		}
		if (LOWORD(wParam) == IDCANCEL) {
			DestroyWindow(hwnd);
			return 0;
		}
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			DestroyWindow(hwnd);
			return 0;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		if (state && state->loop_running) {
			*state->loop_running = false;
		}
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

bool EditTranscriptDialog(void* parent_hwnd, std::string* transcript, std::string* error) {
	if (!transcript) {
		if (error) {
			*error = "Internal error: missing transcript.";
		}
		return false;
	}

	HWND parent = reinterpret_cast<HWND>(parent_hwnd);
	if (!parent) {
		parent = GetForegroundWindow();
	}

	WNDCLASSW wc{};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wc.lpszClassName = L"ChitChatTranscriptDialog";
	if (!RegisterClassW(&wc)) {
		const DWORD already = GetLastError();
		if (already != ERROR_CLASS_ALREADY_EXISTS) {
			if (error) {
				*error = "Could not register the message editor.";
			}
			return false;
		}
	}

	bool loop_running = true;
	DialogState state;
	state.transcript = transcript;
	state.loop_running = &loop_running;

	HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW, wc.lpszClassName, L"Chit Chat — Messages",
								WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
								720, 540, parent, nullptr, wc.hInstance, &state);
	if (!hwnd) {
		if (error) {
			*error = "Could not open the message editor.";
		}
		return false;
	}

	bool disabled_parent = false;
	if (parent && parent != hwnd) {
		EnableWindow(parent, FALSE);
		disabled_parent = true;
	}

	MSG msg;
	while (loop_running) {
		const BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
		if (gm == 0) {
			PostQuitMessage(static_cast<int>(msg.wParam));
			break;
		}
		if (gm < 0) {
			break;
		}
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
			(msg.hwnd == hwnd || IsChild(hwnd, msg.hwnd))) {
			DestroyWindow(hwnd);
			continue;
		}
		if (!IsDialogMessageW(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	if (disabled_parent && IsWindow(parent)) {
		EnableWindow(parent, TRUE);
		SetForegroundWindow(parent);
	}

	return state.accepted;
}

} // namespace chitchat
