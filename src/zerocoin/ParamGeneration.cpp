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
	        &resultCtr).pow_mod(Bignu