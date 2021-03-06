#ifndef __IMENUSEGMENT_H__
#define __IMENUSEGMENT_H__
#include "MarshalSupport/MarshalSupport.pkg"
#include "IMenuItem.h"
#include <string>
#include <list>
class IMenuSubmenu;
class IMenuSelectableOption;
class IMenuHandler;
using namespace MarshalSupport::Operators;

class IMenuSegment :public IMenuItem
{
public:
	// Enumerations
	enum SortMode
	{
		SORTMODE_ADDITIONORDER,
		SORTMODE_TITLE
	};

public:
	// Constructors
	inline virtual ~IMenuSegment() = 0;

	// Interface version functions
	static inline unsigned int ThisIMenuSegmentVersion() { return 1; }
	virtual unsigned int GetIMenuSegmentVersion() const = 0;

	// Menu title functions
	virtual Marshal::Ret<std::wstring> GetMenuSortTitle() const = 0;

	// Sort mode functions
	virtual SortMode GetSortMode() const = 0;

	// Separator functions
	virtual bool GetSurroundWithSeparators() const = 0;

	// Item management functions
	virtual bool NoMenuItemsExist() const = 0;
	virtual Marshal::Ret<std::list<IMenuItem*>> GetMenuItems() const = 0;
	virtual Marshal::Ret<std::list<IMenuItem*>> GetSortedMenuItems() const = 0;

	// Menu item creation and deletion
	virtual IMenuSegment& AddMenuItemSegment(bool surroundWithSeparators = true, IMenuSegment::SortMode sortMode = IMenuSegment::SORTMODE_ADDITIONORDER) = 0;
	virtual IMenuSubmenu& AddMenuItemSubmenu(const Marshal::In<std::wstring>& title) = 0;
	virtual IMenuSelectableOption& AddMenuItemSelectableOption(IMenuHandler& menuHandler, int menuItemID, const Marshal::In<std::wstring>& title) = 0;
	virtual void DeleteMenuItem(IMenuItem& menuItem) = 0;
	virtual void DeleteAllMenuItems() = 0;
};
IMenuSegment::~IMenuSegment() { }

#endif
