// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llstatgraph.cpp
 * @brief Simpler compact stat graph with tooltip
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

//#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "llstatgraph.h"
#include "llrender.h"

#include "llmath.h"
#include "llui.h"
#include "llgl.h"
#include "llglheaders.h"
#include "lltracerecording.h"
#include "lltracethreadrecorder.h"
//#include "llviewercontrol.h"

// rate at which to update display of value that is rapidly changing
const F32 MEAN_VALUE_UPDATE_TIME = 1.f / 4.f;
// time between value changes that qualifies as a "rapid change"
const F32Seconds	RAPID_CHANGE_THRESHOLD(0.2f);
// maximum number of rapid changes in RAPID_CHANGE_WINDOW before switching over to displaying the mean 
// instead of latest value
const S32 MAX_RAPID_CHANGES_PER_SEC = 10;
// period of time over which to measure rapid changes
const F32Seconds RAPID_CHANGE_WINDOW(1.f);

///////////////////////////////////////////////////////////////////////////////////

LLStatGraph::LLStatGraph(const Params& p)
:	LLView(p),
	mStatType(STAT_NONE),
	mValue(p.value),
	mMin(p.min),
	mMax(p.max),
	mLabel(p.label),
	mUnitLabel(p.unit_label),
	mDecimalDigits(p.decimal_digits)
{
	setStat(p.stat);
	setToolTip(p.label());

	for(LLInitParam::ParamIterator<ThresholdParams>::const_iterator it = p.thresholds.threshold.begin(), end_it = p.thresholds.threshold.end();
		it != end_it;
		++it)
	{
		mThresholds.push_back(Threshold(it->value(), it->color));
	}
}

template<typename T>
S32 calc_num_rapid_changes_graph(LLTrace::PeriodicRecording& periodic_recording, const T& stat, const F32Seconds time_period)
{
	F32Seconds			elapsed_time,
		time_since_value_changed;
	S32					num_rapid_changes = 0;
	const F32Seconds	RAPID_CHANGE_THRESHOLD = F32Seconds(0.3f);
	F64					last_value = periodic_recording.getPrevRecording(1).getLastValue(stat);

	for (S32 i = 2; i < periodic_recording.getNumRecordedPeriods(); i++)
	{
		LLTrace::Recording& recording = periodic_recording.getPrevRecording(i);
		F64 cur_value = recording.getLastValue(stat);

		if (last_value != cur_value)
		{
			if (time_since_value_changed < RAPID_CHANGE_THRESHOLD) num_rapid_changes++;
			time_since_value_changed = (F32Seconds) 0;
		}
		last_value = cur_value;

		elapsed_time += recording.getDuration();
		if (elapsed_time > time_period) break;
	}

	return num_rapid_changes;
}

