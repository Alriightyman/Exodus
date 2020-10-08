#include "MemoryWatcherView.h"
#include "resource.h"
#include "WindowsSupport/WindowsSupport.pkg"
#include "WindowsControls/WindowsControls.pkg"

static std::wstring IntToHexString(unsigned int value, int size = 4, bool isAddress = false);

static unsigned int StringToUnsignedInt(std::wstring value, bool strip0x = false);

//----------------------------------------------------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------------------------------------------------
MemoryWatcherView::MemoryWatcherView(IUIManager& uiManager, MemoryWatcherViewPresenter& presenter, IMemory& model)
:ViewBase(uiManager, presenter), _presenter(presenter), _model(model)
{
	_hwndMem = NULL;
	_hwndControlPanel = NULL;
	_hfontData = NULL;
	_hfontHeader = NULL;
	stopTimer = false;
	SetWindowSettings(presenter.GetUnqualifiedViewTitle(), 0, 0, 440, 500);
	SetDockableViewType(true, DockPos::Right, false, L"Exodus.VerticalWatchers");
}

//----------------------------------------------------------------------------------------------------------------------
// Member window procedure
//----------------------------------------------------------------------------------------------------------------------
LRESULT MemoryWatcherView::WndProcWindow(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	WndProcDialogImplementGiveFocusToChildWindowOnClick(hwnd, msg, wparam, lparam);
	switch (msg)
	{
	case WM_CREATE:
		return msgWM_CREATE(hwnd, wparam, lparam);
	case WM_DESTROY:
		return msgWM_DESTROY(hwnd, wparam, lparam);
	case WM_TIMER:
		return msgWM_TIMER(hwnd, wparam, lparam);
	case WM_SIZE:
		return msgWM_SIZE(hwnd, wparam, lparam);
	case WM_PAINT:
		return msgWM_PAINT(hwnd, wparam, lparam);
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//----------------------------------------------------------------------------------------------------------------------
// Event handlers
//----------------------------------------------------------------------------------------------------------------------
LRESULT MemoryWatcherView::msgWM_CREATE(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	// Register the DataGrid window class
	WC_DataGrid::RegisterWindowClass(GetAssemblyHandle());

	// Create the DataGrid child control
	_hwndMem = CreateWindowEx(WS_EX_CLIENTEDGE, WC_DataGrid::WindowClassName, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 0, hwnd, (HMENU)CTL_DATAGRID, GetAssemblyHandle(), NULL);

	// Insert our columns into the DataGrid control
	SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::InsertColumn, 0, (LPARAM) & (const WC_DataGrid::Grid_InsertColumn&)WC_DataGrid::Grid_InsertColumn(L"Name", COLUMN_NAME));
	SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::InsertColumn, 0, (LPARAM) & (const WC_DataGrid::Grid_InsertColumn&)WC_DataGrid::Grid_InsertColumn(L"Address", COLUMN_ADDRESS));
	SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::InsertColumn, 0, (LPARAM) & (const WC_DataGrid::Grid_InsertColumn&)WC_DataGrid::Grid_InsertColumn(L"Byte", COLUMN_BYTE));
	SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::InsertColumn, 0, (LPARAM) & (const WC_DataGrid::Grid_InsertColumn&)WC_DataGrid::Grid_InsertColumn(L"Word", COLUMN_WORD));
	SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::InsertColumn, 0, (LPARAM) & (const WC_DataGrid::Grid_InsertColumn&)WC_DataGrid::Grid_InsertColumn(L"Long", COLUMN_LONG));

	// Create the dialog control panel
	_hwndControlPanel = CreateDialogParam(GetAssemblyHandle(), MAKEINTRESOURCE(IDD_MEMEORY_WATCHER), hwnd, WndProcPanelStatic, (LPARAM)this);
	ShowWindow(_hwndControlPanel, SW_SHOWNORMAL);
	UpdateWindow(_hwndControlPanel);


	// Obtain the correct metrics for our custom font object
	int fontPointSize = 8;
	HDC hdc = GetDC(hwnd);
	int fontnHeight = -MulDiv(fontPointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	ReleaseDC(hwnd, hdc);

	// Create the font for the header in the grid control
	std::wstring headerFontTypefaceName = L"MS Shell Dlg";
	_hfontHeader = CreateFont(fontnHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, &headerFontTypefaceName[0]);

	// Set the header font for the grid control
	SendMessage(_hwndMem, WM_SETFONT, (WPARAM)_hfontHeader, (LPARAM)TRUE);

	// Create the font for the data region in the grid control
	std::wstring dataFontTypefaceName = L"Courier New";
	_hfontData = CreateFont(fontnHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, &dataFontTypefaceName[0]);

	// Set the data region font for the grid control
	SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::SetDataAreaFont, (WPARAM)_hfontData, (LPARAM)TRUE);

	// Create a timer to trigger updates to the grid
	SetTimer(hwnd, 1, 200, NULL);

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
LRESULT MemoryWatcherView::msgWM_DESTROY(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	SendMessage(_hwndMem, WM_SETFONT, (WPARAM)NULL, (LPARAM)FALSE);
	SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::SetDataAreaFont, (WPARAM)NULL, (LPARAM)FALSE);
	DeleteObject(_hfontHeader);
	DeleteObject(_hfontData);

	KillTimer(hwnd, 1);

	return DefWindowProc(hwnd, WM_DESTROY, wparam, lparam);
}

