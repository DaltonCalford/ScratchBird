#include "../common/BigInteger.h"
#include "../common/classes/alloc.h"
#include "../common/classes/fb_string.h"
#include "../common/sha.h"
#include "../common/sha2/sha2.h"

#ifndef AUTH_SRP_SRP_H
#define AUTH_SRP_SRP_H

#define SRP_DEBUG 0		// >0 - prints some debug info
						// >1 - uses consts instead randoms, NEVER use in PRODUCTION!

// for HANDSHAKE_DEBUG
#include "../remote/remot_proto.h"

#include <functional>

namespace Auth {

/*
 * Order of battle for SRP handshake:
 *
 * 													0.  At account creation, the server generates
 * 														a random salt and computes a password
 * 														verifier from the account name, password,
 * 														and salt.
*
 * 		1. Client generates random number
 * 		   as private key, computes public
 * 		   key.
 *
 * 		2. Client sends server the account
 * 		   name and its public key.
 * 													3.  Server receives account name, looks up
 * 														salt and password verifier.  Server
 * 														generates random number as private key.
 * 														Server computes public key from private
 * 														key, account name, verifier, and salt.
 *
 * 													4.  Server sends client public key and salt
 *
 * 		3. Client receives server public
 * 		   key and computes session key
 * 		   from server key, salt, account
 * 		   name, and password.
  * 												5.  Server computes session key from client
 * 														public key, client name, and verifier
 *
 * 		For full details, see http://www.ietf.org/rfc/rfc5054.txt
 *
 */

class RemoteGroup;

template <class SHA> class SecureHash : public SHA
{
public:
	void getInt(ScratchBird::BigInteger& hash)
	{
		ScratchBird::UCharBuffer tmp;
		SHA::getHash(tmp);
		hash.assign(tmp.getCount(), tmp.begin());
	}

	void processInt(const ScratchBird::BigInteger& data)
	{
		ScratchBird::UCharBuffer bytes;
		data.getBytes(bytes);
		SHA::process(bytes);
	}

	void processStrippedInt(const ScratchBird::BigInteger& data)
	{
		ScratchBird::UCharBuffer bytes;
		data.getBytes(bytes);
		if (bytes.getCount())
		{
			unsigned int n = (bytes[0] == 0) ? 1u : 0;
			SHA::process(bytes.getCount() - n, bytes.begin() + n);
		}
	}
};


class RemotePassword : public ScratchBird::GlobalStorage
{
private:
	const RemoteGroup*		group;
	Auth::SecureHash<ScratchBird::Sha1>	hash;
	ScratchBird::BigInteger	privateKey;
	ScratchBird::BigInteger	scramble;

protected:
    virtual ScratchBird::BigInteger makeProof(const ScratchBird::BigInteger n1, const ScratchBird::BigInteger n2,
                const char* salt, const ScratchBird::UCharBuffer& sessionKey) = 0;

public:
	ScratchBird::BigInteger	clientPublicKey;
	ScratchBird::BigInteger	serverPublicKey;

public:
	RemotePassword();
	virtual ~RemotePassword();

	static const char* plugName;
	static const unsigned SRP_KEY_SIZE = 128;
	static const unsigned SRP_VERIFIER_SIZE = SRP_KEY_SIZE;
	static const unsigned SRP_SALT_SIZE = 32;

	static ScratchBird::string pluginName(unsigned bits);

	ScratchBird::BigInteger getUserHash(const char* account,
									 const char* salt,
									 const char* password);
	ScratchBird::BigInteger computeVerifier(const ScratchBird::string& account,
										 const ScratchBird::string& salt,
										 const ScratchBird::string& password);
	void genClientKey(ScratchBird::string& clientPubKey);
	void genServerKey(ScratchBird::string& serverPubKey, const ScratchBird::UCharBuffer& verifier);
	void computeScramble();
	void clientSessionKey(ScratchBird::UCharBuffer& sessionKey, const char* account,
						  const char* salt, const char* password,
						  const char* serverPubKey);
	void serverSessionKey(ScratchBird::UCharBuffer& sessionKey,
						  const char* clientPubKey,
						  const ScratchBird::UCharBuffer& verifier);
	ScratchBird::BigInteger clientProof(const char* account,
									 const char* salt,
									 const ScratchBird::UCharBuffer& sessionKey);
};

template <class SHA> class RemotePasswordImpl : public RemotePassword
{
protected:
	ScratchBird::BigInteger makeProof(const ScratchBird::BigInteger n1, const ScratchBird::BigInteger n2,
                const char* salt, const ScratchBird::UCharBuffer& sessionKey)
    {
		Auth::SecureHash<SHA> digest;
		digest.processInt(n1);				// H(prime) ^ H(g)
		digest.processInt(n2);				// H(I)
		digest.process(salt);				// s
		digest.processInt(clientPublicKey);	// A
		digest.processInt(serverPublicKey);	// B
		digest.process(sessionKey);			// K

		ScratchBird::BigInteger rc;
		digest.getInt(rc);
		return rc;
	}
};



#if SRP_DEBUG > 0
void dumpIt(const char* name, const ScratchBird::BigInteger& bi);
void dumpIt(const char* name, const ScratchBird::UCharBuffer& data);
void dumpIt(const char* name, const ScratchBird::string& str);
void dumpBin(const char* name, const ScratchBird::string& str);
#else
void static inline dumpIt(const char* /*name*/, const ScratchBird::BigInteger& /*bi*/) { }
void static inline dumpIt(const char* /*name*/, const ScratchBird::UCharBuffer& /*data*/) { }
void static inline dumpIt(const char* /*name*/, const ScratchBird::string& /*str*/) { }
void static inline dumpBin(const char* /*name*/, const ScratchBird::string& /*str*/) { }
#endif

void checkStatusVectorForMissingTable(const ISC_STATUS* v, std::function<void ()> cleanup = nullptr);

} // namespace Auth

#endif // AUTH_SRP_SRP_H
