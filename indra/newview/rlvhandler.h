/**
 *
 * Copyright (c) 2009-2018, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#ifndef RLV_HANDLER_H
#define RLV_HANDLER_H

#include "llgroupmgr.h"
#include "llviewertexture.h"
#include <stack>

#include "rlvcommon.h"
#include "rlvhelper.h"


 // ============================================================================
 // Forward declarations
 //


// ============================================================================

class RlvHandler : public LLOldEvents::LLSimpleListener, public LLParticularGroupObserver
{
	// Temporary LLSingleton look-alike
public:
	static RlvHandler& instance();
	static RlvHandler* getInstance();

public:
	RlvHandler();
	~RlvHandler();

	// --------------------------------

	/*
	 * Rule checking functions
	 */
	// NOTE: - to check @detach=n    -> (see RlvAttachmentLocks)
	//       - to check @addattach=n -> (see RlvAttachmentLocks)
	//       - to check @remattach=n -> (see RlvAttachmentLocks)
	//       - to check @addoutfit=n -> (see RlvWearableLocks)
	//       - to check @remoutfit=n -> (see RlvWearableLocks)
	//       - to check exceptions   -> isException()
public:
	// Returns a list of all objects containing the specified behaviour
	bool findBehaviour(ERlvBehaviour eBhvr, std::list<const RlvObject*>& lObjects) const;
	// Returns TRUE is at least one object contains the specified behaviour (and optional option)
	bool hasBehaviour(ERlvBehaviour eBhvr) const { return (eBhvr < RLV_BHVR_COUNT) ? (0 != m_Behaviours[eBhvr]) : false; }
	bool hasBehaviour(ERlvBehaviour eBhvr, const std::string& strOption) const;
	// Returns TRUE if the specified object has at least one active behaviour
	bool hasBehaviour(const LLUUID& idObj) { return m_Objects.end() != m_Objects.find(idObj);  }
	// Returns TRUE if the specified object contains the specified behaviour (and optional option)
	bool hasBehaviour(const LLUUID& idObj, ERlvBehaviour eBhvr, const std::string& strOption = LLStringUtil::null) const;
	// Returns TRUE if at least one object (except the specified one) contains the specified behaviour (and optional option)
	bool hasBehaviourExcept(ERlvBehaviour eBhvr, const LLUUID& idObj) const;
	bool hasBehaviourExcept(ERlvBehaviour eBhvr, const std::string& strOption, const LLUUID& idObj) const;
	// Returns TRUE if at least one object in the linkset with specified root ID contains the specified behaviour (and optional option)
	bool hasBehaviourRoot(const LLUUID& idObjRoot, ERlvBehaviour eBhvr, const std::string& strOption = LLStringUtil::null) const;
	// Returns TRUE if the specified object is the only object containing the specified behaviour
	bool ownsBehaviour(const LLUUID& idObj, ERlvBehaviour eBhvr) const;

	// Adds or removes an exception for the specified behaviour
	void addException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption);
	void removeException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption);
	// Returns TRUE if the specified behaviour has an added exception
	bool hasException(ERlvBehaviour eBhvr) const;
	// Returns TRUE if the specified option was added as an exception for the specified behaviour
	bool isException(ERlvBehaviour eBhvr, const RlvExceptionOption& varOption, ERlvExceptionCheck typeCheck = RLV_CHECK_DEFAULT) const;
	// Returns TRUE if the specified behaviour should behave "permissive" (rather than "strict"/"secure")
	bool isPermissive(ERlvBehaviour eBhvr) const;

	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// Returns TRUE if the composite folder doesn't contain any "locked" items
	bool canTakeOffComposite(const LLInventoryCategory* pFolder) const;
	// Returns TRUE if the composite folder doesn't replace any "locked" items
	bool canWearComposite(const LLInventoryCategory* pFolder) const;
	// Returns TRUE if the folder is a composite folder and optionally returns the name
	bool getCompositeInfo(const LLInventoryCategory* pFolder, std::string* pstrName) const;
	// Returns TRUE if the inventory item belongs to a composite folder and optionally returns the name and composite folder
	bool getCompositeInfo(const LLUUID& idItem, std::string* pstrName, LLViewerInventoryCategory** ppFolder) const;
	// Returns TRUE if the folder is a composite folder
	bool isCompositeFolder(const LLInventoryCategory* pFolder) const { return getCompositeInfo(pFolder, NULL); }
	// Returns TRUE if the inventory item belongs to a composite folder
	bool isCompositeDescendent(const LLUUID& idItem) const { return getCompositeInfo(idItem, NULL, NULL); }
	// Returns TRUE if the inventory item is part of a folded composite folder and should be hidden from @getoufit or @getattach
	bool isHiddenCompositeItem(const LLUUID& idItem, const std::string& strItemType) const;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

