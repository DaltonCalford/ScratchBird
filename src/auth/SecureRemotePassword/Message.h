#ifdef INCLUDE_ScratchBird_H		// Internal build
#define INTERNAL_FIREBIRD
#endif

#include "firebird/Interface.h"

#ifdef INTERNAL_FIREBIRD

#include "../common/classes/alloc.h"
#include "../common/StatusHolder.h"
#include "../common/classes/ImplementHelper.h"
#include "firebird/impl/sqlda_pub.h"

#else // INTERNAL_FIREBIRD

#include <assert.h>
#define fb_assert(x) assert(x)
#include <string.h>

#endif // INTERNAL_FIREBIRD

#ifdef INTERNAL_FIREBIRD
// This class helps to work with metadata iface
class Meta : public ScratchBird::RefPtr<ScratchBird::IMessageMetadata>
{
public:
	Meta(ScratchBird::IStatement* stmt, bool out)
	{
		ScratchBird::LocalStatus ls;
		ScratchBird::CheckStatusWrapper st(&ls);
		ScratchBird::IMessageMetadata* m = out ? stmt->getOutputMetadata(&st) : stmt->getInputMetadata(&st);
		if (st.getState() & ScratchBird::IStatus::STATE_ERRORS)
		{
			ScratchBird::status_exception::raise(&st);
		}
		assignRefNoIncr(m);
	}
};
#endif // INTERNAL_FIREBIRD


// Linked list of various fields
class FieldLink
{
public:
	virtual ~FieldLink() { }
	virtual void linkWithMessage(const unsigned char* buf) = 0;

	FieldLink* next;
};


// This class helps to exchange values with a message
class Message
// : public AutoStorage
{
public:
	Message(ScratchBird::IMessageMetadata* aMeta = NULL)
#ifdef INTERNAL_FIREBIRD
		: s(&st),
#else
		: s(fb_get_master_interface()->getStatus()),
#endif
		  metadata(NULL),
		  buffer(NULL),
		  builder(NULL),
		  fieldCount(0),
		  fieldList(NULL),
		  statusWrapper(s)
	{
		try
		{
			if (aMeta)
			{
				createBuffer(aMeta);
				metadata = aMeta;
				metadata->addRef();
			}
			else
			{
				ScratchBird::IMetadataBuilder* bld =
#ifdef INTERNAL_FIREBIRD
					ScratchBird::MasterInterfacePtr()->
#else
					fb_get_master_interface()->
#endif
						getMetadataBuilder(&statusWrapper, 0);
				check(&statusWrapper);
				builder = bld;
			}
		}
		catch (...)
		{
			s->dispose();
			throw;
		}
	}

	~Message()
	{
		delete[] buffer;
#ifndef INTERNAL_FIREBIRD
		s->dispose();
#endif
		if (builder)
			builder->release();
		if (metadata)
			metadata->release();
	}

public:
	template <typename T>
	static bool checkType(unsigned t, unsigned /*sz*/)
	{
		return T::unknownDataType();
	}

	template <typename T>
	static unsigned getType(unsigned& sz)
	{
		return T::SQL_UnknownDataType;
	}

	template <typename T>
	unsigned add(unsigned& t, unsigned& sz, FieldLink* lnk)
	{
		if (metadata)
		{
			unsigned l = metadata->getCount(&statusWrapper);
			check(&statusWrapper);
			if (fieldCount >= l)
			{
#ifdef INTERNAL_FIREBIRD
				(ScratchBird::Arg::Gds(isc_random) <<
					"Attempt to add to the message more variables than possible").raise();
#else
				fatalErrorHandler("Attempt to add to the message more variables than possible");
#endif
			}

			t = metadata->getType(&statusWrapper, fieldCount);
			check(&statusWrapper);
			sz = metadata->getLength(&statusWrapper, fieldCount);
			check(&statusWrapper);
			if (!checkType<T>(t, sz))
			{
#ifdef INTERNAL_FIREBIRD
				(ScratchBird::Arg::Gds(isc_random) << "Incompatible data type").raise();
#else
				fatalErrorHandler("Incompatible data type");
#endif
			}
		}
		else
		{
			fb_assert(builder);

			unsigned f = builder->addField(&statusWrapper);
			check(&statusWrapper);

			fb_assert(f == fieldCount);

			t = getType<T>(sz);
			builder->setType(&statusWrapper, f, t);
			check(&statusWrapper);
			builder->setLength(&statusWrapper, f, sz);
			check(&statusWrapper);

			lnk->next = fieldList;
			fieldList = lnk;
		}

		return fieldCount++;
	}

	static void check(ScratchBird::IStatus* status)
	{
		if (status->getState() & ScratchBird::IStatus::STATE_ERRORS)
		{
#ifdef INTERNAL_FIREBIRD
			ScratchBird::status_exception::raise(status);
#else
			char msg[100];
			const ISC_STATUS* st = status->getErrors();
			fb_interpret(msg, sizeof(msg), &st);
			fatalErrorHandler(msg);
#endif
		}
	}

	// Attention!
	// No addRef/release interface here!
	// Lifetime is equal at least to Message lifetime
	ScratchBird::IMessageMetadata* getMetadata()
	{
		if (!metadata)
		{
			fb_assert(builder);
			ScratchBird::IMessageMetadata* aMeta = builder->getMetadata(&statusWrapper);
			check(&statusWrapper);
			metadata = aMeta;
			builder->release();
			builder = NULL;
		}

		return metadata;
	}

	bool hasMetadata() const
	{
		return metadata ? true : false;
	}

	// access to message's data buffer
	unsigned char* getBuffer()
	{
		if (!buffer)
		{
			getMetadata();

			createBuffer(metadata);
			while(fieldList)
			{
				fieldList->linkWithMessage(buffer);
				fieldList = fieldList->next;
			}
		}

		return buffer;
	}

private:
	void createBuffer(ScratchBird::IMessageMetadata* aMeta)
	{
		unsigned l = aMeta->getMessageLength(&statusWrapper);
		check(&statusWrapper);
		buffer = FB_NEW unsigned char[l];
	}

public:
	ScratchBird::IStatus* s;

private:
	ScratchBird::IMessageMetadata* metadata;
	unsigned char* buffer;
	ScratchBird::IMetadataBuilder* builder;
	unsigned fieldCount;
	FieldLink* fieldList;
#ifdef INTERNAL_FIREBIRD
	ScratchBird::LocalStatus st;
#endif

public:
	ScratchBird::CheckStatusWrapper statusWrapper;
};


