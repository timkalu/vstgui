
#include "win32optionmenu.h"

#if WINDOWS && VSTGUI_PLATFORM_ABSTRACTION

#include "win32support.h"
#include "../../controls/coptionmenu.h"
#include "../../cframe.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
Win32OptionMenu::Win32OptionMenu (HWND windowHandle)
: windowHandle (windowHandle)
{
}

//------------------------------------------------------------------------
COptionMenu* getItemMenu (long idx, long &idxInMenu, long &offsetIdx, COptionMenu* _menu)
{
	long oldIDx = offsetIdx;
	offsetIdx += _menu->getNbEntries ();

	if (idx < offsetIdx)
	{
		idxInMenu = idx - oldIDx;
		return _menu;
	}
	

	COptionMenu *menu = 0;
	CMenuItemIterator it = _menu->getItems ()->begin ();
	while (it != _menu->getItems ()->end ())
	{
		if ((*it)->getSubmenu ())
		{
			menu = getItemMenu (idx, idxInMenu, offsetIdx, (*it)->getSubmenu ());
			if (menu)
				break;
		}
		it++;
	}
	return menu;
}

//-----------------------------------------------------------------------------
PlatformOptionMenuResult Win32OptionMenu::popup (COptionMenu* optionMenu)
{
	PlatformOptionMenuResult result = {0};
	
	//---Transform local coordinates to global coordinates
	CRect rect (optionMenu->getViewSize ());
	CPoint p (0, 0);
	optionMenu->localToFrame (p);
	rect.offset (p.x, p.y);

	long offset;

	if (optionMenu->getStyle () & kPopupStyle)
		offset = 0;
	else
		offset = (long)optionMenu->getViewSize ().getHeight ();

	CCoord gx = 0, gy = 0;
	optionMenu->getFrame()->getPosition (gx, gy);
	gy += rect.top + offset;
	gx += rect.left;

	long offsetIndex = 0;
	HMENU menu = createMenu (optionMenu, offsetIndex);
	if (menu)
	{
		int flags = TPM_LEFTALIGN;

// do we need the following ?
//		if (lastButton & kRButton)
//			flags |= TPM_RIGHTBUTTON;

		if (TrackPopupMenu (menu, flags, (int)gx, (int)gy, 0, windowHandle, 0))
		{
			MSG msg;
			if (PeekMessage (&msg, windowHandle, WM_COMMAND, WM_COMMAND, PM_REMOVE))
			{
				if (HIWORD (msg.wParam) == 0)
				{
					long res = LOWORD (msg.wParam);
					if (res != -1)
					{
						long idx = 0;
						offsetIndex = 0;
						COptionMenu* resultMenu = getItemMenu (res, idx, offsetIndex, optionMenu);
						if (resultMenu)
						{
							result.menu = resultMenu;
							result.index = idx;
						}
					}
				}
			}
		}
		DestroyMenu (menu);
	}
	return result;
}

//-----------------------------------------------------------------------------
HMENU Win32OptionMenu::createMenu (COptionMenu* _menu, long& offsetIdx)
{
	HMENU menu = CreatePopupMenu ();

	bool multipleCheck = _menu->getStyle () & (kMultipleCheckStyle & ~kCheckStyle);

	int flags = 0;
	long idxSubmenu = 0;
	long offset = offsetIdx;
	long nbEntries = _menu->getNbEntries ();
	offsetIdx += nbEntries;
	long inc = 0;
	CMenuItemIterator it = _menu->getItems ()->begin ();
	while (it != _menu->getItems ()->end ())
	{
		CMenuItem* item = (*it);
		if (item->isSeparator ())
		{
			AppendMenu (menu, MF_SEPARATOR, 0, 0);
		}
		else
		{
			char* titleWithPrefixNumbers = 0;
			if (_menu->getPrefixNumbers ())
			{
				titleWithPrefixNumbers = (char*)malloc (strlen (item->getTitle ()) + 50);
				switch (_menu->getPrefixNumbers ())
				{
					case 2:
					{
						sprintf (titleWithPrefixNumbers, "%1d %s", inc+1, item->getTitle ());
						break;
					}
					case 3:
					{
						sprintf (titleWithPrefixNumbers, "%02d %s", inc+1, item->getTitle ());
						break;
					}
					case 4:
					{
						sprintf (titleWithPrefixNumbers, "%03d %s", inc+1, item->getTitle ());
						break;
					}
				}
			}
			UTF8StringHelper entryText (titleWithPrefixNumbers ? titleWithPrefixNumbers : item->getTitle ());
			flags = MF_STRING;
			if (nbEntries < 160 && _menu->getNbItemsPerColumn () > 0 && inc && !(inc % _menu->getNbItemsPerColumn ()))
				flags |= MF_MENUBARBREAK;

			if (item->getSubmenu ())
			{
				HMENU submenu = createMenu (item->getSubmenu (), offsetIdx);
				if (submenu)
				{
					AppendMenu (menu, flags|MF_POPUP|MF_ENABLED, (UINT_PTR)submenu, (const TCHAR*)entryText);
				}
			}
			else
			{
				if (item->isEnabled ())
					flags |= MF_ENABLED;
				else
					flags |= MF_GRAYED;
				if (item->isTitle ())
					flags |= MF_DISABLED;
				if (multipleCheck && item->isChecked ())
					flags |= MF_CHECKED;
				if (_menu->getStyle () & kCheckStyle && inc == _menu->getCurrentIndex () && item->isChecked ())
					flags |= MF_CHECKED;
				if (!(flags & MF_CHECKED))
					flags |= MF_UNCHECKED;
				AppendMenu (menu, flags, offset + inc, entryText);
				#if 0 // TODO
				if (item->getIcon ())
				{
					MENUITEMINFO mInfo = {0};
					mInfo.cbSize = sizeof (MENUITEMINFO);
					mInfo.fMask = MIIM_BITMAP;
					#if GDIPLUS
					Gdiplus::Bitmap* bitmap = item->getIcon ()->getBitmap ();
					HBITMAP hBmp = NULL;
					bitmap->GetHBITMAP (Gdiplus::Color (0, 0, 0, 0), &hBmp);
					mInfo.hbmpItem = hBmp;
					#else
					mInfo.hbmpItem = item->getIcon ()->getHandle ();
					#endif
					SetMenuItemInfo (menu, offset + inc, TRUE, &mInfo);
				}
				#endif
			}
			if (titleWithPrefixNumbers)
				free (titleWithPrefixNumbers);
		}
		inc++;
		it++;
	}
	return menu;
}

} // namespace

#endif // WINDOWS && VSTGUI_PLATFORM_ABSTRACTION