public:
	// Adds a blocked object (= object that is blocked from issuing commands) by UUID (can be null) and/or name
	void addBlockedObject(const LLUUID& idObj, const std::string& strObjName);
	// Returns TRUE if there's an unresolved blocked object (known name but unknown UUID)
	bool hasUnresolvedBlockedObject() const;
	// Returns TRUE if the object with the specified UUID is blocked from issuing commands
	bool isBlockedObject(const LLUUID& idObj) const;
	// Removes a blocked object
	void removeBlockedObject(const LLUUID& idObj);

	// --------------------------------

	/*
	 * Helper functions
	 */
public:
	// Accessors/Mutators
	const LLUUID&     getAgentGroup() const			{ return m_idAgentGroup; }					// @setgroup
	bool              getCanCancelTp() const		{ return m_fCanCancelTp; }					// @accepttp and @tpto
	void              setCanCancelTp(bool fAllow)	{ m_fCanCancelTp = fAllow; }				// @accepttp and @tpto
	const LLVector3d& getSitSource() const						{ return m_posSitSource; }		// @standtp
	void              setSitSource(const LLVector3d& posSource)	{ m_posSitSource = posSource; }	// @standtp

	// Command specific helper functions
	bool filterChat(std::string& strUTF8Text, bool fFilterEmote) const;							// @sendchat, @recvchat and @redirchat
	bool hitTestOverlay(const LLCoordGL& ptMouse) const;                                        // @setoverlay
	bool redirectChatOrEmote(const std::string& strUTF8Test) const;								// @redirchat and @rediremote
	void renderOverlay();																		// @setoverlay

	// Command processing helper functions
	ERlvCmdRet processCommand(const LLUUID& idObj, const std::string& strCommand, bool fFromObj);
	void       processRetainedCommands(ERlvBehaviour eBhvrFilter = RLV_BHVR_UNKNOWN, ERlvParamType eTypeFilter = RLV_TYPE_UNKNOWN);
	bool       processIMQuery(const LLUUID& idSender, const std::string& strCommand);

	// Returns a pointer to the currently executing command (do *not* save this pointer)
	const RlvCommand* getCurrentCommand() const { return (!m_CurCommandStack.empty()) ? m_CurCommandStack.top() : NULL; }
	// Returns the UUID of the object we're currently executing a command for
	const LLUUID&     getCurrentObject() const	{ return (!m_CurObjectStack.empty()) ? m_CurObjectStack.top() : LLUUID::null; }

	// Initialization
	static bool canEnable();
	static bool isEnabled()	{ return m_fEnabled; }
	static bool setEnabled(bool fEnable);
