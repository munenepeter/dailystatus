#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0600
#define WINVER 0x0500
#define _WIN32_WINDOWS 0x0500

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <ctime>
#include <memory>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <tchar.h>
#include <vector>

#pragma comment(lib, "winmm.lib")

#define IDC_TEXTAREA 101
#define IDC_BUTTON 102
#define IDC_MENU_SUMMARY 1000

HWND g_textArea = NULL;
HWND g_button = NULL;
HWND g_label = NULL;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateUIElements(HWND hwnd);
void DisplayCenteredMessage(HWND hwnd, const char *text);
void InitializeDB();
void SaveActivity(const char *activity);
std::vector<std::pair<std::string, std::string>> GetDailySummary();
void ShowInputDialog();
void ShowSummaryDialog();
void CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();

const int TIMER_ID = 1;
const int SUMMARY_TIMER_ID = 2;
const UINT WM_TRAYICON = WM_USER + 1;

HWND g_hwnd = NULL;
HWND g_editBox = NULL;
sqlite3 *g_db = NULL;
NOTIFYICONDATAW g_nid = {};

TCHAR szClassName[] = _T("ModernTimeTrackerWindow");

COLORREF BACKGROUND_COLOR = RGB(240, 240, 250);
COLORREF TEXT_COLOR = RGB(40, 40, 60);
COLORREF BUTTON_COLOR = RGB(100, 149, 237);
COLORREF BUTTON_TEXT_COLOR = RGB(255, 255, 255);

int WINAPI WinMain(HINSTANCE hThisInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpszArgument,
                   int nCmdShow) {
  HWND hwnd;
  MSG messages;
  WNDCLASSEX wincl;

  //   InitCommonControls();

  wincl.hInstance = hThisInstance;
  wincl.lpszClassName = szClassName;
  wincl.lpfnWndProc = WindowProc;
  wincl.style = CS_DBLCLKS;
  wincl.cbSize = sizeof(WNDCLASSEX);

  wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
  wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
  wincl.lpszMenuName = NULL;
  wincl.cbClsExtra = 0;
  wincl.cbWndExtra = 0;

  wincl.hbrBackground = CreateSolidBrush(BACKGROUND_COLOR);

  InitializeDB();

  if (!RegisterClassEx(&wincl))
    return 0;

  hwnd = CreateWindowEx(
      WS_EX_TOPMOST,
      szClassName,
      _T("Activity Tracker"),
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      600,
      500,
      HWND_DESKTOP,
      NULL,
      hThisInstance,
      NULL);

  SetTimer(hwnd, TIMER_ID, 18000, NULL);

  while (GetMessage(&messages, NULL, 0, 0)) {

    TranslateMessage(&messages);

    DispatchMessage(&messages);
  }

  sqlite3_close(g_db);

  return messages.wParam;
}