//----------------------------------------------------------------------------------------------------------------------
LRESULT MemoryWatcherView::msgWM_TIMER(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	if (rowData.size() > 0 && !stopTimer)
	{		
		std::vector<std::map<unsigned int, std::wstring>> rowsInfo;
		bool oddAddress = false;

		for (auto rowIter : rowData)
		{
			oddAddress = false;
			auto row = rowIter.second;
			unsigned int addressValue = StringToUnsignedInt(row[COLUMN_ADDRESS], true);
			
			auto actualAddressValue = addressValue / 2;

			if (addressValue % 2 != 0)
			{
				// need to add a value if it is odd
				oddAddress = true;
			}

			auto upperNibble = _model.ReadMemoryEntry(actualAddressValue);
			unsigned int lowerNibble = 0;
			
			// only set the lowerNibble IF we don't exceed RAM limitations ie. 0xFFFF
			if (addressValue <= 0xFFFC && !oddAddress)
				lowerNibble = _model.ReadMemoryEntry(actualAddressValue + 1);


			std::wstring byteValue = IntToHexString(upperNibble & 0xFF, 2);
			std::wstring wordValue = L"";
			std::wstring longValue = L"";

			if (!oddAddress)
			{
				byteValue = IntToHexString((upperNibble & 0x0000FF00) >> 8, 2);
				wordValue = IntToHexString(upperNibble);
				longValue = IntToHexString((upperNibble << 16) | lowerNibble, 8);
			}

			row[COLUMN_BYTE] = byteValue;
			row[COLUMN_WORD] = wordValue;
			row[COLUMN_LONG] = longValue;

			rowsInfo.push_back(row);
		}

		WC_DataGrid::Grid_InsertRows insertRows;
		insertRows.rowCount = rowCount;
		insertRows.rowData = rowsInfo;
		insertRows.clearExistingRows = true;

		SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::InsertRows, 0, (LPARAM)&insertRows);
	}	

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
LRESULT MemoryWatcherView::msgWM_SIZE(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	// Read the new client size of the window
	RECT rect;
	GetClientRect(hwnd, &rect);
	int controlWidth = rect.right;
	int controlHeight = rect.bottom;
	GetClientRect(_hwndControlPanel, &rect);
	int controlPanelWidth = rect.right;
	int controlPanelHeight = rect.bottom;

	// Global parameters defining how child windows are positioned
	int borderSize = 4;

	// Calculate the new position of the control panel
	int controlPanelPosX = borderSize;
	int controlPanelPosY = controlHeight - (borderSize + controlPanelHeight);
	MoveWindow(_hwndControlPanel, controlPanelPosX, controlPanelPosY, controlPanelWidth, controlPanelHeight, TRUE);

	// Calculate the new size and position of the list
	int listBoxWidth = controlWidth - (borderSize * 2);
	int listBoxPosX = borderSize;
	int listBoxHeight = controlHeight - ((borderSize * 2) + controlPanelHeight);
	int listBoxPosY = borderSize;
	MoveWindow(_hwndMem, listBoxPosX, listBoxPosY, listBoxWidth, listBoxHeight, TRUE);

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
LRESULT MemoryWatcherView::msgWM_PAINT(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	// Fill the background of the control with the dialog background colour
	HDC hdc = GetDC(hwnd);
	HBRUSH hbrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
	HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, hbrush);

	RECT rect;
	GetClientRect(hwnd, &rect);
	FillRect(hdc, &rect, hbrush);

	SelectObject(hdc, hbrushOld);
	DeleteObject(hbrush);
	ReleaseDC(hwnd, hdc);

	return DefWindowProc(hwnd, WM_PAINT, wparam, lparam);
}


