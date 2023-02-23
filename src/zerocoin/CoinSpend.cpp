/**
 * @file       CoinSpend.cpp
 *
 * @brief      CoinSpend class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/

#include "Zerocoin.h"

namespace libzerocoin {

CoinSpend::CoinSpend(const Params* p, const PrivateCoin& coin,
                     Accumulator& a, const AccumulatorWitness& witness, const SpendMetaData& m):
	params(p),
	denomination(coin.getPublicCoin().getDenomination()),
	coinSerialNumber((coin.getSerialNumber())),
	accumulatorPoK(&p->accumulatorParams),
	serialNumberSoK(p),
	commitmentPoK(&p->serialNumberSoKCommitmentGroup, &p->accumulatorParams.accumulatorPoKCommitmentGroup) {

	// Sanity check: let's verify that the Witness is valid with respect to
	// the coin and Accumulator provided.
	if (!(witness.VerifyWitness(a, coin.getPublicCoin()))) {
		thr