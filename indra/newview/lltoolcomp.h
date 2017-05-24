/** 
 * @file lltoolcomp.h
 * @brief Composite tools
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_TOOLCOMP_H
#define LL_TOOLCOMP_H

#include "lltool.h"

class LLManip;
class LLToolSelectRect;
class LLToolPlacer;
class LLPickInfo;

class LLView;
class LLTextBox;

//-----------------------------------------------------------------------
// LLToolComposite

class LLToolComposite : public LLTool
{
public:
	LLToolComposite(const std::string& name);

	BOOL			handleMouseDown(S32 x, S32 y, MASK mask) override = 0;	// Sets the current tool
	BOOL			handleMouseUp(S32 x, S32 y, MASK mask) override;			// Returns to the default tool
	BOOL			handleDoubleClick(S32 x, S32 y, MASK mask) override = 0;

	// Map virtual functions to the currently active internal tool
	BOOL			handleHover(S32 x, S32 y, MASK mask) override { return mCur->handleHover( x, y, mask ); }
	BOOL			handleScrollWheel(S32 x, S32 y, S32 clicks) override { return mCur->handleScrollWheel( x, y, clicks ); }
	BOOL			handleRightMouseDown(S32 x, S32 y, MASK mask) override { return mCur->handleRightMouseDown( x, y, mask ); }

	LLViewerObject*	getEditingObject() override { return mCur->getEditingObject(); }
	LLVector3d		getEditingPointGlobal() override { return mCur->getEditingPointGlobal(); }
	BOOL			isEditing() override { return mCur->isEditing(); }

	void			stopEditing() override
	{ mCur->stopEditing(); mCur = mDefault; }

	BOOL			clipMouseWhenDown() override { return mCur->clipMouseWhenDown(); }

	void			handleSelect() override;
	void			handleDeselect() override;

	void			render() override { mCur->render(); }
	void			draw() override { mCur->draw(); }

	BOOL			handleKey(KEY key, MASK mask) override { return mCur->handleKey( key, mask ); }

	void			onMouseCaptureLost() override;

	void			screenPointToLocal(S32 screen_x, S32 screen_y, S32* local_x, S32* local_y) const override
	{ mCur->screenPointToLocal(screen_x, screen_y, local_x, local_y); }

	void			localPointToScreen(S32 local_x, S32 local_y, S32* screen_x, S32* screen_y) const override
	{ mCur->localPointToScreen(local_x, local_y, screen_x, screen_y); }

	BOOL					isSelecting();
	LLTool*					getCurrentTool()								{ return mCur; }

protected:
	void					setCurrentTool( LLTool* new_tool );
	// In hover handler, call this to auto-switch tools
	void					setToolFromMask( MASK mask, LLTool *normal );

protected:
	LLTool*					mCur;		// The tool to which we're delegating.
	LLTool*					mDefault;
	BOOL					mSelected;
	BOOL					mMouseDown;
	LLManip*				mManip;
	LLToolSelectRect*		mSelectRect;

public:
	static const std::string sNameComp;
};


//-----------------------------------------------------------------------
// LLToolCompTranslate

class LLToolCompInspect : public LLToolComposite, public LLSingleton<LLToolCompInspect>
{
	LLSINGLETON(LLToolCompInspect);
	virtual ~LLToolCompInspect();
public:

	// Overridden from LLToolComposite
	BOOL		handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL		handleMouseUp(S32 x, S32 y, MASK mask) override;
	BOOL		handleDoubleClick(S32 x, S32 y, MASK mask) override;
	BOOL		handleKey(KEY key, MASK mask) override;
	void		onMouseCaptureLost() override;
			void		keyUp(KEY key, MASK mask);

	static void pickCallback(const LLPickInfo& pick_info);

	BOOL isToolCameraActive() const { return mIsToolCameraActive; }

private:
	BOOL mIsToolCameraActive;
};

//-----------------------------------------------------------------------
// LLToolCompTranslate

class LLToolCompTranslate : public LLToolComposite, public LLSingleton<LLToolCompTranslate>
{
	LLSINGLETON(LLToolCompTranslate);
	virtual ~LLToolCompTranslate();
public:

	// Overridden from LLToolComposite
	BOOL		handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL		handleDoubleClick(S32 x, S32 y, MASK mask) override;
	BOOL		handleHover(S32 x, S32 y, MASK mask) override;
	BOOL		handleMouseUp(S32 x, S32 y, MASK mask) override;			// Returns to the default tool
	void		render() override;

	LLTool*		getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

//-----------------------------------------------------------------------
// LLToolCompScale

class LLToolCompScale : public LLToolComposite, public LLSingleton<LLToolCompScale>
{
	LLSINGLETON(LLToolCompScale);
	virtual ~LLToolCompScale();
public:

	// Overridden from LLToolComposite
	BOOL		handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL		handleDoubleClick(S32 x, S32 y, MASK mask) override;
	BOOL		handleHover(S32 x, S32 y, MASK mask) override;
	BOOL		handleMouseUp(S32 x, S32 y, MASK mask) override;			// Returns to the default tool
	void		render() override;

	LLTool*		getOverrideTool(MASK mask) override;
	
	static void pickCallback(const LLPickInfo& pick_info);
};


//-----------------------------------------------------------------------
// LLToolCompRotate

class LLToolCompRotate : public LLToolComposite, public LLSingleton<LLToolCompRotate>
{
	LLSINGLETON(LLToolCompRotate);
	virtual ~LLToolCompRotate();
public:

	// Overridden from LLToolComposite
	BOOL		handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL		handleDoubleClick(S32 x, S32 y, MASK mask) override;
	BOOL		handleHover(S32 x, S32 y, MASK mask) override;
	BOOL		handleMouseUp(S32 x, S32 y, MASK mask) override;
	void		render() override;

	LLTool*		getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);

protected:
};

//-----------------------------------------------------------------------
// LLToolCompCreate

class LLToolCompCreate : public LLToolComposite, public LLSingleton<LLToolCompCreate>
{
	LLSINGLETON(LLToolCompCreate);
	virtual ~LLToolCompCreate();
public:

	// Overridden from LLToolComposite
	BOOL		handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL		handleDoubleClick(S32 x, S32 y, MASK mask) override;
	BOOL		handleMouseUp(S32 x, S32 y, MASK mask) override;
	
	static void pickCallback(const LLPickInfo& pick_info);
protected:
	LLToolPlacer*		mPlacer;
	BOOL				mObjectPlacedOnMouseDown;
};


//-----------------------------------------------------------------------
// LLToolCompGun

class LLToolGun;
class LLToolGrabBase;
class LLToolSelect;

class LLToolCompGun : public LLToolComposite, public LLSingleton<LLToolCompGun>
{
	LLSINGLETON(LLToolCompGun);
	virtual ~LLToolCompGun();
public:

	void			draw() override;

	// Overridden from LLToolComposite
	BOOL			handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL			handleMouseUp(S32 x, S32 y, MASK mask) override;
	BOOL			handleDoubleClick(S32 x, S32 y, MASK mask) override;
	BOOL			handleHover(S32 x, S32 y, MASK mask) override;
	BOOL			handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL			handleRightMouseUp(S32 x, S32 y, MASK mask) override;
	BOOL			handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	void			onMouseCaptureLost() override;
	void			handleSelect() override;
	void			handleDeselect() override;
	LLTool*			getOverrideTool(MASK mask) override { return nullptr; }

protected:
	LLToolGun*			mGun;
	LLToolGrabBase*		mGrab;
	LLTool*				mNull;

	bool				mRightMouseDown;
	LLTimer				mTimerFOV;
	F32				mOriginalFOV,
					mStartFOV,
					mTargetFOV;
};


#endif  // LL_TOOLCOMP_H