//----------------------------------------------------------------------------------------------------------------------
// Member window procedure
//----------------------------------------------------------------------------------------------------------------------
INT_PTR CALLBACK MemoryWatcherView::WndProcPanelStatic(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// Obtain the object pointer
	auto state = (MemoryWatcherView*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	// Process the message
	switch (msg)
	{
	case WM_INITDIALOG:
		// Set the object pointer
		state = (MemoryWatcherView*)lparam;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(state));

		// Pass this message on to the member window procedure function
		if (state != 0)
		{
			return state->WndProcPanel(hwnd, msg, wparam, lparam);
		}
		break;
	case WM_DESTROY:
		if (state != 0)
		{
			// Pass this message on to the member window procedure function
			INT_PTR result = state->WndProcPanel(hwnd, msg, wparam, lparam);

			// Discard the object pointer
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)0);

			// Return the result from processing the message
			return result;
		}
		break;
	}

	// Pass this message on to the member window procedure function
	INT_PTR result = FALSE;
	if (state != 0)
	{
		result = state->WndProcPanel(hwnd, msg, wparam, lparam);
	}
	return result;
}

//----------------------------------------------------------------------------------------------------------------------
INT_PTR MemoryWatcherView::WndProcPanel(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
WndProcDialogImplementSaveFieldWhenLostFocus(hwnd, msg, wparam, lparam);
switch (msg)
{
case WM_INITDIALOG:
	return  msgPanelWM_INITDIALOG(hwnd, wparam, lparam);
case WM_TIMER:
	return  msgPanelWM_TIMER(hwnd, wparam, lparam);
case WM_COMMAND:
	return  msgPanelWM_COMMAND(hwnd, wparam, lparam);
}
return FALSE;
}
//----------------------------------------------------------------------------------------------------------------------
// Panel dialog event handlers
//----------------------------------------------------------------------------------------------------------------------
INT_PTR MemoryWatcherView::msgPanelWM_INITDIALOG(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	// limit text to 4 characters
	SendDlgItemMessage(_hwndControlPanel, IDC_EDIT_RAM_ADDRESS, EM_LIMITTEXT, (WPARAM)4, (LPARAM)0);

	// only allow certain characters
	// ???

	return TRUE;
}

INT_PTR MemoryWatcherView::msgPanelWM_TIMER(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	return TRUE;
}

