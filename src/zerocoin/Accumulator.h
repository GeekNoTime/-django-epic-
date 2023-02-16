/**
 * @file       Accumulator.h
 *
 * @brief      Accumulator and AccumulatorWitness classes for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
#ifndef ACCUMULATOR_H_
#define ACCUMULATOR_H_

namespace libzerocoin {
/**
 * \brief Implementation of the RSA-based accumulator.
 **/

class Accumulator {
public:

	/**
	 * @brief      Construct an Accumulator from a stream.
	 * @param p    An AccumulatorAndProofParams object containing global parameters
	 * @param d    the denomination of coins we are accumulating
	 * @throw      Zerocoin exception in case of invalid parameters
	 **/
	template<typename Stream>
	Accumulator(const AccumulatorAndProofParams* p, Stream& strm): params(p) {
		strm >> *this;
	}

	template<typename Stream>
	Accumulator(const Params* p, Stream& strm) {
		strm >> *this;
		this->params = &(p->accumulatorParams);
	}

	/**
	 * @brief      Construct an Accumulator from a Params object.
	 * @param p    A Params object containing global parameters
	 * @param d the denomination of coins we are accumulating
	 * @throw     Zerocoin exception in case of invalid parameters
	 **/
	Accumulator(const AccumulatorAndProofParams* p, const CoinDenomination d = ZQ_PEDERSEN);

	Accumulator(const Params* p, const CoinDenomination d = ZQ_PEDERSEN);

	/**
	 * Accumulate a coin into the accumulator. Validates
	 * the coin prior to accumulation.
	 *
	 * @param coin	A PublicCoin to accumulate.
	 *
	 * @throw		Zerocoin exception if the coin is not valid.
	 *
	 **/
	void accumulate(const PublicCoin &coin);

	const CoinDenomination getDenomination() const;
	/** Get the accumulator result
	 *
	 * @return a Bignum containing the result.
	 */
	const Bignum& getValue() const;


	// /**
	//  * Used to set the accumulator value
	//  *
	//  * Use this to handle accumulator checkpoints
	//  * @param b the value to set the accumulator to.
	//  * @throw  A ZerocoinException if the accumulator value is invalid.
	//  */
	// void setValue(Bignum &b); // shouldn't this be a constructor?

	/** Us