protected:
	// Command specific helper functions (NOTE: these generally do not perform safety checks)
	bool checkActiveGroupThrottle(const LLUUID& idRlvObj);                                      // @setgroup=force
	void clearOverlayImage();                                                                   // @setoverlay=n
	void setActiveGroup(const LLUUID& idGroup);                                                 // @setgroup=force
	void setActiveGroupRole(const LLUUID& idGroup, const std::string& strRole);                 // @setgroup=force
	void setOverlayImage(const LLUUID& idTexture);                                              // @setoverlay=n

	void onIMQueryListResponse(const LLSD& sdNotification, const LLSD sdResponse);

	// --------------------------------

	/*
	 * Event handling
	 */
public:
	// The behaviour signal is triggered whenever a command is successfully processed and resulted in adding or removing a behaviour
	typedef boost::signals2::signal<void (ERlvBehaviour, ERlvParamType)> rlv_behaviour_signal_t;
	boost::signals2::connection setBehaviourCallback(const rlv_behaviour_signal_t::slot_type& cb )		 { return m_OnBehaviour.connect(cb); }
	boost::signals2::connection setBehaviourToggleCallback(const rlv_behaviour_signal_t::slot_type& cb ) { return m_OnBehaviourToggle.connect(cb); }
	// The command signal is triggered whenever a command is processed
	typedef boost::signals2::signal<void (const RlvCommand&, ERlvCmdRet, bool)> rlv_command_signal_t;
	boost::signals2::connection setCommandCallback(const rlv_command_signal_t::slot_type& cb )			 { return m_OnCommand.connect(cb); }

	void addCommandHandler(RlvExtCommandHandler* pHandler);
	void removeCommandHandler(RlvExtCommandHandler* pHandler);
protected:
	void clearCommandHandlers();
	bool notifyCommandHandlers(rlvExtCommandHandler f, const RlvCommand& rlvCmd, ERlvCmdRet& eRet, bool fNotifyAll) const;

	// Externally invoked event handlers
public:
	void onActiveGroupChanged();
	void onAttach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt);
	void onDetach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt);
	void onExperienceAttach(const LLSD& sdExperience, const std::string& strObjName);
	void onExperienceEvent(const LLSD& sdEvent);
	bool onGC();
	void onLoginComplete();
	void onSitOrStand(bool fSitting);
	void onTeleportFailed();
	void onTeleportFinished(const LLVector3d& posArrival);
	static void onIdleStartup(void* pParam);
protected:
	void getAttachmentResourcesCoro(const std::string& strUrl);
	void onTeleportCallback(U64 hRegion, const LLVector3& posRegion, const LLVector3& vecLookAt, const LLUUID& idRlvObj);

	/*
	 * Base class overrides
	 */
public:
	// LLParticularGroupObserver implementation
	void changed(const LLUUID& group_id, LLGroupChange gc) override;
	// LLOldEvents::LLSimpleListener implementation
	bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& sdUserdata) override;

	/*
	 * Command processing
	 */