INT_PTR MemoryWatcherView::msgPanelWM_COMMAND(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
	if (HIWORD(wparam) == BN_CLICKED)
	{
		switch (LOWORD(wparam))
		{
		case IDADD:
		{
			stopTimer = true;
			int nameLenth = 0;
			int addressLength = 0;
			wchar_t* name = 0;
			wchar_t* address = 0;
			std::map<unsigned int, std::wstring> rowInfo;
			bool oddAddress = false;

			// variable name
			nameLenth = SendDlgItemMessage(_hwndControlPanel, IDC_EDIT_VARIABLE_NAME, EM_LINELENGTH, (WPARAM)0, (LPARAM)0);
			if (nameLenth == 0)
			{
				stopTimer = false;
				return 0;
			}

			nameLenth++;
			name = new wchar_t[nameLenth];
			GetDlgItemText(_hwndControlPanel, IDC_EDIT_VARIABLE_NAME, name, nameLenth);

			// address text
			addressLength = SendDlgItemMessage(_hwndControlPanel, IDC_EDIT_RAM_ADDRESS, EM_LINELENGTH, (WPARAM)0, (LPARAM)0);

			if (addressLength == 0)
			{
				stopTimer = false;
				return 0;
			}


			addressLength++;
			address = new wchar_t[addressLength];
			GetDlgItemText(_hwndControlPanel, IDC_EDIT_RAM_ADDRESS, address, addressLength);


			// set the variable and add to list
			rowInfo[COLUMN_NAME] = name;

			// set the values
			unsigned int addressValue = StringToUnsignedInt(address);
			rowInfo[COLUMN_ADDRESS] = IntToHexString(addressValue, 4, true);

			auto actualAddressValue = addressValue / 2;

			if (addressValue % 2 != 0)
			{
				oddAddress = true;
			}

			auto upperNibble = _model.ReadMemoryEntry(actualAddressValue);
			unsigned int lowerNibble = 0;

			// only set the lowerNibble IF we don't exceed RAM limitations ie. 0xFFFF
			if (addressValue <= 0xFFFC && !oddAddress)
				lowerNibble = _model.ReadMemoryEntry(actualAddressValue + 2);

			
			std::wstring byteValue = IntToHexString(upperNibble, 2);
			std::wstring wordValue = L"";
			std::wstring longValue = L"";

			// if not odd, set the values
			if (!oddAddress) 
			{
				byteValue = IntToHexString((upperNibble & 0x0000FF00) >> 6, 2);
				wordValue = IntToHexString(upperNibble);
				longValue = IntToHexString((upperNibble << 16) | lowerNibble, 8);
			}
			rowInfo[COLUMN_BYTE] = byteValue;
			rowInfo[COLUMN_WORD] = wordValue;
			rowInfo[COLUMN_LONG] = longValue;

			// clear the textboxes
			SetDlgItemText(_hwndControlPanel, IDC_EDIT_VARIABLE_NAME, L"");
			SetDlgItemText(_hwndControlPanel, IDC_EDIT_RAM_ADDRESS, L"");

			WC_DataGrid::Grid_InsertRows insertRows;
			insertRows.rowCount = 1;
			insertRows.rowData = std::vector<std::map<unsigned int, std::wstring>>() = { rowInfo };

			// add entry into grid
			SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::InsertRows, 0, (LPARAM)&insertRows);
			rowData[rowCount] = rowInfo;
			rowCount++;
			stopTimer = false;
		}
		break;
		case IDREMOVE:
		{
			WC_DataGrid::Grid_DeleteRows deleteRows;
			deleteRows.rowCount = rowCount;
			deleteRows.targetRowNo = 0;
			
			SendMessage(_hwndMem, (UINT)WC_DataGrid::WindowMessages::DeleteRows, 0, (LPARAM)&deleteRows);

			rowData.clear(); 
			rowCount = 0;			
		}
			break;
		}
	}
	else if (LOWORD(wparam) == CTL_DATAGRID)
	{
		if ((WC_DataGrid::WindowNotifications)HIWORD(wparam) == WC_DataGrid::WindowNotifications::CellButtonClick)
		{
			WC_DataGrid::Grid_CellButtonClickEvent* cellButtonClickEventInfo = (WC_DataGrid::Grid_CellButtonClickEvent*)lparam;
			//if ((cellButtonClickEventInfo->targetColumnID == ValueColumnID) && (cellButtonClickEventInfo->targetRowNo < _rowInfo.size())// && LockTargetRowEntry(cellButtonClickEventInfo->targetRowNo))
			//{

			selectedRow = cellButtonClickEventInfo->targetRowNo;

		}
		else
		{
			selectedRow = -1;
		}
	}
	return TRUE;
}

static std::wstring IntToHexString(unsigned int value, int size, bool isAddress)
{
	std::wstringstream ss;
	ss << L"0x";
	ss << std::setfill(L'0') << std::setw(size) <<
		std::hex << std::uppercase << value;

	return ss.str();
}

static unsigned int StringToUnsignedInt(std::wstring value, bool strip0x)
{
	std::wstring outputValue;
	if (strip0x)
	{
		outputValue = std::wstring(value.begin() + 2, value.end());
	}
	else
	{
		outputValue = value;
	}

	unsigned int x;
	std::wstringstream ss;
	ss << std::hex << outputValue;
	ss >> x;

	return x;
}