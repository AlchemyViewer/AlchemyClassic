/** 
 * @file lldate.cpp
 * @author Phoenix
 * @date 2006-02-05
 * @brief Implementation of the date class
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "linden_common.h"
#include "lldate.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <time.h>
#include <locale.h>
#include <string>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "lltimer.h"
#include "llstring.h"
#include "llfasttimer.h"

using namespace boost::posix_time;

static const F64 DATE_EPOCH = 0.0;

LLDate::LLDate() : mSecondsSinceEpoch(DATE_EPOCH)
{}

LLDate::LLDate(const LLDate& date) :
	mSecondsSinceEpoch(date.mSecondsSinceEpoch)
{}

LLDate::LLDate(F64SecondsImplicit seconds_since_epoch) :
	mSecondsSinceEpoch(seconds_since_epoch.value())
{}

LLDate::LLDate(const std::string& iso8601_date)
{
	if(!fromString(iso8601_date))
	{
		LL_WARNS() << "date " << iso8601_date << " failed to parse; ZEROING IT OUT" << LL_ENDL;
		mSecondsSinceEpoch = DATE_EPOCH;
	}
}

std::string LLDate::asString() const
{
	std::ostringstream stream;
	toStream(stream);
	return stream.str();
}

//@ brief Converts time in seconds since EPOCH
//        to RFC 1123 compliant date format
//        E.g. 1184797044.037586 == Wednesday, 18 Jul 2007 22:17:24 GMT
//        in RFC 1123. HTTP dates are always in GMT and RFC 1123
//        is one of the standards used and the prefered format
std::string LLDate::asRFC1123() const
{
	return toHTTPDateString(std::string("%A, %d %b %Y %H:%M:%S GMT"));
}

LLTrace::BlockTimerStatHandle FT_DATE_FORMAT("Date Format");

std::string LLDate::toHTTPDateString(std::string fmt) const
{
	LL_RECORD_BLOCK_TIME(FT_DATE_FORMAT);
	
	time_t locSeconds = (time_t) mSecondsSinceEpoch;
	struct tm * gmt = gmtime (&locSeconds);
	return toHTTPDateString(gmt, fmt);
}

std::string LLDate::toHTTPDateString (tm * gmt, std::string fmt)
{
	LL_RECORD_BLOCK_TIME(FT_DATE_FORMAT);

	// avoid calling setlocale() unnecessarily - it's expensive.
	static std::string prev_locale = "";
	std::string this_locale = LLStringUtil::getLocale();
	if (this_locale != prev_locale)
	{
		setlocale(LC_TIME, this_locale.c_str());
		prev_locale = this_locale;
	}

	// use strftime() as it appears to be faster than std::time_put
	char buffer[128];
	strftime(buffer, 128, fmt.c_str(), gmt);
	std::string res(buffer);
#if LL_WINDOWS
	// Convert from locale-dependant charset to UTF-8 (EXT-8524).
	res = ll_convert_string_to_utf8_string(res);
#endif
	return res;
}

void LLDate::toStream(std::ostream& s) const
{
	std::ios::fmtflags f( s.flags() );

	ptime time = from_time_t(mSecondsSinceEpoch);
	
	s << std::dec << std::setfill('0');
	s << std::right;
	s		 << std::setw(4) << (time.date().year())
	  << '-' << std::setw(2) << (time.date().month().as_number())
	  << '-' << std::setw(2) << (time.date().day())
	  << 'T' << std::setw(2) << (time.time_of_day().hours())
	  << ':' << std::setw(2) << (time.time_of_day().minutes())
	  << ':' << std::setw(2) << (time.time_of_day().seconds());
	if (time.time_of_day().fractional_seconds() > 0)
	{
		s << '.' << std::setw(2)
		<< time.time_of_day().fractional_seconds();
	}
	s << 'Z'
	  << std::setfill(' ');

	s.flags( f );
}

bool LLDate::split(S32 *year, S32 *month, S32 *day, S32 *hour, S32 *min, S32 *sec) const
{
	ptime result = from_time_t(mSecondsSinceEpoch);

	if (year)
		*year = result.date().year();

	if (month)
		*month = result.date().month().as_number();

	if (day)
		*day = result.date().day();

	if (hour)
		*hour = result.time_of_day().hours();

	if (min)
		*min = result.time_of_day().minutes();

	if (sec)
		*sec = result.time_of_day().seconds();

	return true;
}

bool LLDate::fromString(const std::string& iso8601_date)
{
	try
	{
		std::string date(iso8601_date);
		if (date.back() == 'Z') date.pop_back(); //HACK ALERT! parse_delimited_time can't f with tz, so just strip Z. lol
		ptime pt = boost::date_time::parse_delimited_time<ptime>(date, 'T');
		time_duration time_since_epoch = (pt - ptime(boost::gregorian::date(1970,1,1)));
		mSecondsSinceEpoch = time_since_epoch.total_seconds();
	}
	catch (std::exception const&)
	{
		return false;
	}
	return true;
}

bool LLDate::fromStream(std::istream& s)
{
	std::istreambuf_iterator<char> eos;
	std::string str(std::istreambuf_iterator<char>(s), eos);
	return fromString(str);
}

bool LLDate::fromYMDHMS(S32 year, S32 month, S32 day, S32 hour, S32 min, S32 sec)
{
	std::tm exp_time = {0};
	exp_time.tm_year = year - 1900;
	exp_time.tm_mon = month - 1;
	exp_time.tm_mday = day;
	exp_time.tm_hour = hour;
	exp_time.tm_min = min;
	exp_time.tm_sec = sec;

	mSecondsSinceEpoch = std::mktime(&exp_time);
	return true;
}

F64 LLDate::secondsSinceEpoch() const
{
	return mSecondsSinceEpoch;
}

void LLDate::secondsSinceEpoch(F64 seconds)
{
	mSecondsSinceEpoch = seconds;
}

/* static */ LLDate LLDate::now()
{
	// time() returns seconds, we want fractions of a second, which LLTimer provides --RN
	return LLDate(LLTimer::getTotalSeconds());
}

bool LLDate::operator<(const LLDate& rhs) const
{
    return mSecondsSinceEpoch < rhs.mSecondsSinceEpoch;
}

std::ostream& operator<<(std::ostream& s, const LLDate& date)
{
	date.toStream(s);
	return s;
}

std::istream& operator>>(std::istream& s, LLDate& date)
{
	date.fromStream(s);
	return s;
}

