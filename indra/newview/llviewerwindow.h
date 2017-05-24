/** 
 * @file llviewerwindow.h
 * @brief Description of the LLViewerWindow class.
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

//
// A note about X,Y coordinates:
//
// X coordinates are in pixels, from the left edge of the window client area
// Y coordinates are in pixels, from the BOTTOM edge of the window client area
//
// The Y coordinates therefore match OpenGL window coords, not Windows(tm) window coords.
// If Y is from the top, the variable will be called "y_from_top"

#ifndef LL_LLVIEWERWINDOW_H
#define LL_LLVIEWERWINDOW_H

#include "v3dmath.h"
#include "v2math.h"
#include "llcursortypes.h"
#include "llwindowcallbacks.h"
#include "lltimer.h"
#include "llmousehandler.h"
#include "llnotifications.h"
#include "llhandle.h"
#include "llinitparam.h"
#include "lltrace.h"
#include "llsnapshotmodel.h"

#include <boost/scoped_ptr.hpp>

class LLView;
class LLViewerObject;
class LLUUID;
class LLProgressView;
class LLTool;
class LLVelocityBar;
class LLPanel;
class LLImageRaw;
class LLImageFormatted;
class LLHUDIcon;
class LLWindow;
class LLRootView;
class LLWindowListener;
class LLViewerWindowListener;
class LLVOPartGroup;
class LLPopupView;

#define PICK_HALF_WIDTH 5
#define PICK_DIAMETER (2 * PICK_HALF_WIDTH + 1)

class LLPickInfo
{
public:
	typedef enum
	{
		PICK_OBJECT,
		PICK_FLORA,
		PICK_LAND,
		PICK_ICON,
		PICK_PARCEL_WALL,
		PICK_INVALID
	} EPickType;

public:
	LLPickInfo();
	LLPickInfo(const LLCoordGL& mouse_pos, 
		MASK keyboard_mask, 
		BOOL pick_transparent,
		BOOL pick_rigged,
		BOOL pick_particle,
		BOOL pick_surface_info,
		BOOL pick_unselectable,
		void (*pick_callback)(const LLPickInfo& pick_info));

	void fetchResults();
	LLPointer<LLViewerObject> getObject() const;
	LLUUID getObjectID() const { return mObjectID; }
	bool isValid() const { return mPickType != PICK_INVALID; }

	static bool isFlora(LLViewerObject* object);

public:
	LLCoordGL		mMousePt;
	MASK			mKeyMask;
	void			(*mPickCallback)(const LLPickInfo& pick_info);

	EPickType		mPickType;
	LLCoordGL		mPickPt;
	LLVector3d		mPosGlobal;
	LLVector3		mObjectOffset;
	LLUUID			mObjectID;
	LLUUID			mParticleOwnerID;
	LLUUID			mParticleSourceID;
	S32				mObjectFace;
	LLHUDIcon*		mHUDIcon;
	LLVector3       mIntersection;
	LLVector2		mUVCoords;
	LLVector2       mSTCoords;
	LLCoordScreen	mXYCoords;
	LLVector3		mNormal;
	LLVector4		mTangent;
	LLVector3		mBinormal;
	BOOL			mPickTransparent;
	BOOL			mPickRigged;
	BOOL			mPickParticle;
	BOOL			mPickUnselectable;
	void		    getSurfaceInfo();

private:
	void			updateXYCoords();

	BOOL			mWantSurfaceInfo;   // do we populate mUVCoord, mNormal, mBinormal?

};

static const U32 MAX_SNAPSHOT_IMAGE_SIZE = 6 * 1024; // max snapshot image size 6144 * 6144

class LLViewerWindow : public LLWindowCallbacks
{
public:
	//
	// CREATORS
	//
	struct Params : public LLInitParam::Block<Params>
	{
		Mandatory<std::string>		title,
									name;
		Mandatory<S32>				x,
									y,
									width,
									height,
									min_width,
									min_height;
		Optional<bool>				ignore_pixel_depth,
									first_run;
		Optional<U32>				window_mode;

		Params();
	};

	LLViewerWindow(const Params& p);
	virtual ~LLViewerWindow();

	void			shutdownViews();
	void			shutdownGL();
	
	void			initGLDefaults();
	void			initBase();
	void			adjustRectanglesForFirstUse(const LLRect& window);
	void            adjustControlRectanglesForFirstUse(const LLRect& window);
	void			initWorldUI();
	void			setUIVisibility(bool);
	bool			getUIVisibility();
	void			setWindowTitle(const std::string& title);

	BOOL handleAnyMouseClick(LLWindow *window,  LLCoordGL pos, MASK mask, LLMouseHandler::EClickType clicktype, BOOL down);

	//
	// LLWindowCallback interface implementation
	//
	/*virtual*/ BOOL handleTranslatedKeyDown(KEY key,  MASK mask, BOOL repeated) override;
	/*virtual*/ BOOL handleTranslatedKeyUp(KEY key,  MASK mask) override;
	/*virtual*/ void handleScanKey(KEY key, BOOL key_down, BOOL key_up, BOOL key_level) override;
	/*virtual*/ BOOL handleUnicodeChar(llwchar uni_char, MASK mask) override;	// NOT going to handle extended 
	/*virtual*/ BOOL handleMouseDown(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ BOOL handleMouseUp(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ BOOL handleCloseRequest(LLWindow *window) override;
	/*virtual*/ void handleQuit(LLWindow *window) override;
	/*virtual*/ BOOL handleRightMouseDown(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ BOOL handleRightMouseUp(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ BOOL handleMiddleMouseDown(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ BOOL handleMiddleMouseUp(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ LLWindowCallbacks::DragNDropResult handleDragNDrop(LLWindow *window, LLCoordGL pos, MASK mask, LLWindowCallbacks::DragNDropAction action, std::string data) override;
    /*virtual*/ void handleMouseMove(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ void handleMouseLeave(LLWindow *window) override;
	/*virtual*/ void handleResize(LLWindow *window, S32 width, S32 height) override;
	/*virtual*/ void handleFocus(LLWindow *window) override;
	/*virtual*/ void handleFocusLost(LLWindow *window) override;
	/*virtual*/ BOOL handleActivate(LLWindow *window, BOOL activated) override;
	/*virtual*/ BOOL handleActivateApp(LLWindow *window, BOOL activating) override;
	/*virtual*/ void handleMenuSelect(LLWindow *window,  S32 menu_item) override;
	/*virtual*/ BOOL handlePaint(LLWindow *window,  S32 x,  S32 y,  S32 width,  S32 height) override;
	/*virtual*/ void handleScrollWheel(LLWindow *window,  S32 clicks) override;
	/*virtual*/ BOOL handleDoubleClick(LLWindow *window,  LLCoordGL pos, MASK mask) override;
	/*virtual*/ void handleWindowBlock(LLWindow *window) override;
	/*virtual*/ void handleWindowUnblock(LLWindow *window) override;
	/*virtual*/ void handleDataCopy(LLWindow *window, S32 data_type, void *data) override;
	/*virtual*/ BOOL handleTimerEvent(LLWindow *window) override;
	/*virtual*/ BOOL handleDeviceChange(LLWindow *window) override;
	/*virtual*/ BOOL handleDPIChanged(LLWindow *window, F32 ui_scale_factor, S32 window_width, S32 window_height) override;

	/*virtual*/ void handlePingWatchdog(LLWindow *window, const char * msg) override;
	/*virtual*/ void handlePauseWatchdog(LLWindow *window) override;
	/*virtual*/ void handleResumeWatchdog(LLWindow *window) override;
	/*virtual*/ std::string translateString(const char* tag) override;
	/*virtual*/ std::string translateString(const char* tag,
					const std::map<std::string, std::string>& args) override;
	
	// signal on update of WorldView rect
	typedef std::function<void (LLRect old_world_rect, LLRect new_world_rect)> world_rect_callback_t;
	typedef boost::signals2::signal<void (LLRect old_world_rect, LLRect new_world_rect)> world_rect_signal_t;
	world_rect_signal_t mOnWorldViewRectUpdated;
	boost::signals2::connection setOnWorldViewRectUpdated(world_rect_callback_t cb) { return mOnWorldViewRectUpdated.connect(cb); }

	//
	// ACCESSORS
	//
	LLRootView*			getRootView()		const;

	// 3D world area in scaled pixels (via UI scale), use for most UI computations
	LLRect			getWorldViewRectScaled() const;
	S32				getWorldViewHeightScaled() const;
	S32				getWorldViewWidthScaled() const;

	// 3D world area, in raw unscaled pixels
	LLRect			getWorldViewRectRaw() const		{ return mWorldViewRectRaw; }
	S32 			getWorldViewHeightRaw() const;
	S32 			getWorldViewWidthRaw() const;

	// Window in scaled pixels (via UI scale), use for most UI computations
	LLRect			getWindowRectScaled() const		{ return mWindowRectScaled; }
	S32				getWindowHeightScaled() const;
	S32				getWindowWidthScaled() const;

	// Window in raw pixels as seen on screen.
	LLRect			getWindowRectRaw() const		{ return mWindowRectRaw; }
	S32				getWindowHeightRaw() const;
	S32				getWindowWidthRaw() const;

	LLWindow*		getWindow()			const	{ return mWindow; }
	void*			getPlatformWindow() const;
	void*			getMediaWindow() 	const;
	void			focusClient()		const;

	LLCoordGL		getLastMouse()		const	{ return mLastMousePoint; }
	S32				getLastMouseX()		const	{ return mLastMousePoint.mX; }
	S32				getLastMouseY()		const	{ return mLastMousePoint.mY; }
	LLCoordGL		getCurrentMouse()		const	{ return mCurrentMousePoint; }
	S32				getCurrentMouseX()		const	{ return mCurrentMousePoint.mX; }
	S32				getCurrentMouseY()		const	{ return mCurrentMousePoint.mY; }
	S32				getCurrentMouseDX()		const	{ return mCurrentMouseDelta.mX; }
	S32				getCurrentMouseDY()		const	{ return mCurrentMouseDelta.mY; }
	LLCoordGL		getCurrentMouseDelta()	const	{ return mCurrentMouseDelta; }
	static LLTrace::SampleStatHandle<>*	getMouseVelocityStat()		{ return &sMouseVelocityStat; }
	BOOL			getLeftMouseDown()	const	{ return mLeftMouseDown; }
	BOOL			getMiddleMouseDown()	const	{ return mMiddleMouseDown; }
	BOOL			getRightMouseDown()	const	{ return mRightMouseDown; }

	const LLPickInfo&	getLastPick() const { return mLastPick; }

	void			setup2DViewport(S32 x_offset = 0, S32 y_offset = 0);
	void			setup3DViewport(S32 x_offset = 0, S32 y_offset = 0);
	void			setup3DRender();
	void			setup2DRender();

	LLVector3		mouseDirectionGlobal(const S32 x, const S32 y) const;
	LLVector3		mouseDirectionCamera(const S32 x, const S32 y) const;
	LLVector3       mousePointHUD(const S32 x, const S32 y) const;
		

	// Is window of our application frontmost?
	BOOL			getActive() const			{ return mActive; }

	const std::string&	getInitAlert() { return mInitAlert; }
	
	//
	// MANIPULATORS
	//
	void			saveLastMouse(const LLCoordGL &point);

	void			setCursor( ECursorType c );
	void			showCursor();
	void			hideCursor();
	BOOL            getCursorHidden() { return mCursorHidden; }
	void			moveCursorToCenter();								// move to center of window
													
	void			setShowProgress(const BOOL show);
	BOOL			getShowProgress() const;
	void			setProgressString(const std::string& string);
	void			setProgressPercent(const F32 percent);
	void			setProgressMessage(const std::string& msg);
	void			setProgressCancelButtonVisible( BOOL b, const std::string& label = LLStringUtil::null );
	LLProgressView *getProgressView() const;
	void			revealIntroPanel();
	void			setStartupComplete();

	void			updateObjectUnderCursor();

	void			updateUI();		// Once per frame, update UI based on mouse position, calls following update* functions
	void			updateLayout();
	void			updateMouseDelta();
	void			updateKeyboardFocus();

	void			updateWorldViewRect(bool use_full_window=false);
	LLView*			getToolBarHolder() { return mToolBarHolder.get(); }
	LLView*			getHintHolder() { return mHintHolder.get(); }
	LLView*			getLoginPanelHolder() { return mLoginPanelHolder.get(); }
	BOOL			handleKey(KEY key, MASK mask);
	BOOL			handleKeyUp(KEY key, MASK mask);
	void			handleScrollWheel	(S32 clicks);

	// add and remove views from "popup" layer
	void			addPopup(LLView* popup);
	void			removePopup(LLView* popup);
	void			clearPopups();

	// Hide normal UI when a logon fails, re-show everything when logon is attempted again
	void			setNormalControlsVisible( BOOL visible );
	void			setMenuBackgroundColor(bool god_mode = false);

	void			reshape(S32 width, S32 height);
	void			sendShapeToSim();

	void			draw();
	void			updateDebugText();
	void			drawDebugText();

	static void		loadUserImage(void **cb_data, const LLUUID &uuid);

	static void		movieSize(S32 new_width, S32 new_height);

	// snapshot functionality.
	// perhaps some of this should move to llfloatershapshot?  -MG

	BOOL			saveSnapshot(const std::string&  filename, S32 image_width, S32 image_height, BOOL show_ui = TRUE, BOOL do_rebuild = FALSE, LLSnapshotModel::ESnapshotLayerType type = LLSnapshotModel::SNAPSHOT_TYPE_COLOR);
	BOOL			rawSnapshot(LLImageRaw *raw, S32 image_width, S32 image_height, BOOL keep_window_aspect = TRUE, BOOL is_texture = FALSE,
		BOOL show_ui = TRUE, BOOL do_rebuild = FALSE, LLSnapshotModel::ESnapshotLayerType type = LLSnapshotModel::SNAPSHOT_TYPE_COLOR, S32 max_size = MAX_SNAPSHOT_IMAGE_SIZE);
	BOOL			thumbnailSnapshot(LLImageRaw *raw, S32 preview_width, S32 preview_height, BOOL show_ui, BOOL do_rebuild, LLSnapshotModel::ESnapshotLayerType type);
	BOOL			isSnapshotLocSet() const { return ! sSnapshotDir.empty(); }
	void			resetSnapshotLoc() const { sSnapshotDir.clear(); }
	BOOL		    saveImageNumbered(LLImageFormatted *image, bool force_picker = false);

	// Reset the directory where snapshots are saved.
	// Client will open directory picker on next snapshot save.
	void resetSnapshotLoc();

	void			playSnapshotAnimAndSound();
	
	// draws selection boxes around selected objects, must call displayObjects first
	void			renderSelections( BOOL for_gl_pick, BOOL pick_parcel_walls, BOOL for_hud );
	void			performPick();
	void			returnEmptyPicks();

	void			pickAsync(	S32 x,
								S32 y_from_bot,
								MASK mask,
								void (*callback)(const LLPickInfo& pick_info),
								BOOL pick_transparent = FALSE,
								BOOL pick_rigged = FALSE,
								BOOL pick_unselectable = FALSE);
	LLPickInfo		pickImmediate(S32 x, S32 y, BOOL pick_transparent, BOOL pick_rigged = FALSE, BOOL pick_particle = FALSE);
	LLHUDIcon* cursorIntersectIcon(S32 mouse_x, S32 mouse_y, F32 depth,
										   LLVector4a* intersection);

	LLViewerObject* cursorIntersect(S32 mouse_x = -1, S32 mouse_y = -1, F32 depth = 512.f,
									LLViewerObject *this_object = nullptr,
									S32 this_face = -1,
									BOOL pick_transparent = FALSE,
									BOOL pick_rigged = FALSE,
									S32* face_hit = nullptr,
									LLVector4a *intersection = nullptr,
									LLVector2 *uv = nullptr,
									LLVector4a *normal = nullptr,
									LLVector4a *tangent = nullptr,
									LLVector4a* start = nullptr,
									LLVector4a* end = nullptr);
	
	
	// Returns a pointer to the last object hit
	//LLViewerObject	*getObject();
	//LLViewerObject  *lastNonFloraObjectHit();

	//const LLVector3d& getObjectOffset();
	//const LLVector3d& lastNonFloraObjectHitOffset();

	// mousePointOnLand() returns true if found point
	BOOL			mousePointOnLandGlobal(const S32 x, const S32 y, LLVector3d *land_pos_global, BOOL ignore_distance = FALSE);
	BOOL			mousePointOnPlaneGlobal(LLVector3d& point, const S32 x, const S32 y, const LLVector3d &plane_point, const LLVector3 &plane_normal);
	LLVector3d		clickPointInWorldGlobal(const S32 x, const S32 y_from_bot, LLViewerObject* clicked_object) const;
	BOOL			clickPointOnSurfaceGlobal(const S32 x, const S32 y, LLViewerObject *objectp, LLVector3d &point_global) const;

	// Prints window implementation details
	void			dumpState();

	// handle shutting down GL and bringing it back up
	void			requestResolutionUpdate();
	void			checkSettings();
	void			restartDisplay(BOOL show_progress_bar);
	BOOL			changeDisplaySettings(LLCoordScreen size, BOOL disable_vsync, BOOL show_progress_bar);
	BOOL			getIgnoreDestroyWindow() { return mIgnoreActivate; }
	F32				getWorldViewAspectRatio() const;
	const LLVector2& getDisplayScale() const { return mDisplayScale; }
	void			calcDisplayScale();
	static LLRect 	calcScaledRect(const LLRect & rect, const LLVector2& display_scale);

	bool getSystemUIScaleFactorChanged() { return mSystemUIScaleFactorChanged; }
	static void showSystemUIScaleFactorChanged();

private:
	bool                    shouldShowToolTipFor(LLMouseHandler *mh);

	void			switchToolByMask(MASK mask);
	void			destroyWindow();
	void			drawMouselookInstructions();
	void			stopGL(BOOL save_state = TRUE);
	void			restoreGL(const std::string& progress_message = LLStringUtil::null);
	void			initFonts(F32 zoom_factor = 1.f);
	void			schedulePick(LLPickInfo& pick_info);
	S32				getChatConsoleBottomPad(); // Vertical padding for child console rect, varied by bottom clutter
	LLRect			getChatConsoleRect(); // Get optimal cosole rect.

	static bool onSystemUIScaleFactorChanged(const LLSD& notification, const LLSD& response);
private:
	LLWindow*		mWindow;						// graphical window object
	bool			mActive;
	bool			mUIVisible;

	LLNotificationChannelPtr mSystemChannel;
	LLNotificationChannelPtr mCommunicationChannel;
	LLNotificationChannelPtr mAlertsChannel;
	LLNotificationChannelPtr mModalAlertsChannel;

	LLRect			mWindowRectRaw;				// whole window, including UI
	LLRect			mWindowRectScaled;			// whole window, scaled by UI size
	LLRect			mWorldViewRectRaw;			// area of screen for 3D world
	LLRect			mWorldViewRectScaled;		// area of screen for 3D world scaled by UI size
	LLRootView*		mRootView;					// a view of size mWindowRectRaw, containing all child views
	LLVector2		mDisplayScale;

	LLCoordGL		mCurrentMousePoint;			// last mouse position in GL coords
	LLCoordGL		mLastMousePoint;		// Mouse point at last frame.
	LLCoordGL		mCurrentMouseDelta;		//amount mouse moved this frame
	BOOL			mLeftMouseDown;
	BOOL			mMiddleMouseDown;
	BOOL			mRightMouseDown;

	LLProgressView	*mProgressView;

	LLFrameTimer	mToolTipFadeTimer;
	LLPanel*		mToolTip;
	std::string		mLastToolTipMessage;
	LLRect			mToolTipStickyRect;			// Once a tool tip is shown, it will stay visible until the mouse leaves this rect.

	BOOL			mMouseInWindow;				// True if the mouse is over our window or if we have captured the mouse.
	BOOL			mFocusCycleMode;
	typedef std::set<LLHandle<LLView> > view_handle_set_t;
	view_handle_set_t mMouseHoverViews;

	// Variables used for tool override switching based on modifier keys.  JC
	MASK			mLastMask;			// used to detect changes in modifier mask
	LLTool*			mToolStored;		// the tool we're overriding
	BOOL			mHideCursorPermanent;	// true during drags, mouselook
	BOOL            mCursorHidden;
	LLPickInfo		mLastPick;
	std::vector<LLPickInfo> mPicks;
	LLRect			mPickScreenRegion; // area of frame buffer for rendering pick frames (generally follows mouse to avoid going offscreen)
	LLTimer         mPickTimer;        // timer for scheduling n picks per second

	std::string		mOverlayTitle;		// Used for special titles such as "Second Life - Special E3 2003 Beta"

	BOOL			mIgnoreActivate;

	std::string		mInitAlert;			// Window / GL initialization requires an alert

	LLHandle<LLView> mWorldViewPlaceholder;	// widget that spans the portion of screen dedicated to rendering the 3d world
	LLHandle<LLView> mToolBarHolder;		// container for toolbars
	LLHandle<LLView> mHintHolder;			// container for hints
	LLHandle<LLView> mLoginPanelHolder;		// container for login panel
	LLPopupView*	mPopupView;			// container for transient popups
	LLHandle<LLPanel> mStatusBarPanel;
	
	class LLDebugText* mDebugText; // Internal class for debug text
	
	bool			mResDirty;
	bool			mStatesDirty;
	U32			mCurrResolutionIndex;

	boost::scoped_ptr<LLWindowListener> mWindowListener;
	boost::scoped_ptr<LLViewerWindowListener> mViewerWindowListener;

	static std::string sSnapshotBaseName;
	static std::string sSnapshotDir;

	static std::string sMovieBaseName;
	
	// Object temporarily hovered over while dragging
	LLPointer<LLViewerObject>	mDragHoveredObject;

	static LLTrace::SampleStatHandle<>	sMouseVelocityStat;
	bool mSystemUIScaleFactorChanged; // system UI scale factor changed from last run
};

//
// Globals
//

extern LLViewerWindow*	gViewerWindow;

extern LLFrameTimer		gAwayTimer;				// tracks time before setting the avatar away state to true
extern LLFrameTimer		gAwayTriggerTimer;		// how long the avatar has been away

extern LLViewerObject*  gDebugRaycastObject;
extern LLVector4a       gDebugRaycastIntersection;
extern LLVOPartGroup*	gDebugRaycastParticle;
extern LLVector4a		gDebugRaycastParticleIntersection;
extern LLVector2        gDebugRaycastTexCoord;
extern LLVector4a       gDebugRaycastNormal;
extern LLVector4a       gDebugRaycastTangent;
extern S32				gDebugRaycastFaceHit;
extern LLVector4a		gDebugRaycastStart;
extern LLVector4a		gDebugRaycastEnd;

extern BOOL			gDisplayCameraPos;
extern BOOL			gDisplayWindInfo;
extern BOOL			gDisplayFOV;
extern BOOL			gDisplayBadge;

#endif
