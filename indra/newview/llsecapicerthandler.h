/**
 * @file llsecapicerthandler.h
 * @brief Security API for certificate handling
 * @author Cinder Roxley <cinder@sdf.org>
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010-2019, Linden Research, Inc.
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

#ifndef LLSECHAPICERTHANDLER_H
#define LLSECHAPICERTHANDLER_H

#include "llexception.h"
#include "llsdutil.h"
#include <openssl/x509.h>

#define CERT_SUBJECT_NAME "subject_name"
#define CERT_ISSUER_NAME "issuer_name"
#define CERT_NAME_CN "commonName"

#define CERT_SUBJECT_NAME_STRING "subject_name_string"
#define CERT_ISSUER_NAME_STRING "issuer_name_string"

#define CERT_SERIAL_NUMBER "serial_number"

#define CERT_VALID_FROM "valid_from"
#define CERT_VALID_TO "valid_to"
#define CERT_SHA1_DIGEST "sha1_digest"
#define CERT_MD5_DIGEST "md5_digest"
#define CERT_HOSTNAME "hostname"
#define CERT_BASIC_CONSTRAINTS "basicConstraints"
#define CERT_BASIC_CONSTRAINTS_CA "CA"
#define CERT_BASIC_CONSTRAINTS_PATHLEN "pathLen"

#define CERT_KEY_USAGE "keyUsage"
#define CERT_KU_DIGITAL_SIGNATURE    "digitalSignature"
#define CERT_KU_NON_REPUDIATION      "nonRepudiation"
#define CERT_KU_KEY_ENCIPHERMENT     "keyEncipherment"
#define CERT_KU_DATA_ENCIPHERMENT    "dataEncipherment"
#define CERT_KU_KEY_AGREEMENT        "keyAgreement"
#define CERT_KU_CERT_SIGN        "certSigning"
#define CERT_KU_CRL_SIGN             "crlSigning"
#define CERT_KU_ENCIPHER_ONLY        "encipherOnly"
#define CERT_KU_DECIPHER_ONLY        "decipherOnly"

#define CERT_VALIDATION_DATE "validation_date"

#define CERT_EXTENDED_KEY_USAGE "extendedKeyUsage"
#define CERT_EKU_SERVER_AUTH SN_server_auth

#define CERT_SUBJECT_KEY_IDENTFIER "subjectKeyIdentifier"
#define CERT_AUTHORITY_KEY_IDENTIFIER "authorityKeyIdentifier"
#define CERT_AUTHORITY_KEY_IDENTIFIER_ID "authorityKeyIdentifierId"
#define CERT_AUTHORITY_KEY_IDENTIFIER_NAME "authorityKeyIdentifierName"
#define CERT_AUTHORITY_KEY_IDENTIFIER_SERIAL "authorityKeyIdentifierSerial"

// validate the current time lies within
// the validation period of the cert
#define VALIDATION_POLICY_TIME 1

// validate that the CA, or some cert in the chain
// lies within the certificate store
#define VALIDATION_POLICY_TRUSTED 2

// validate that the subject name of
// the cert contains the passed in hostname
// or validates against the hostname
#define VALIDATION_POLICY_HOSTNAME 4


// validate that the cert contains the SSL EKU
#define VALIDATION_POLICY_SSL_KU 8

// validate that the cert contains the SSL EKU
#define VALIDATION_POLICY_CA_KU 16

#define VALIDATION_POLICY_CA_BASIC_CONSTRAINTS 32

// validate that the cert is correct for SSL
#define VALIDATION_POLICY_SSL (VALIDATION_POLICY_TIME | \
VALIDATION_POLICY_HOSTNAME | \
VALIDATION_POLICY_TRUSTED | \
VALIDATION_POLICY_SSL_KU | \
VALIDATION_POLICY_CA_BASIC_CONSTRAINTS | \
VALIDATION_POLICY_CA_KU)

bool valueCompareLLSD(const LLSD& lhs, const LLSD& rhs);

// class LLCertificate
// parent class providing an interface for certifiate.
// LLCertificates are considered unmodifiable
// Certificates are pulled out of stores, or created via
// factory calls
class LLCertificate : public LLThreadSafeRefCount
{
    LOG_CLASS(LLCertificate);
public:
    LLCertificate() {}
    
    virtual ~LLCertificate() {}
    
    // return a PEM encoded certificate.  The encoding
    // includes the -----BEGIN CERTIFICATE----- and end certificate elements
    virtual std::string getPem() const=0;
    
    // return a DER encoded certificate
    virtual std::vector<U8> getBinary() const=0;
    
    // return an LLSD object containing information about the certificate
    // such as its name, signature, expiry time, serial number
    virtual void getLLSD(LLSD& llsd)=0;
    
    // return an openSSL X509 struct for the certificate
    virtual X509* getOpenSSLX509() const=0;
    
};

// class LLCertificateVector
// base class for a list of certificates.


class LLCertificateVector : public LLThreadSafeRefCount
{
    
public:
    
    LLCertificateVector() {};
    virtual ~LLCertificateVector() {};
    
    // base iterator implementation class, providing
    // the functionality needed for the iterator class.
    class iterator_impl : public LLThreadSafeRefCount
    {
    public:
        iterator_impl() {};
        virtual ~iterator_impl() {};
        virtual void seek(bool incr)=0;
        virtual LLPointer<iterator_impl> clone() const=0;
        virtual bool equals(const LLPointer<iterator_impl>& _iter) const=0;
        virtual LLPointer<LLCertificate> get()=0;
    };
    
    // iterator class
    class iterator
    {
    public:
        iterator(LLPointer<iterator_impl> impl) : mImpl(impl) {}
        iterator() : mImpl(NULL) {}
        iterator(const iterator& _iter) {mImpl = _iter.mImpl->clone(); }
        ~iterator() {}
        iterator& operator++() { if(mImpl.notNull()) mImpl->seek(true); return *this;}
        iterator& operator--() { if(mImpl.notNull()) mImpl->seek(false); return *this;}
        
        iterator operator++(int) { iterator result = *this; if(mImpl.notNull()) mImpl->seek(true); return result;}
        iterator operator--(int) { iterator result = *this; if(mImpl.notNull()) mImpl->seek(false); return result;}
        LLPointer<LLCertificate> operator*() { return mImpl->get(); }
        
        LLPointer<iterator_impl> mImpl;
    protected:
        friend bool operator==(const LLCertificateVector::iterator& _lhs, const LLCertificateVector::iterator& _rhs);
        bool equals(const iterator& _iter) const { return mImpl->equals(_iter.mImpl); }
    };
    
    // numeric indexer
    virtual LLPointer<LLCertificate> operator[](int)=0;
    
    // Iteration
    virtual iterator begin()=0;
    
    virtual iterator end()=0;
    
    // find a cert given params
    virtual iterator find(const LLSD& params) =0;
    
    // return the number of certs in the store
    virtual int size() const = 0;
    
    // append the cert to the store.  if a copy of the cert already exists in the store, it is removed first
    virtual void  add(LLPointer<LLCertificate> cert)=0;
    
    // insert the cert to the store.  if a copy of the cert already exists in the store, it is removed first
    virtual void  insert(iterator location, LLPointer<LLCertificate> cert)=0;
    
    // remove a certificate from the store
    virtual LLPointer<LLCertificate> erase(iterator cert)=0;
};

// class LLCertificateChain
// Class representing a chain of certificates in order, with the
// first element being the child cert.
class LLCertificateChain : virtual public LLCertificateVector
{
    
public:
    LLCertificateChain() {}
    
    virtual ~LLCertificateChain() {}
    
};

// class LLCertificateStore
// represents a store of certificates, typically a store of root CA
// certificates.  The store can be persisted, and can be used to validate
// a cert chain
//
class LLCertificateStore : virtual public LLCertificateVector
{
    
public:
    
    LLCertificateStore() {}
    virtual ~LLCertificateStore() {}
    
    // persist the store
    virtual void save()=0;
    
    // return the store id
    virtual std::string storeId() const=0;
    
    // validate a certificate chain given the params.
    // Will throw exceptions on error
    
    virtual void validate(int validation_policy,
                          LLPointer<LLCertificateChain> cert_chain,
                          const LLSD& validation_params) =0;
    
};


inline
bool operator==(const LLCertificateVector::iterator& _lhs, const LLCertificateVector::iterator& _rhs)
{
    return _lhs.equals(_rhs);
}
inline
bool operator!=(const LLCertificateVector::iterator& _lhs, const LLCertificateVector::iterator& _rhs)
{
    return !(_lhs == _rhs);
}

// helpers
extern LLSD cert_name_from_X509_NAME(X509_NAME* name);
extern std::string cert_string_name_from_X509_NAME(X509_NAME* name);
extern std::string cert_string_from_asn1_integer(ASN1_INTEGER* value);
extern LLDate cert_date_from_asn1_time(ASN1_TIME* asn1_time);
extern std::string cert_get_digest(const std::string& digest_type, X509 *cert);


// class LLCertificate
//
class LLBasicCertificate : public LLCertificate
{
public:
    LOG_CLASS(LLBasicCertificate);
    
    // The optional validation_params allow us to make the unit test time-invariant
    LLBasicCertificate(const std::string& pem_cert, const LLSD* validation_params = NULL);
    LLBasicCertificate(X509* openSSLX509, const LLSD* validation_params = NULL);
    
    virtual ~LLBasicCertificate();
    
    std::string getPem() const override;
    std::vector<U8> getBinary() const override;
    void getLLSD(LLSD &llsd) override;
    
    X509* getOpenSSLX509() const override;
    
    // set llsd elements for testing
    void setLLSD(const std::string name, const LLSD& value) { mLLSDInfo[name] = value; }
    
protected:
    // certificates are stored as X509 objects, as validation and
    // other functionality is via openssl
    X509* mCert;
    
    LLSD& _initLLSD();
    LLSD mLLSDInfo;
};


// class LLBasicCertificateVector
// Class representing a list of certificates
// This implementation uses a stl vector of certificates.
class LLBasicCertificateVector : virtual public LLCertificateVector
{
    
public:
    LLBasicCertificateVector() {}
    
    virtual ~LLBasicCertificateVector() {}
    
    // Implementation of the basic iterator implementation.
    // The implementation uses a vector iterator derived from
    // the vector in the LLBasicCertificateVector class
    class BasicIteratorImpl : public iterator_impl
    {
    public:
        BasicIteratorImpl(std::vector<LLPointer<LLCertificate> >::iterator _iter) { mIter = _iter;}
        virtual ~BasicIteratorImpl() {};
        // seek forward or back.  Used by the operator++/operator-- implementations
        void seek(bool incr) override
        {
            if(incr)
            {
                ++mIter;
            }
            else
            {
                --mIter;
            }
        }
        // create a copy of the iterator implementation class, used by the iterator copy constructor
        LLPointer<iterator_impl> clone() const override
        {
            return new BasicIteratorImpl(mIter);
        }
        
        bool equals(const LLPointer<iterator_impl>& _iter) const override
        {
            const BasicIteratorImpl *rhs_iter = dynamic_cast<const BasicIteratorImpl *>(_iter.get());
            llassert(rhs_iter);
            if (!rhs_iter) return false;
            return (mIter == rhs_iter->mIter);
        }
        
        LLPointer<LLCertificate> get() override
        {
            return *mIter;
        }
    protected:
        friend class LLBasicCertificateVector;
        std::vector<LLPointer<LLCertificate> >::iterator mIter;
    };
    
    // numeric index of the vector
    LLPointer<LLCertificate> operator[](int _index) override { return mCerts[_index];}
    
    // Iteration
    iterator begin() override { return iterator(new BasicIteratorImpl(mCerts.begin())); }
    
    iterator end() override {  return iterator(new BasicIteratorImpl(mCerts.end())); }
    
    // find a cert given params
    iterator find(const LLSD& params) override;
    
    // return the number of certs in the store
    int size() const override { return mCerts.size(); }
    
    // insert the cert to the store.  if a copy of the cert already exists in the store, it is removed first
    void  add(LLPointer<LLCertificate> cert) override { insert(end(), cert); }
    
    // insert the cert to the store.  if a copy of the cert already exists in the store, it is removed first
    void  insert(iterator _iter, LLPointer<LLCertificate> cert) override;
    
    // remove a certificate from the store
    LLPointer<LLCertificate> erase(iterator _iter) override;
    
protected:
    std::vector<LLPointer<LLCertificate> >mCerts;
};

// class LLCertificateStore
// represents a store of certificates, typically a store of root CA
// certificates.  The store can be persisted, and can be used to validate
// a cert chain
//
class LLBasicCertificateStore : virtual public LLBasicCertificateVector, public LLCertificateStore
{
public:
    LLBasicCertificateStore(const std::string& filename);
    void load_from_file(const std::string& filename);
    
    virtual ~LLBasicCertificateStore();
    
    // persist the store
    void save() override;
    
    // return the store id
    std::string storeId() const override;
    
    // validate a certificate chain against a certificate store, using the
    // given validation policy.
    void validate(int validation_policy,
                  LLPointer<LLCertificateChain> ca_chain,
                  const LLSD& validation_params) override;
    
protected:
    std::vector<LLPointer<LLCertificate> >            mCerts;
    
    // cache of cert sha1 hashes to from/to date pairs, to improve
    // performance of cert trust.  Note, these are not the CA certs,
    // but the certs that have been validated against this store.
    typedef std::map<std::string, std::pair<LLDate, LLDate> > t_cert_cache;
    t_cert_cache mTrustedCertCache;
    
    std::string mFilename;
};

// class LLCertificateChain
// Class representing a chain of certificates in order, with the
// first element being the child cert.
class LLBasicCertificateChain : virtual public LLBasicCertificateVector, public LLCertificateChain
{
    
public:
    LLBasicCertificateChain(X509_STORE_CTX * store);
    
    virtual ~LLBasicCertificateChain() {}
    
};

// All error handling is via exceptions.
class LLCertException: public LLException
{
public:
    LLCertException(const LLSD& cert_data, const std::string& msg): LLException(msg),
    mCertData(cert_data)
    {
        LL_WARNS("SECAPI") << "Certificate Error: " << msg;
        LL_CONT << " in certificate: \n";
        LL_CONT << ll_pretty_print_sd(cert_data) << LL_ENDL;
    }
    virtual ~LLCertException() throw() {}
    LLSD getCertData() const { return mCertData; }
protected:
    LLSD mCertData;
};

class LLInvalidCertificate : public LLCertException
{
public:
    LLInvalidCertificate(const LLSD& cert_data) : LLCertException(cert_data, "CertInvalid")
    {
    }
    virtual ~LLInvalidCertificate() throw() {}
protected:
};

class LLCertValidationTrustException : public LLCertException
{
public:
    LLCertValidationTrustException(const LLSD& cert_data) : LLCertException(cert_data, "CertUntrusted")
    {
    }
    virtual ~LLCertValidationTrustException() throw() {}
protected:
};

class LLCertValidationHostnameException : public LLCertException
{
public:
    LLCertValidationHostnameException(std::string hostname,
                                      const LLSD& cert_data) : LLCertException(cert_data, "CertInvalidHostname")
    {
        mHostname = hostname;
    }
    virtual ~LLCertValidationHostnameException() throw() {}
    std::string getHostname() { return mHostname; }
protected:
    std::string mHostname;
};

class LLCertValidationExpirationException : public LLCertException
{
public:
    LLCertValidationExpirationException(const LLSD& cert_data,
                                        LLDate current_time) : LLCertException(cert_data, "CertExpired")
    {
        mTime = current_time;
    }
    virtual ~LLCertValidationExpirationException() throw() {}
    LLDate GetTime() { return mTime; }
protected:
    LLDate mTime;
};

class LLCertKeyUsageValidationException : public LLCertException
{
public:
    LLCertKeyUsageValidationException(const LLSD& cert_data) : LLCertException(cert_data, "CertKeyUsage")
    {
    }
    virtual ~LLCertKeyUsageValidationException() throw() {}
protected:
};

class LLCertBasicConstraintsValidationException : public LLCertException
{
public:
    LLCertBasicConstraintsValidationException(const LLSD& cert_data) : LLCertException(cert_data, "CertBasicConstraints")
    {
    }
    virtual ~LLCertBasicConstraintsValidationException() throw() {}
protected:
};

class LLCertValidationInvalidSignatureException : public LLCertException
{
public:
    LLCertValidationInvalidSignatureException(const LLSD& cert_data) : LLCertException(cert_data, "CertInvalidSignature")
    {
    }
    virtual ~LLCertValidationInvalidSignatureException() throw() {}
protected:
};

/**
 * LLSecAPICertHandler Class
 * Handler class for certificate handlers.
 */

