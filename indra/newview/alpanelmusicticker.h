#ifndef AL_PANELMUSICTICKER_H
#define AL_PANELMUSICTICKER_H

#include "llpanel.h"

class LLIconCtrl;
class LLTextBox;

class ALPanelMusicTicker : public LLPanel
{
public:
	ALPanelMusicTicker();	//ctor

	virtual ~ALPanelMusicTicker() {}
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void draw();
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
private:
	void updateTickerText(); //called via draw.
	void drawOscilloscope(); //called via draw.
	bool setPaused(bool pause); //returns true on state change.
	void resetTicker(); //Resets tickers to their innitial values (no offset).
	bool setArtist(const std::string &artist);	//returns true on change
	bool setTitle(const std::string &title);	//returns true on change
	S32 countExtraChars(LLTextBox *texbox, const std::string &text);	//calculates how many characters are truncated by bounds.
	void iterateTickerOffset();	//Logic that actually shuffles the text to the left.

	enum ePlayState
	{
		STATE_PAUSED,
		STATE_PLAYING
	};

	ePlayState mPlayState;
	std::string mszLoading;
	std::string mszPaused;
	std::string mszArtist;
	std::string mszTitle;
	LLTimer mScrollTimer;
	LLTimer mLoadTimer;
	S32 mArtistScrollChars;
	S32 mTitleScrollChars;
	S32 mCurScrollChar;

	LLColor4 mOscillatorColor;

	//UI elements
	LLIconCtrl* mTickerBackground;
	LLTextBox* mArtistText;
	LLTextBox* mTitleText;
	LLUICtrl* mVisualizer;
};

#endif // AL_PANELMUSICTICKER_H
