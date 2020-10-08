#include "MemoryWatcherViewPresenter.h"
#include "MemoryWatcherView.h"

MemoryWatcherViewPresenter::MemoryWatcherViewPresenter(const std::wstring& viewGroupName, const std::wstring& viewName, int viewID, MemoryMenus& owner, const IDevice& modelInstanceKey, IMemory& model)
:ViewPresenterBase(owner.GetAssemblyHandle(), viewGroupName, viewName, viewID, modelInstanceKey.GetDeviceInstanceName(), modelInstanceKey.GetDeviceModuleID(), modelInstanceKey.GetModuleDisplayName()), _owner(owner), _modelInstanceKey(modelInstanceKey), _model(model)

{
}

std::wstring MemoryWatcherViewPresenter::GetUnqualifiedViewTitle()
{
	return L"Memory Watcher";
}

IView* MemoryWatcherViewPresenter::CreateView(IUIManager& uiManager)
{
	return new MemoryWatcherView(uiManager,*this,_model);
}

void MemoryWatcherViewPresenter::DeleteView(IView* view)
{
	delete view;
}
