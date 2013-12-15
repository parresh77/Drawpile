/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef DP_NET_MESSAGE_H
#define DP_NET_MESSAGE_H

#include <Qt>

namespace protocol {

/**
 * DrawPile network protocol message types
 */
enum MessageType {
	// Login stream
	MSG_LOGIN,

	// Meta stream
	MSG_USER_JOIN,
	MSG_USER_ATTR,
	MSG_USER_LEAVE,
	MSG_CHAT,
	MSG_LAYER_ACL,
	MSG_SNAPSHOT,
	MSG_SESSION_TITLE,
	MSG_SESSION_CONFIG,
	MSG_STREAMPOS,

	// Command stream
	MSG_CANVAS_RESIZE=128,
	MSG_LAYER_CREATE,
	MSG_LAYER_ATTR,
	MSG_LAYER_RETITLE,
	MSG_LAYER_ORDER,
	MSG_LAYER_DELETE,
	MSG_PUTIMAGE,
	MSG_TOOLCHANGE,
	MSG_PEN_MOVE,
	MSG_PEN_UP,
	MSG_ANNOTATION_CREATE,
	MSG_ANNOTATION_RESHAPE,
	MSG_ANNOTATION_EDIT,
	MSG_ANNOTATION_DELETE,
	MSG_UNDOPOINT,
	MSG_UNDO
};

class Message {
	friend class MessagePtr;
public:
	Message(MessageType type, uint8_t ctx): _type(type), _contextid(ctx), _undone(false), _refcount(0) {}
	virtual ~Message() = default;
	
	/**
	 * @brief Get the type of this message.
	 * @return message type
	 */
	MessageType type() const { return _type; }
	
	/**
	 * @brief Check if this message type is a command stream type
	 * 
	 * Command stream messages are the messages directly related to drawing.
	 * The canvas can be reconstructed exactly using only command messages.
	 * @return true if this is a drawing command
	 */
	bool isCommand() const { return _type >= MSG_CANVAS_RESIZE; }

	/**
	 * @brief Get the message length, header included
	 * @return message length in bytes
	 */
	int length() const { return 3 + payloadLength(); }

	/**
	 * @brief Get the user context ID of this message
	 *
	 * The ID is 0 for messages that are not related to any user
	 * @return context ID or 0 if not applicable
	 */
	uint8_t contextId() const { return _contextid; }

	/**
	 * @brief Set the user ID of this message
	 *
	 * @param userid the new user id
	 */
	void setContextId(uint8_t userid) { _contextid = userid; }

	/**
	 * @brief Does this command need operator privileges to issue?
	 * @return true if user must be session operator to send this
	 */
	virtual bool isOpCommand() const { return false; }

	/**
	 * @brief Has this command been marked as undone?
	 *
	 * Note. This is a purely local flag that is not part of the
	 * protocol. It is here to avoid the need to maintain an
	 * external undone action list.
	 *
	 * @return true if this message has been marked as undone
	 */
	bool isUndone() const { return _undone; }

	/**
	 * @brief Mark this message as undone
	 *
	 * Note. Not all messages are undoable. This function
	 * does nothing if this message type doesn't support undoing.
	 *
	 * @param undone new undo flag state
	 */
	void setUndone(bool undone) { if(isUndoable()) _undone = undone; }

	/**
	 * @brief Serialize this message
	 *
	 * The data buffer must be long enough to hold length() bytes.
	 * @param data buffer where to write the message
	 * @return number of bytes written (should always be length())
	 */
	int serialize(char *data) const;

	/**
	 * @brief get the length of the message from the given data
	 *
	 * Data buffer should be at least two bytes long
	 * @param data data buffer
	 * @return length
	 */
	static int sniffLength(const char *data);

	/**
	 * @brief deserialize a message from data buffer
	 *
	 * The provided buffer should contain at least sniffLength(data)
	 * bytes.
	 *
	 * If the message type is unrecognized or the message content is
	 * determined to be invalid, a null pointer is returned.
	 *
	 * @param data input data buffer
	 * @return message or 0 if type is unknown
	 */
	static Message *deserialize(const uchar *data);

protected:
	/**
	 * @brief Get the length of the message payload
	 * @return payload length in bytes
	 */
	virtual int payloadLength() const = 0;

	/**
	 * @brief Serialize the message payload
	 * @param data data buffer
	 * @return number of bytes written (should always be the same as payloadLenth())
	 */
	virtual int serializePayload(uchar *data) const = 0;

	/**
	 * @brief Is this message type undoable?
	 * @return true if this action can be undone
	 */
	virtual bool isUndoable() const { return false; }

private:
	const MessageType _type;
	uint8_t _contextid; // this is part of the payload for those message types that have it

	bool _undone;
	int _refcount;
};

/**
* @brief A reference counting pointer for Messages
*
* This object is the length of a normal pointer so it can be used
* efficiently with QList.
*
* @todo use QAtomicInt if thread safety is needed
*/
class MessagePtr {
public:
	/**
	 * @brief Take ownership of the given raw Message pointer.
	 *
	 * The message will be deleted when reference count falls to zero.
	 * Null pointers are not allowed.
	 * @param msg
	 */
	explicit MessagePtr(Message *msg)
		: _ptr(msg)
	{
		Q_ASSERT(_ptr);
		Q_ASSERT(_ptr->_refcount==0);
		++_ptr->_refcount;
	}

	MessagePtr(const MessagePtr &ptr) : _ptr(ptr._ptr) { ++_ptr->_refcount; }

	~MessagePtr()
	{
		Q_ASSERT(_ptr->_refcount>0);
		if(--_ptr->_refcount == 0)
			delete _ptr;
	}

	MessagePtr &operator=(const MessagePtr &msg)
	{
		if(msg._ptr != _ptr) {
			Q_ASSERT(_ptr->_refcount>0);
			if(--_ptr->_refcount == 0)
				delete _ptr;
			_ptr = msg._ptr;
			++_ptr->_refcount;
		}
		return *this;
	}

	Message &operator*() const { return *_ptr; }
	Message *operator ->() const { return _ptr; }

	template<class msgtype> msgtype &cast() const { return static_cast<msgtype&>(*_ptr); }

private:
	Message *_ptr;
};

}

#endif