void CreateUIElements(HWND hwnd) {

  HMENU hMenu = CreateMenu();
  HMENU hSubMenu = CreatePopupMenu();
  AppendMenu(hSubMenu, MF_STRING, IDC_MENU_SUMMARY, _T("View Daily Summary"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, _T("Activities"));
  SetMenu(hwnd, hMenu);

  ShowWindow(hwnd, SW_SHOWMINIMIZED);
  UpdateWindow(hwnd);

  HFONT hFont = CreateFont(
      20, 0, 0, 0, FW_NORMAL,
      FALSE, FALSE, FALSE,
      DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS,
      _T("Segoe UI"));

  if (!g_textArea) {

    g_textArea = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        _T("EDIT"), _T(""),
        WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        40, 100, 520, 160,
        hwnd, (HMENU)IDC_TEXTAREA, GetModuleHandle(NULL), NULL);

    SendMessage(g_textArea, WM_SETFONT, (WPARAM)hFont, TRUE);
  }

  if (!g_button) {

    g_button = CreateWindowEx(
        0,
        _T("BUTTON"), _T("Save Activity"),
        WS_TABSTOP | WS_CHILD | BS_FLAT,
        250, 280, 100, 40,
        hwnd, (HMENU)IDC_BUTTON, GetModuleHandle(NULL), NULL);
    SendMessage(g_button, WM_SETFONT, (WPARAM)hFont, TRUE);
  }

  if (!g_label) {

    g_label = CreateWindowEx(
        0,
        _T("STATIC"),
        _T("What are you working on?"),
        WS_CHILD | SS_CENTERIMAGE,
        40, 40, 520, 40,
        hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessage(g_label, WM_SETFONT, (WPARAM)hFont, TRUE);
  }

  ShowWindow(g_textArea, SW_SHOW);
  ShowWindow(g_button, SW_SHOW);
  ShowWindow(g_label, SW_SHOW);

  SetFocus(g_textArea);
}

void HideUIElements() {
  if (g_textArea) ShowWindow(g_textArea, SW_HIDE);
  if (g_button) ShowWindow(g_button, SW_HIDE);
  if (g_label) ShowWindow(g_label, SW_HIDE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_TIMER:
    if (LOWORD(wParam) == TIMER_ID) {
      CreateUIElements(hwnd);
      
      // Play the system notification sound
      PlaySound(TEXT("SystemExclamation"), NULL, SND_ALIAS | SND_ASYNC);

      // Bring the window to the top and keep it there
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      SetForegroundWindow(hwnd);
      SetActiveWindow(hwnd);

      // Show the window if it was minimized or hidden
      ShowWindow(hwnd, SW_RESTORE);
    }
    break;

  case WM_CREATE:

    // MessageBox(hwnd, "Application started", "Info", MB_OK);
    break;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_BUTTON) {
      char buffer[1024];
      GetWindowText(g_textArea, buffer, sizeof(buffer));

      if (strlen(buffer) == 0) {
        MessageBox(hwnd, "The text box is empty!", "Warning", MB_OK | MB_ICONWARNING);
        return 1;
      }

      if (g_db == NULL) {
        MessageBox(hwnd, "Database connection is NULL!", "Warning", MB_OK | MB_ICONWARNING);
        return 0;
      }

      try {
        SaveActivity(buffer);
        MessageBox(hwnd, "Activity saved successfully", "Debug: Save Status", MB_OK);

        SetWindowText(g_textArea, "");

        HideUIElements();

        SetTimer(hwnd, TIMER_ID, 18000, NULL);

        ShowWindow(hwnd, SW_HIDE);
      } catch (const std::exception &e) {
        MessageBox(hwnd, "Failed to save activity", "Debug: Error", MB_OK);
      }
    } else if (LOWORD(wParam) == IDC_MENU_SUMMARY) {

      ShowSummaryDialog();
    }
    break;

  case WM_CLOSE:
    // Prevent closing the window until an activity is saved
    MessageBox(hwnd, "Please save an activity before closing.", "Info", MB_OK | MB_ICONINFORMATION);
    return 0;

  case WM_DESTROY:
    KillTimer(hwnd, TIMER_ID);
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

std::vector<std::pair<std::string, std::string>> GetDailySummary() {
  std::vector<std::pair<std::string, std::string>> summary;
  const char *sql = "SELECT timestamp, activity FROM activities "
                    "WHERE date(timestamp) = date('now') "
                    "ORDER BY timestamp DESC;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);

  if (rc == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const char *timestamp = (const char *)sqlite3_column_text(stmt, 0);
      const char *activity = (const char *)sqlite3_column_text(stmt, 1);
      summary.emplace_back(timestamp, activity);
    }
  }

  sqlite3_finalize(stmt);
  return summary;
}

void ShowSummaryDialog() {
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        szClassName,
        _T("Daily Activity Summary"),
        WS_VISIBLE | WS_SYSMENU | WS_CAPTION,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );

    // Create ListView
    HWND hListView = CreateWindowEx(
        0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_BORDER,
        20, 20, 740, 520,
        hDlg, NULL, GetModuleHandle(NULL), NULL
    );

    // Modern styling for ListView
    ListView_SetExtendedListViewStyle(hListView, 
        LVS_EX_FULLROWSELECT | 
        LVS_EX_GRIDLINES | 
        LVS_EX_DOUBLEBUFFER
    );

    // Set up columns
    LVCOLUMN lvc = { 0 };
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    // Time column
    lvc.cx = 200;
    lvc.pszText = (LPSTR)"Time";
    ListView_InsertColumn(hListView, 0, &lvc);

    // Activity column
    lvc.cx = 520;
    lvc.pszText = (LPSTR)"Activity";
    ListView_InsertColumn(hListView, 1, &lvc);

    // Get and populate data
    auto summary = GetDailySummary();
    LVITEM lvi = { 0 };
    lvi.mask = LVIF_TEXT;

    // Custom font for ListView
    HFONT hFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        _T("Segoe UI")
    );
    SendMessage(hListView, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Populate the ListView
    for (size_t i = 0; i < summary.size(); ++i) {
        lvi.iItem = i;
        
        // Format timestamp for better readability
        std::string formattedTime = summary[i].first.substr(11, 5); // Extract HH:MM
        lvi.iSubItem = 0;
        lvi.pszText = (LPSTR)formattedTime.c_str();
        ListView_InsertItem(hListView, &lvi);

        // Add activity text
        lvi.iSubItem = 1;
        lvi.pszText = (LPSTR)summary[i].second.c_str();
        ListView_SetItem(hListView, &lvi);
    }

    // Create "Close" button
    HWND hButton = CreateWindow(
        _T("BUTTON"), _T("Close"),
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        350, 500, 100, 40,
        hDlg, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL
    );
    SendMessage(hButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Center the dialog on screen
    RECT rc;
    GetWindowRect(hDlg, &rc);
    int xPos = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int yPos = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hDlg, NULL, xPos, yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
}

void InitializeDB() {
  int rc = sqlite3_open("timetracker.db", &g_db);
  if (rc) {
    MessageBoxW(NULL, L"Cannot open database", L"Error", MB_OK | MB_ICONERROR);
    return;
  }

  const char *sql = "CREATE TABLE IF NOT EXISTS activities ("
                    "timestamp TEXT NOT NULL,"
                    "activity TEXT NOT NULL);";

  char *errMsg = 0;
  rc = sqlite3_exec(g_db, sql, 0, 0, &errMsg);
  if (rc != SQLITE_OK) {
    MessageBoxW(NULL, L"SQL error", L"Error", MB_OK | MB_ICONERROR);
    sqlite3_free(errMsg);
  }
}

void SaveActivity(const char *activity) {
  if (activity == nullptr || strlen(activity) == 0) {
    MessageBoxA(NULL, "Activity is empty!", "Debug: Error", MB_OK);
    return;
  }

  time_t now = time(0);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

  std::string sql = "INSERT INTO activities (timestamp, activity) VALUES (?, ?);";
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(g_db, sql.c_str(), -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    MessageBoxA(NULL, "Failed to prepare SQL statement", "Debug: SQL Error", MB_OK);
    return;
  }

  rc = sqlite3_bind_text(stmt, 1, timestamp, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    MessageBoxA(NULL, "Failed to bind timestamp", "Debug: SQL Error", MB_OK);
    sqlite3_finalize(stmt);
    return;
  }

  rc = sqlite3_bind_text(stmt, 2, activity, -1, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    MessageBoxA(NULL, "Failed to bind activity", "Debug: SQL Error", MB_OK);
    sqlite3_finalize(stmt);
    return;
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    MessageBoxA(NULL, "Failed to execute statement", "Debug: SQL Error", MB_OK);
    sqlite3_finalize(stmt);
    return;
  }

  sqlite3_finalize(stmt);
}

void DisplayCenteredMessage(HWND hwnd, const char *text) {
  int result = MessageBox(hwnd, text, "Text Content", MB_OK | MB_ICONINFORMATION);
  if (result == IDOK) {
    SetFocus(GetDlgItem(hwnd, IDC_TEXTAREA));
  }
}
