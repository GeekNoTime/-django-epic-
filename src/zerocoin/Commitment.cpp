/**
 * @file       Commitment.cpp
 *
 * @brief      Commitment and CommitmentProof classes for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/

#include <stdlib.h>
#include "Zerocoin.h"

namespace libzerocoin {

//Commitment class
Commitment::Commitment::Commitment(const IntegerGroupParams* p,
                                   const Bignum& value): params(p), contents(value) {
	this->randomness = Bignum::randBignum(params->groupOrder);
	this->commitmentValue = (params->g.pow_mod(this->contents, params->modulus).mul_mod(
	                         params->h.pow_mod(this->randomness, params->modulus), params->modulus));
}

const Bignum& Commitment::getCommitmentValue() const {
	return this->commitmentValue;
}

const Bignum& Commitment::getRandomness() const {
	return this->randomness;
}

const Bignum& Commitment::getContents() const {
	return this->contents;
}

//CommitmentProofOfKnowledge class
CommitmentProofOfKnowledge::CommitmentProofOfKnowledge(const IntegerGroupParams* ap, const IntegerGroupParams* bp): ap(ap), bp(bp) {}

// TODO: get parameters from the commitment group
CommitmentProofOfKnowledge::CommitmentProofOfKnowledge(const IntegerGroupParams* aParams,
        const IntegerGroupParams* bParams, const Commitment& a, const Commitment& b):
	ap(aParams),bp(bParams)
{
	Bignum r1, r2, r3;

	// First: make sure that the two commitments have the
	// same contents.
	if (a.getContents() != b.getContents()) {
		throw std::invalid_argument("Both commitments must contain the same value");
	}

	// Select three random values "r1, r2, r3" in the range 0 to (2^l)-1 where l is:
	// length of challenge value + max(modulus 1, modulus 2, order 1, order 2) + margin.
	// We set "margin" to be a relatively generous  security parameter.
	//
	// We choose these large values to ensure statistical zero knowledge.
	uint32_t randomSize = COMMITMENT_EQUALITY_CHALLENGE_SIZE + COMMITMENT_EQUALITY_SECMARGIN +
	                      std::max(st