class LLSecAPICertHandler : public LLThreadSafeRefCount
{
public:
    LLSecAPICertHandler() {}
    virtual ~LLSecAPICertHandler() {}
    
    // instantiate a certificate from a pem string
    virtual LLPointer<LLCertificate> getCertificate(const std::string& pem_cert) = 0;
    
    // instiate a certificate from an openssl X509 structure
    virtual LLPointer<LLCertificate> getCertificate(X509* openssl_cert) = 0;
    
    // instantiate a chain from an X509_STORE_CTX
    virtual LLPointer<LLCertificateChain> getCertificateChain(X509_STORE_CTX* chain) = 0;
    
    // instantiate a cert store given it's id.  if a persisted version
    // exists, it'll be loaded.  If not, one will be created (but not
    // persisted)
    virtual LLPointer<LLCertificateStore> getCertificateStore(const std::string& store_id) = 0;
};

class LLSecAPIBasicCertHandler : public LLSecAPICertHandler
{
public:
    LLSecAPIBasicCertHandler();
    virtual ~LLSecAPIBasicCertHandler() {}
    
    // instantiate a certificate from a pem string
    LLPointer<LLCertificate> getCertificate(const std::string& pem_cert) final;
    
    // instiate a certificate from an openssl X509 structure
    LLPointer<LLCertificate> getCertificate(X509* openssl_cert) final;
    
    // instantiate a chain from an X509_STORE_CTX
    LLPointer<LLCertificateChain> getCertificateChain(X509_STORE_CTX* chain) final;
    
    // instantiate a cert store given it's id.  if a persisted version
    // exists, it'll be loaded.  If not, one will be created (but not
    // persisted)
    LLPointer<LLCertificateStore> getCertificateStore(const std::string& store_id) final;
    
protected:
    LLPointer<LLBasicCertificateStore> mStore;
};

extern LLPointer<LLSecAPICertHandler> gSecAPICertHandler;

#endif // LLSECHAPICERTHANDLER_H
