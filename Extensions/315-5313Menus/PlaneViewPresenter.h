#ifndef __PLANEVIEWPRESENTER_H__
#define __PLANEVIEWPRESENTER_H__
#include "DeviceInterface/DeviceInterface.pkg"
#include "S315_5313Menus.h"
#include "315-5313/IS315_5313.h"

class PlaneViewPresenter :public ViewPresenterBase
{
public:
	// Constructors
	PlaneViewPresenter(const std::wstring& viewGroupName, const std::wstring& viewName, int viewID, S315_5313Menus& owner, const IDevice& modelInstanceKey, IS315_5313& model);

	// View title functions
	static std::wstring GetUnqualifiedViewTitle();

	// View creation and deletion
	virtual IView* CreateView(IUIManager& uiManager);
	virtual void DeleteView(IView* view);

	// System interface functions
	ISystemExtensionInterface& GetSystemInterface() const;

private:
	S315_5313Menus& _owner;
	const IDevice& _modelInstanceKey;
	IS315_5313& _model;
};

#endif