template <typename T>
class Field : public FieldLink
{
public:
	class Null
	{
	public:
		explicit Null(Message* m)
			: msg(m), ptr(NULL)
		{ }

		void linkMessage(short* p)
		{
			ptr = p;
			*ptr = -1;	// mark as null initially
		}

		operator FB_BOOLEAN() const
		{
			msg->getBuffer();
			return (*ptr) ? FB_TRUE : FB_FALSE;
		}

		FB_BOOLEAN operator=(FB_BOOLEAN val)
		{
			msg->getBuffer();
			*ptr = val ? -1 : 0;
			return val;
		}

	private:
		Message* msg;
		short* ptr;
	};

	explicit Field(Message& m, unsigned sz = 0)
		: ptr(NULL), charBuffer(NULL), msg(&m), null(msg), ind(~0), type(0), size(sz)
	{
		ind = msg->add<T>(type, size, this);

		if (msg->hasMetadata())
			setPointers(msg->getBuffer());
	}

	~Field()
	{
		delete[] charBuffer;
	}

	operator T()
	{
		msg->getBuffer();
		return *ptr;
	}

	T* operator&()
	{
		msg->getBuffer();
		return ptr;
	}

	T* operator->()
	{
		msg->getBuffer();
		return ptr;
	}

	T operator= (T newVal)
	{
		msg->getBuffer();
		*ptr = newVal;
		null = FB_FALSE;
		return newVal;
	}

	operator const char*()
	{
		msg->getBuffer();

		if (!charBuffer)
		{
			charBuffer = FB_NEW char[size + 1];
		}
		getStrValue(charBuffer);
		return charBuffer;
	}

	const char* operator= (const char* newVal)
	{
		msg->getBuffer();
		setStrValue(newVal, static_cast<unsigned>(strnlen(newVal, size)));
		null = FB_FALSE;
		return newVal;
	}

	void set(unsigned length, const void* newVal)
	{
		msg->getBuffer();
		setStrValue(newVal, length);
		null = FB_FALSE;
	}

private:
	void linkWithMessage(const unsigned char* buf)
	{
		setPointers(buf);
	}

