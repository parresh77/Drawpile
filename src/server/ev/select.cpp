/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior writeitten authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "select.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif
#include <cerrno> // errno
#include <cassert> // assert()

const bool event_has_error<EventSelect>::value = true;
const int event_read<EventSelect>::value = 1;
const int event_write<EventSelect>::value = 2;
const int event_error<EventSelect>::value = 4;
const std::string event_system<EventSelect>::value("select");

EventSelect::EventSelect() throw()
	#ifndef WIN32
	: nfds_r(event_invalid_fd<EventSelect>::value),
	nfds_w(event_invalid_fd<EventSelect>::value),
	nfds_e(event_invalid_fd<EventSelect>::value)
	#endif
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select()" << endl;
	#endif
	
	FD_ZERO(&fds_r);
	FD_ZERO(&fds_w);
	FD_ZERO(&fds_e);
}

EventSelect::~EventSelect() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~select()" << endl;
	#endif
}

// Errors: WSAENETDOWN
int EventSelect::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.wait()" << endl;
	#endif
	
	#ifdef EV_SELECT_COPY
	FD_COPY(&fds_r, &t_fds_r);
	FD_COPY(&fds_w, &t_fds_w);
	FD_COPY(&fds_e, &t_fds_e);
	#else
	memcpy(&t_fds_r, &fds_r, sizeof(fds_r));
	memcpy(&t_fds_w, &fds_w, sizeof(fds_w));
	memcpy(&t_fds_e, &fds_e, sizeof(fds_e));
	#endif // HAVE_SELECT_COPY
	
	#ifdef WIN32
	static const fd_t ubnfds = 0; // not used
	#else
	using std::max;
	const fd_t ubnfds = max(max(nfds_w,nfds_r), nfds_e);
	#endif
	
	nfds = select((ubnfds == event_invalid_fd<EventSelect>::value ? event_invalid_fd<EventSelect>::value : ubnfds+1), &t_fds_r, &t_fds_w, &t_fds_e, &_timeout);
	
	switch (nfds)
	{
		case -1:
			#ifdef WIN32
			error = WSAGetLastError();
			#else
			error = errno;
			#endif
			
			if (error == EINTR)
				return nfds = 0;
			
			#ifdef WIN32
			assert(error != WSANOTINITIALISED);
			#endif
			assert(error != EBADF);
			assert(error != ENOTSOCK);
			assert(error != EINVAL);
			assert(error != EFAULT);
			
			#if defined(WIN32) and !defined(NDEBUG)
			if (error == WSAENETDOWN)
				cerr << "The network subsystem has failed." << endl;
			#endif
			break;
		case 0:
			// do nothing
			break;
		default:
			fd_iter = fd_list.begin();
			break;
	}
	
	return nfds;
}

int EventSelect::add(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.add(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != event_invalid_fd<EventSelect>::value);
	
	bool rc=false;
	
	if (fIsSet(events, event_read<EventSelect>::value))
	{
		FD_SET(fd, &fds_r);
		#if !defined(WIN32) // win32 ignores the argument
		read_set.insert(read_set.end(), fd);
		if (fd > nfds_r) nfds_r = fd;
		#endif // !Win32
		rc = true;
	}
	if (fIsSet(events, event_write<EventSelect>::value))
	{
		FD_SET(fd, &fds_w);
		#if !defined(WIN32) // win32 ignores the argument
		write_set.insert(write_set.end(), fd);
		if (fd > nfds_w) nfds_w = fd;
		#endif // !Win32
		rc = true;
	}
	if (fIsSet(events, event_error<EventSelect>::value))
	{
		FD_SET(fd, &fds_e);
		#if !defined(WIN32) // win32 ignores the argument
		error_set.insert(error_set.end(), fd);
		if (fd > nfds_e) nfds_e = fd;
		#endif // !Win32
		rc = true;
	}
	
	assert(rc);
	
	// maintain fd_list
	fd_list[fd] = events;
	
	return rc;
}

int EventSelect::modify(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.modify(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != event_invalid_fd<EventSelect>::value);
	
	// act like a wrapper.
	if (events != 0)
		add(fd, events);
	
	if (!fIsSet(events, event_read<EventSelect>::value))
	{
		FD_CLR(fd, &fds_r);
		#ifndef WIN32
		read_set.erase(fd);
		if (fd == nfds_r)
			nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : event_invalid_fd<EventSelect>::value);
		#endif // WIN32
	}
	
	if (!fIsSet(events, event_write<EventSelect>::value))
	{
		FD_CLR(fd, &fds_w);
		#ifndef WIN32
		write_set.erase(fd);
		if (fd == nfds_w)
			nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : event_invalid_fd<EventSelect>::value);
		#endif // WIN32
	}
	
	if (!fIsSet(events, event_error<EventSelect>::value))
	{
		FD_CLR(fd, &fds_e);
		#ifndef WIN32
		error_set.erase(fd);
		if (fd == nfds_e)
			nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : event_invalid_fd<EventSelect>::value);
		#endif // WIN32
	}
	
	return 0;
}

int EventSelect::remove(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.remove(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != event_invalid_fd<EventSelect>::value);
	
	std::map<fd_t,uint>::iterator iter(fd_list.find(fd));
	assert(iter != fd_list.end());
	
	FD_CLR(fd, &fds_r);
	#ifndef WIN32
	read_set.erase(fd);
	if (fd == nfds_r)
		nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : event_invalid_fd<EventSelect>::value);
	#endif // WIN32
	
	FD_CLR(fd, &fds_w);
	#ifndef WIN32
	write_set.erase(fd);
	if (fd == nfds_w)
		nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : event_invalid_fd<EventSelect>::value);
	#endif // WIN32
	
	FD_CLR(fd, &fds_e);
	#ifndef WIN32
	error_set.erase(fd);
	if (fd == nfds_e)
		nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : event_invalid_fd<EventSelect>::value);
	#endif // WIN32
	
	fd_list.erase(iter);
	fd_iter = fd_list.begin();
	
	return true;
}

bool EventSelect::getEvent(fd_t &fd, ev_t &events) throw()
{
	assert(fd_list.size() > 0);
	
	while (fd_iter != fd_list.end())
	{
		fd = (fd_iter++)->first;
		
		events = 0;
		
		if (FD_ISSET(fd, &t_fds_r) != 0)
			fSet(events, event_read<EventSelect>::value);
		if (FD_ISSET(fd, &t_fds_w) != 0)
			fSet(events, event_write<EventSelect>::value);
		if (FD_ISSET(fd, &t_fds_e) != 0)
			fSet(events, event_error<EventSelect>::value);
		
		if (events != 0)
			return true;
	}
	
	return false;
}

void EventSelect::timeout(uint msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "select.timeout(msecs: " << msecs << ")" << std::endl;
	#endif
	
	if (msecs > 1000)
	{
		_timeout.tv_sec = msecs/1000;
		msecs -= _timeout.tv_sec*1000;
	}
	else
		_timeout.tv_sec = 0;
	
	_timeout.tv_usec = msecs * 1000;
}
