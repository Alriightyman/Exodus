#ifndef __LOGGINGVIEW_H__
#define __LOGGINGVIEW_H__
#include "WindowsSupport/WindowsSupport.pkg"
#include "ExodusDeviceInterface/ExodusDeviceInterface.pkg"
#include "LoggingViewPresenter.h"
#include "YM2612/IYM2612.h"

class LoggingView :public ViewBase
{
public:
	//Constructors
	LoggingView(IUIManager& auiManager, LoggingViewPresenter& apresenter, IYM2612& amodel);

protected:
	//Member window procedure
	virtual INT_PTR WndProcDialog(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	//Event handlers
	INT_PTR msgWM_INITDIALOG(HWND hwnd, WPARAM wParam, LPARAM lParam);
	INT_PTR msgWM_COMMAND(HWND hwnd, WPARAM wParam, LPARAM lParam);

private:
	LoggingViewPresenter& presenter;
	IYM2612& model;
	bool initializedDialog;
	std::wstring previousText;
	unsigned int currentControlFocus;
};

#endif