protected:
	ERlvCmdRet processCommand(const RlvCommand& rlvCmd, bool fFromObj);
	ERlvCmdRet processClearCommand(const RlvCommand& rlvCmd);

	// Command handlers (RLV_TYPE_ADD and RLV_TYPE_CLEAR)
	ERlvCmdRet processAddRemCommand(const RlvCommand& rlvCmd);
	ERlvCmdRet onAddRemFolderLock(const RlvCommand& rlvCmd, bool& fRefCount);
	ERlvCmdRet onAddRemFolderLockException(const RlvCommand& rlvCmd, bool& fRefCount);
	// Command handlers (RLV_TYPE_FORCE)
	ERlvCmdRet processForceCommand(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onForceWear(const LLViewerInventoryCategory* pFolder, U32 nFlags) const;
	void       onForceWearCallback(const uuid_vec_t& idItems, U32 nFlags) const;
	// Command handlers (RLV_TYPE_REPLY)
	ERlvCmdRet processReplyCommand(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onFindFolder(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetAttach(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetAttachNames(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetInv(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetInvWorn(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetOutfit(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetOutfitNames(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetPath(const RlvCommand& rlvCmd, std::string& strReply) const;

	// --------------------------------

	/*
	 * Member variables
	 */
public:
	typedef std::map<LLUUID, RlvObject> rlv_object_map_t;
	typedef std::tuple<LLUUID, std::string, double> rlv_blocked_object_t;
	typedef std::list<rlv_blocked_object_t> rlv_blocked_object_list_t;
	typedef std::multimap<ERlvBehaviour, RlvException> rlv_exception_map_t;
protected:
	rlv_object_map_t      m_Objects;				// Map of objects that have active restrictions (idObj -> RlvObject)
	rlv_blocked_object_list_t m_BlockedObjects;		// List of (attached) objects that can't issue commands
	rlv_exception_map_t   m_Exceptions;				// Map of currently active restriction exceptions (ERlvBehaviour -> RlvException)
	S16                   m_Behaviours[RLV_BHVR_COUNT];

	rlv_command_list_t    m_Retained;
	RlvGCTimer*           m_pGCTimer;

	std::stack<const RlvCommand*> m_CurCommandStack;// Convenience (see @tpto)
	std::stack<LLUUID>    m_CurObjectStack;			// Convenience (see @tpto)

	rlv_behaviour_signal_t m_OnBehaviour;
	rlv_behaviour_signal_t m_OnBehaviourToggle;
	rlv_command_signal_t   m_OnCommand;
	mutable std::list<RlvExtCommandHandler*> m_CommandHandlers;
	boost::signals2::scoped_connection       m_ExperienceEventConn;
	boost::signals2::scoped_connection       m_TeleportFailedConn;
	boost::signals2::scoped_connection       m_TeleportFinishedConn;

	static bool         m_fEnabled;					// Use setEnabled() to toggle this

	bool                                    m_fCanCancelTp;					// @accepttp=n and @tpto=force
	mutable LLVector3d                      m_posSitSource;					// @standtp=n (mutable because onForceXXX handles are all declared as const)
	mutable LLUUID                          m_idAgentGroup;					// @setgroup=n
	std::pair<LLUUID, std::string>          m_PendingGroupChange;			// @setgroup=force
	std::pair<LLTimer, LLUUID>              m_GroupChangeExpiration;        // @setgroup=force
	LLPointer<LLViewerFetchedTexture>       m_pOverlayImage = nullptr;		// @setoverlay=n
	int                                     m_nOverlayOrigBoost = 0;		// @setoverlay=n

	friend class RlvSharedRootFetcher;				// Fetcher needs access to m_fFetchComplete
	friend class RlvGCTimer;						// Timer clear its own point at destruction
	template<ERlvBehaviourOptionType> friend struct RlvBehaviourGenericHandler;
	template<ERlvParamType> friend struct RlvCommandHandlerBaseImpl;
	template<ERlvParamType, ERlvBehaviour> friend struct RlvCommandHandler;
	template<ERlvBehaviourModifier> friend class RlvBehaviourModifierHandler;

	// --------------------------------

	/*
	 * Internal access functions
	 */
public:
	const rlv_object_map_t& getObjectMap() const { return m_Objects; }
};

typedef RlvHandler rlv_handler_t;
extern rlv_handler_t gRlvHandler;

// ============================================================================
// Inlined member functions
//

inline RlvHandler& RlvHandler::instance()
{
	return gRlvHandler;
}

inline RlvHandler* RlvHandler::getInstance()
{
	return &gRlvHandler;
}

inline bool RlvHandler::hasBehaviour(ERlvBehaviour eBhvr, const std::string& strOption) const
{
	return hasBehaviourExcept(eBhvr, strOption, LLUUID::null);
}

inline bool RlvHandler::hasBehaviourExcept(ERlvBehaviour eBhvr, const LLUUID& idObj) const
{
	return hasBehaviourExcept(eBhvr, LLStringUtil::null, idObj);
}

// ============================================================================

#endif // RLV_HANDLER_H
