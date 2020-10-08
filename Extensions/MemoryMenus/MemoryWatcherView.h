#ifndef __MEMORYWATCHERVIEW_H__
#define __MEMORYWATCHERVIEW_H__
#include "WindowsSupport/WindowsSupport.pkg"
#include "DeviceInterface/DeviceInterface.pkg"
#include "MemoryWatcherViewPresenter.h"
#include "Memory/MemoryRead.h"

class MemoryWatcherView :public ViewBase
{
public:
	// Constructors
	MemoryWatcherView(IUIManager& uiManager, MemoryWatcherViewPresenter& presenter, IMemory& model);

protected:
	// Member window procedure
	virtual LRESULT WndProcWindow(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:


	struct WatchVariable
	{
		std::wstring Name;
		std::wstring Address;
		std::wstring Byte;
		std::wstring Word;
		std::wstring Long;
	};

	// Constants
	enum Columns
	{
		COLUMN_NAME,
		COLUMN_ADDRESS,
		COLUMN_BYTE,
		COLUMN_WORD,
		COLUMN_LONG
	};

	enum ControlIDList
	{
		CTL_DATAGRID = 100
	};

private:
	// Event handlers
	LRESULT msgWM_CREATE(HWND hwnd, WPARAM wParam, LPARAM lParam);
	LRESULT msgWM_DESTROY(HWND hwnd, WPARAM wParam, LPARAM lParam);
	LRESULT msgWM_TIMER(HWND hwnd, WPARAM wParam, LPARAM lParam);
	LRESULT msgWM_SIZE(HWND hwnd, WPARAM wParam, LPARAM lParam);
	LRESULT msgWM_PAINT(HWND hwnd, WPARAM wParam, LPARAM lParam);

	// Panel dialog window procedure
	static INT_PTR CALLBACK WndProcPanelStatic(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	INT_PTR WndProcPanel(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	
	// Panel dialog event handlers
	INT_PTR msgPanelWM_INITDIALOG(HWND hwnd, WPARAM wParam, LPARAM lParam);
	INT_PTR msgPanelWM_TIMER(HWND hwnd, WPARAM wParam, LPARAM lParam);
	INT_PTR msgPanelWM_COMMAND(HWND hwnd, WPARAM wParam, LPARAM lParam);

private:	
	MemoryWatcherViewPresenter& _presenter;
	IMemory& _model;
	HWND _hwndMem;
	HWND _hwndControlPanel;
	HFONT _hfontHeader;
	HFONT _hfontData;
	int selectedRow;
	int rowCount;
	bool stopTimer;
	std::map<std::wstring, std::wstring> variables;
	// map of rows with columndata (row number, (column number, textdata))
	std::map<unsigned int, std::map<unsigned int, std::wstring>> rowData;
};

#endif