	void setPointers(const unsigned char* buf)
	{
		unsigned tmp = msg->getMetadata()->getOffset(&msg->statusWrapper, ind);
		Message::check(&msg->statusWrapper);
		ptr = (T*) (buf + tmp);

		tmp = msg->getMetadata()->getNullOffset(&msg->statusWrapper, ind);
		Message::check(&msg->statusWrapper);
		null.linkMessage((short*) (buf + tmp));
	}

	void getStrValue(char* to)
	{
		T::incompatibleDataType();
	}

	void setStrValue(const void* from, unsigned len)
	{
		T::incompatibleDataType();
	}

	T* ptr;
	char* charBuffer;
	Message* msg;

public:
	Null null;

private:
	unsigned ind, type, size;
};


// ---------------------------------------------
struct Varying
{
	short len;
	char data[1];
};

template <>
inline bool Message::checkType<Varying>(unsigned t, unsigned /*sz*/)
{
	return t == SQL_VARYING;
}

template <>
inline unsigned Message::getType<Varying>(unsigned& sz)
{
	if (!sz)
		sz = 1;
	sz += sizeof(short);
	return SQL_VARYING;
}

template<>
inline void Field<Varying>::getStrValue(char* to)
{
	unsigned len = ptr->len;
	if (len > size)
		len = size;
	memcpy(to, ptr->data, len);
	to[len] = 0;
}

template<>
inline void Field<Varying>::setStrValue(const void* from, unsigned len)
{
	if (len > size)
		len = size;
	memcpy(ptr->data, from, len);
	ptr->len = len;
}

struct Text
{
	char data[1];
};

template <>
inline bool Message::checkType<Text>(unsigned t, unsigned /*sz*/)
{
	return t == SQL_TEXT;
}

template <>
inline unsigned Message::getType<Text>(unsigned& sz)
{
	if (!sz)
		sz = 1;
	return SQL_TEXT;
}

template<>
inline void Field<Text>::getStrValue(char* to)
{
	memcpy(to, ptr->data, size);
	to[size] = 0;
	unsigned len = size;
	while (len--)
	{
		if (to[len] == ' ')
			to[len] = 0;
		else
			break;
	}
}

template<>
inline void Field<Text>::setStrValue(const void* from, unsigned len)
{
	if (len > size)
		len = size;
	memcpy(ptr->data, from, len);
	if (len < size)
		memset(&ptr->data[len], ' ', size - len);
}

template <>
inline bool Message::checkType<ISC_SHORT>(unsigned t, unsigned sz)
{
	return t == SQL_SHORT && sz == sizeof(ISC_SHORT);
}

template <>
inline bool Message::checkType<ISC_LONG>(unsigned t, unsigned sz)
{
	return t == SQL_LONG && sz == sizeof(ISC_LONG);
}

template <>
inline bool Message::checkType<ISC_QUAD>(unsigned t, unsigned sz)
{
	return (t == SQL_BLOB || t == SQL_QUAD) && sz == sizeof(ISC_QUAD);
}

template <>
inline bool Message::checkType<ISC_INT64>(unsigned t, unsigned sz)
{
	return t == SQL_INT64 && sz == sizeof(ISC_INT64);
}

template <>
inline bool Message::checkType<FB_BOOLEAN>(unsigned t, unsigned sz)
{
	return t == SQL_BOOLEAN && sz == sizeof(FB_BOOLEAN);
}

template <>
inline unsigned Message::getType<ISC_SHORT>(unsigned& sz)
{
	sz = sizeof(ISC_SHORT);
	return SQL_SHORT;
}

template <>
inline unsigned Message::getType<ISC_LONG>(unsigned& sz)
{
	sz = sizeof(ISC_LONG);
	return SQL_LONG;
}

template <>
inline unsigned Message::getType<ISC_QUAD>(unsigned& sz)
{
	sz = sizeof(ISC_QUAD);
	return SQL_BLOB;
}

template <>
inline unsigned Message::getType<ISC_INT64>(unsigned& sz)
{
	sz = sizeof(ISC_INT64);
	return SQL_INT64;
}

template <>
inline unsigned Message::getType<FB_BOOLEAN>(unsigned& sz)
{
	sz = sizeof(FB_BOOLEAN);
	return SQL_BOOLEAN;
}
