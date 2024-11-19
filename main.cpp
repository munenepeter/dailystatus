// #include <commctrl.h>
#include <ctime>
#include <memory>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <tchar.h>
#include <vector>
#include <windows.h>

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
      0,
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
      16, 0, 0, 0, FW_NORMAL,
      FALSE, FALSE, FALSE,
      DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      DEFAULT_QUALITY,
      DEFAULT_PITCH | FF_SWISS,
      _T("Segoe UI"));

  if (!g_textArea) {

    g_textArea = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        _T("EDIT"), _T(""),
        WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        20, 70, 560, 200,
        hwnd, (HMENU)IDC_TEXTAREA, GetModuleHandle(NULL), NULL);

    SendMessage(g_textArea, WM_SETFONT, (WPARAM)hFont, TRUE);
  }

  if (!g_button) {

    g_button = CreateWindowEx(
        0,
        _T("BUTTON"), _T("Save Activity"),
        WS_TABSTOP | WS_CHILD | BS_DEFPUSHBUTTON,
        240, 300, 120, 50,
        hwnd, (HMENU)IDC_BUTTON, GetModuleHandle(NULL), NULL);
    SendMessage(g_button, WM_SETFONT, (WPARAM)hFont, TRUE);
  }

  if (!g_label) {

    g_label = CreateWindowEx(
        0,
        _T("STATIC"),
        _T("What are you working on?"),
        WS_CHILD | SS_CENTERIMAGE,
        20, 20, 560, 40,
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
    }
    break;

  case WM_CREATE:

    MessageBox(hwnd, "Application started", "Info", MB_OK);
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

  //   HWND hListView = CreateWindowEx(
  //       0, WC_LISTVIEW, NULL,
  //       WS_CHILD | WS_VISIBLE | LVS_REPORT,
  //       20, 20, 560, 300,
  //       g_hwnd, NULL, GetModuleHandle(NULL), NULL);

  //   ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  //   LVCOLUMN lvc;
  //   lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

  //   lvc.cx = 200;
  //   lvc.pszText = (LPSTR) "Timestamp";
  //   ListView_InsertColumn(hListView, 0, &lvc);

  //   lvc.cx = 350;
  //   lvc.pszText = (LPSTR) "Activity";
  //   ListView_InsertColumn(hListView, 1, &lvc);

  //   auto summary = GetDailySummary();
  //   LVITEM lvi;
  //   lvi.mask = LVIF_TEXT;
  //   lvi.iSubItem = 0;

  //   for (size_t i = 0; i < summary.size(); ++i) {

  //     lvi.iItem = i;
  //     char timestampBuffer[100];
  //     strncpy(timestampBuffer, summary[i].first.c_str(), sizeof(timestampBuffer));
  //     lvi.pszText = timestampBuffer;
  //     ListView_InsertItem(hListView, &lvi);

  //     char activityBuffer[1024];
  //     strncpy(activityBuffer, summary[i].second.c_str(), sizeof(activityBuffer));
  //     ListView_SetItemText(hListView, i, 1, activityBuffer);
  //   }
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
