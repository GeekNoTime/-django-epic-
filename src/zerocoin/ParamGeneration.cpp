/// \file       ParamGeneration.cpp
///
/// \brief      Parameter manipulation routines for the Zerocoin cryptographic
///             components.
///
/// \author     Ian Miers, Christina Garman and Matthew Green
/// \date       June 2013
///
/// \copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
/// \license    This project is released under the MIT license.

#include <string>
#include "Zerocoin.h"

using namespace std;

namespace libzerocoin {

/// \brief Fill in a set of Zerocoin parameters from a modulus "N".
/// \param N                A trusted RSA modulus
/// \param aux              An optional auxiliary string used in derivation
/// \param securityLevel    A security level
///
/// \throws         ZerocoinException if the process fails
///
/// Fills in a ZC_Params data structure deterministically from
/// a trustworthy RSA modulus "N", which is provided as a Bignum.
///
/// Note: this routine makes the fundamental assumption that "N"
/// encodes a valid RSA-style modulus of the form "e1*e2" for some
/// unknown safe primes "e1" and "e2". These factors must not
/// be known to any party, or the security of Zerocoin is
/// compromised. The integer "N" must be a MINIMUM of 1023
/// in length, and 3072 bits is strongly recommended.
///

void
CalculateParams(Params &params, Bignum N, string aux, uint32_t securityLevel)
{
	params.initialized = false;
	params.accumulatorParams.initialized = false;

	// Verify that |N| is > 1023 bits.
	uint32_t NLen = N.bitSize();
	if (NLen < 1023) {
		throw ZerocoinException("Modulus must be at least 1023 bits");
	}

	// Verify that "securityLevel" is  at least 80 bits (minimum).
	if (securityLevel < 80) {
		throw ZerocoinException("Security level must be at least 80 bits.");
	}

	// Set the accumulator modulus to "N".
	params.accumulatorParams.accumulatorModulus = N;

	// Calculate the required size of the field "F_p" into which
	// we're embedding the coin commitment group. This may throw an
	// exception if the securityLevel is too large to be supported
	// by the current modulus.
	uint32_t pLen = 0;
	uint32_t qLen = 0;
	calculateGroupParamLengths(NLen - 2, securityLevel, &pLen, &qLen);

	// Calculate candidate parameters ("p", "q") for the coin commitment group
	// using a deterministic process based on "N", the "aux" string, and
	// the dedicated string "COMMITMENTGROUP".
	params.coinCommitmentGroup = deriveIntegerGroupParams(calculateSeed(N, aux, securityLevel, STRING_COMMIT_GROUP),
	                             pLen, qLen);

	// Next, we derive parameters for a second Accumulated Value commitment group.
	// This is a Schnorr group with the specific property that the order of the group
	// must be exactly equal to "q" from the commitment group. We set
	// the modulus of the new group equal to "2q+1" and test to see if this is prime.
	params.serialNumberSoKCommitmentGroup = deriveIntegerGroupFromOrder(params.coinCommitmentGroup.modulus);

	// Calculate the parameters for the internal commitment
	// using the same process.
	params.accumulatorParams.accumulatorPoKCommitmentGroup = deriveIntegerGroupParams(calculateSeed(N, aux, securityLevel, STRING_AIC_GROUP),
	        qLen + 300, qLen + 1);

	// Calculate the parameters for the accumulator QRN commitment generators. This isn't really
	// a whole group, just a pair of random generators in QR_N.
	uint32_t resultCtr;
	params.accumulatorParams.accumulatorQRNCommitmentGroup.g = generateIntegerFromSeed(NLen - 1,
	        calculateSeed(N, aux, securityLevel, STRING_QRNCOMMIT_GROUPG),
	        &resultCtr).pow_mod(Bignum(2), N);
	params.accumulatorParams.accumulatorQRNCommitmentGroup.h = generateIntegerFromSeed(NLen - 1,
	        calculateSeed(N, aux, securityLevel, STRING_QRNCOMMIT_GROUPG),
	        &resultCtr).pow_mod(Bignum(2), N);

	// Calculate the accumulator base, which we calculate as "u = C**2 mod N"
	// where C is an arbitrary value. In the unlikely case that "u = 1" we increment
	// "C" and repeat.
	Bignum constant(ACCUMULATOR_BASE_CONSTANT);
	params.accumulatorParams.accumulatorBase = Bignum(1);
	for (uint32_t count = 0; count < MAX_ACCUMGEN_ATTEMPTS && params.accumulatorParams.accumulatorBase.isOne(); count++) {
		params.accumulatorParams.accumulatorBase = constant.pow_mod(Bignum(2), params.accumulatorParams.accumulatorModulus);
	}

	// Compute the accumulator range. The upper range is the largest possible coin commitment value.
	// The lower range is sqrt(upper range) + 1. Since OpenSSL doesn't have
	// a square root function we use a slightly higher approximation.
	params.accumulatorParams.maxCoinValue = params.coinCommitmentGroup.modulus;
	params.accumulatorParams.minCoinValue = Bignum(2).pow((params.coinCommitmentGroup.modulus.bitSize() / 2) + 3);

	// If all went well, mark params as successfully initialized.
	params.accumulatorParams.initialized = true;

	// If all went well, mark params as successfully initialized.
	params.initialized = true;
}

/// \brief Format a seed string by hashing several values.
/// \param N                A Bignum
/// \param aux              An auxiliary string
/// \param securityLevel    The security level in bits
/// \param groupName        A group description string
/// \throws         ZerocoinException if the process fails
///
/// Returns the hash of the value.

uint256
calculateGeneratorSeed(uint256 seed, uint256 pSeed, uint256 qSeed, string label, uint32_t index, uint32_t count)
{
	CHashWriter hasher(0,0);
	uint256     hash;

	// Compute the hash of:
	// <modulus>||<securitylevel>||<auxString>||groupName
	hasher << seed;
	hasher << string("||");
	hasher << pSeed;
	hasher << string("||");
	hasher << qSeed;
	hasher << string("||");
	hasher << label;
	hasher << string("||");
	hasher << index;
	hasher << string("||");
	hasher << count;

	return hasher.GetHash();
}

/// \brief Format a seed string by hashing several values.
/// \param N                A Bignum
/// \param aux              An auxiliary string
/// \param securityLevel    The security level in bits
/// \param groupName        A group description string
/// \throws         ZerocoinException if the process fails
///
/// Returns the hash of the value.

uint256
calculateSeed(Bignum modulus, string auxString, uint32_t securityLevel, string groupName)
{
	CHashWriter hasher(0,0);
	uint256     hash;

	// Compute the hash of:
	// <modulus>||<securitylevel>||<auxString>||groupName
	hasher << modulus;
	hasher << string("||");
	hasher << securityLevel;
	hasher << string("||");
	hasher << auxString;
	hasher << string("||");
	hasher << groupName;

	return hasher.GetHash();
}

uint256
calculateHash(uint256 input)
{
	CHashWriter hasher(0,0);

	// Compute the hash of "input"
	hasher << input;

	return hasher.GetHash();
}

/// \brief Calculate field/group parameter sizes based on a security level.
/// \param maxPLen          Maximum size of the field (modulus "p") in bits.
/// \param securityLevel    Required security level in bits (at least 80)
/// \param pLen             Result: length of "p" in bits
/// \param qLen             Result: length of "q" in bits
/// \throws                 ZerocoinException if the process fails
///
/// Calculates the appropriate sizes of "p" and "q" for a prime-order
/// subgroup of order "q" embedded within a field "F_p". The sizes
/// are based on a 'securityLevel' provided in symmetric-equivalent
/// bits. Our choices slightly exceed the specs in FIPS 186-3:
///
/// securityLevel = 80:     pLen = 1024, qLen = 256
/// securityLevel = 112:    pLen = 2048, qLen = 256
/// securityLevel = 128:    qLen = 3072, qLen = 320
///
/// If the length of "p" exceeds the length provided in "maxPLen", or
/// if "securityLevel < 80" this routine throws an exception.

void
calculateGroupParamLengths(uint32_t maxPLen, uint32_t securityLevel,
                           uint32_t *pLen, uint32_t *qLen)
{
	*pLen = *qLen = 0;

	if (securityLevel < 80) {
		throw ZerocoinException("Security level must be at least 80 bits.");
	} else if (securityLevel == 80) {
		*qLen = 256;
		*pLen = 1024;
	} else if (securityLevel <= 112) {
		*qLen = 256;
		*pLen = 2048;
	} else if (securityLevel <= 128) {
		*qLen = 320;
		*pLen = 3072;
	} else {
		throw ZerocoinException("Security level not supported.");
	}

	if (*pLen > maxPLen) {
		throw ZerocoinException("Modulus size is too small for this security level.");
	}
}

/// \brief Deterministically compute a set of group parameters using NIST procedures.
/// \param seedStr  A byte string seeding the process.
/// \param pLen     The desired length of the modulus "p" in bits
/// \param qLen     The desired length of the order "q" in bits
/// \return         An IntegerGroupParams object
///
/// Calculates the description of a group G of prime order "q" embedded within
/// a field "F_p". The input to this routine is in arbitrary seed. It uses the
/// algorithms described in FIPS 186-3 Appendix A.1.2 to calculate
/// primes "p" and "q". It uses the procedure in Appendix A.2.3 to
/// derive two generators "g", "h".

IntegerGroupParams
deriveIntegerGroupParams(uint256 seed, uint32_t pLen, uint32_t qLen)
{
	IntegerGroupParams result;
	Bignum p;
	Bignum q;
	uint256 pSeed, qSeed;

	// Calculate "p" and "q" and "domain_param