void LLStatGraph::draw()
{
	F32 range, frac;
	range = mMax - mMin;

	std::string unit_label;
	F32			current = 0,
		min = 0,
		max = 0,
		mean = 0,
		display_value = 0;
	S32			num_rapid_changes = 0;
	S32			decimal_digits = mDecimalDigits;

	const S32 num_frames = 20;
	LLTrace::PeriodicRecording& frame_recording = LLTrace::get_frame_recording();
	LLTrace::Recording& last_frame_recording = frame_recording.getLastRecording();

	switch (mStatType)
	{
	case STAT_COUNT:
	{
		const LLTrace::StatType<LLTrace::CountAccumulator>& count_stat = *mStat.countStatp;
		static const std::string seconds("/s");
		unit_label = mUnitLabel.empty() ? count_stat.getUnitLabel() + seconds : mUnitLabel;
		current = last_frame_recording.getPerSec(count_stat);
		min = frame_recording.getPeriodMinPerSec(count_stat, num_frames);
		max = frame_recording.getPeriodMaxPerSec(count_stat, num_frames);
		mean = frame_recording.getPeriodMeanPerSec(count_stat, num_frames);
		display_value = mean;
	}
	break;
	case STAT_EVENT:
	{
		const LLTrace::StatType<LLTrace::EventAccumulator>& event_stat = *mStat.eventStatp;

		unit_label = mUnitLabel.empty() ? event_stat.getUnitLabel() : mUnitLabel;
		current = last_frame_recording.getLastValue(event_stat);
		min = frame_recording.getPeriodMin(event_stat, num_frames);
		max = frame_recording.getPeriodMax(event_stat, num_frames);
		mean = frame_recording.getPeriodMean(event_stat, num_frames);
		display_value = mean;
	}
	break;
	case STAT_SAMPLE:
	{
		const LLTrace::StatType<LLTrace::SampleAccumulator>& sample_stat = *mStat.sampleStatp;

		unit_label = mUnitLabel.empty() ? sample_stat.getUnitLabel() : mUnitLabel;
		current = last_frame_recording.getLastValue(sample_stat);
		min = frame_recording.getPeriodMin(sample_stat, num_frames);
		max = frame_recording.getPeriodMax(sample_stat, num_frames);
		mean = frame_recording.getPeriodMean(sample_stat, num_frames);
		num_rapid_changes = calc_num_rapid_changes_graph(frame_recording, sample_stat, RAPID_CHANGE_WINDOW);

		if (num_rapid_changes / RAPID_CHANGE_WINDOW.value() > MAX_RAPID_CHANGES_PER_SEC)
		{
			display_value = mean;
		}
		else
		{
			display_value = current;
			if (is_approx_equal((F32) (S32) display_value, display_value))
			{
				decimal_digits = 0;
			}
		}
	}
	break;
	case STAT_MEM:
	{
		const LLTrace::StatType<LLTrace::MemAccumulator>& mem_stat = *mStat.memStatp;

		unit_label = mUnitLabel.empty() ? mem_stat.getUnitLabel() : mUnitLabel;
		current = last_frame_recording.getLastValue(mem_stat).value();
		min = frame_recording.getPeriodMin(mem_stat, num_frames).value();
		max = frame_recording.getPeriodMax(mem_stat, num_frames).value();
		mean = frame_recording.getPeriodMean(mem_stat, num_frames).value();
		display_value = current;
	}
	break;
	default:
		break;
	}

	mValue = display_value;

	frac = (mValue - mMin) / range;
	frac = llmax(0.f, frac);
	frac = llmin(1.f, frac);

	if (mUpdateTimer.getElapsedTimeF32() > 0.5f)
	{
		std::string tmp_str = llformat("%s %10.*f %s", mLabel.getString().c_str(), decimal_digits, mValue, unit_label.c_str());
		setToolTip(tmp_str);

		mUpdateTimer.reset();
	}

	LLColor4 color;

	threshold_vec_t::iterator it = std::lower_bound(mThresholds.begin(), mThresholds.end(), Threshold(mValue / mMax, LLUIColor()));

	if (it != mThresholds.begin())
	{
		it--;
	}

	color = LLUIColorTable::instance().getColor( "MenuDefaultBgColor" );
	gGL.color4fv(color.mV);
	gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, TRUE);

	gGL.color4fv(LLColor4::black.mV);
	gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, FALSE);
	
	color = it->mColor;
	gGL.color4fv(color.mV);
	gl_rect_2d(1, ll_round(frac*getRect().getHeight()), getRect().getWidth() - 1, 0, TRUE);
}

void LLStatGraph::setMin(const F32 min)
{
	mMin = min;
}

void LLStatGraph::setMax(const F32 max)
{
	mMax = max;
}

void LLStatGraph::setStat(const std::string& stat_name)
{
	using namespace LLTrace;
	const StatType<CountAccumulator>*	count_stat;
	const StatType<EventAccumulator>*	event_stat;
	const StatType<SampleAccumulator>*	sample_stat;
	const StatType<MemAccumulator>*		mem_stat;

	if ((count_stat = StatType<CountAccumulator>::getInstance(stat_name)))
	{
		mStat.countStatp = count_stat;
		mStatType = STAT_COUNT;
	}
	else if ((event_stat = StatType<EventAccumulator>::getInstance(stat_name)))
	{
		mStat.eventStatp = event_stat;
		mStatType = STAT_EVENT;
	}
	else if ((sample_stat = StatType<SampleAccumulator>::getInstance(stat_name)))
	{
		mStat.sampleStatp = sample_stat;
		mStatType = STAT_SAMPLE;
	}
	else if ((mem_stat = StatType<MemAccumulator>::getInstance(stat_name)))
	{
		mStat.memStatp = mem_stat;
		mStatType = STAT_MEM;
	}
}

